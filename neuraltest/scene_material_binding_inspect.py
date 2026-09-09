"""Bind selected source draws to captured material identities, not PBR materials."""
from transform_store_inspect import require
from material_inspect import decode,fnv
import re
import struct
import math


def sample_linear_repeat(level,uv):
    """Explicit mip-level reference only; does not infer raster derivatives/LOD."""
    w,h=level['width'],level['height'];data=level['rgba']
    require(type(w) is int and type(h) is int and 0<w<=4096 and 0<h<=4096
            and len(data)==w*h*4,'sample dimensions')
    require(len(uv)==2 and all(math.isfinite(v) for v in uv),'sample UV')
    x=(uv[0]%1)*w-.5;y=(uv[1]%1)*h-.5
    ix,iy=math.floor(x),math.floor(y);fx,fy=x-ix,y-iy
    result=[0.]*4
    for dx,dy,weight in ((0,0,(1-fx)*(1-fy)),(1,0,fx*(1-fy)),(0,1,(1-fx)*fy),(1,1,fx*fy)):
        offset=(((iy+dy)%h)*w+(ix+dx)%w)*4
        for c in range(4):result[c]+=data[offset+c]/255*weight
    return result


def effective_globals(scene,materials,draw):
    for key in ('frame_id','game_id','git_sha'):
        require(scene[key]==materials[key],'global identity '+key)
    require(materials['scene_sha']==scene['git_sha'],'global scene SHA')
    g=materials.get('shader_globals')
    require(isinstance(g,dict) and g.get('provenance')=='native-pixel-constant-upload'
            and g.get('cpu_snapshot_source_bytes_unchanged') is True,'global provenance')
    require(type(g.get('fog_enabled')) is bool,'fog enable type')
    for key,size in [('fog_color_vertex',3),('fog_color_ram',3),('clamp_min',4),('clamp_max',4)]:
        values=g.get(key)
        require(isinstance(values,list) and len(values)==size
                and all(type(v) in (int,float) and math.isfinite(v) for v in values),'global '+key)
    # PixelFmt=4 is PVR bump mapping; other unsupported states remain checked
    # by selected_source_color. Do not infer bump state from decoded image format.
    tcw=draw['tcw'];require(type(tcw) is int and 0<=tcw<=0xffffffff,'global TCW')
    return dict(g,bump_mapping=((tcw>>27)&7)==4)


def selected_source_color(vertex,texture,offset,state,globals):
    """Selected non-bump mode only; not a sampler, PBR shader or fog inference."""
    require(state['shading']==3 and state['offset']==1 and state['fog_control']==1
            and state['use_alpha']==0 and state['ignore_texture_alpha']==1,'unsupported source shading state')
    require(globals is not None and type(globals.get('fog_enabled')) is bool
            and 'fog_color_vertex' in globals,'missing effective fog state')
    require(globals.get('bump_mapping') is False,'unknown/unsupported bump state')
    require(state['color_clamp']==0 or (globals.get('clamp_min')==[0,0,0,0]
            and globals.get('clamp_max')==[1,1,1,1]),'active color clamp unsupported')
    fog=globals['fog_color_vertex']
    require(len(fog)==3 and all(math.isfinite(v) and 0<=v<=1 for v in fog),'fog color')
    require(all(len(v)==4 and all(math.isfinite(x) and 0<=x<=1 for x in v)
                for v in (vertex,texture,offset)),'normalized shading inputs')
    # UseAlpha=0 and IgnoreTexA=1 force both input alpha terms to one.
    rgb=[vertex[i]*texture[i]+offset[i] for i in range(3)]
    if globals['fog_enabled']:
        rgb=[rgb[i]*(1-offset[3])+fog[i]*offset[3] for i in range(3)]
    return [*rgb,1.0]


def draw_state(draw):
    # TSP/PCW definitions: core/hw/pvr/ta_structs.h. Raw requested state;
    # global overrides, fog constants and renderer-specific effects stay separate.
    tsp=draw['tsp'];pcw=draw['pcw']
    require(all(type(v) is int and 0<=v<=0xffffffff for v in (tsp,pcw)),'state words')
    def bits(start,width=1):return (tsp>>start)&((1<<width)-1)
    return dict(shading=bits(6,2),mipmap_d=bits(8,4),filter_mode=bits(13,2),
                clamp_v=bits(15),clamp_u=bits(16),flip_v=bits(17),flip_u=bits(18),
                ignore_texture_alpha=bits(19),use_alpha=bits(20),color_clamp=bits(21),
                fog_control=bits(22,2),offset=(pcw>>2)&1,texture=(pcw>>3)&1,
                gouraud=(pcw>>1)&1,global_overrides_applied=False)


def vertex_attributes(scene,indices):
    expected=['x_bits','y_bits','z_bits','u_bits','v_bits','color_rgba_bytes','offset_rgba_bytes',
              'u1_bits','v1_bits','color1_rgba_bytes','offset1_rgba_bytes']
    require(scene['vertex_layout']==expected,'vertex layout')
    require(0<len(indices)<=65536 and len(set(indices))==len(indices),'vertex selection')
    def number(word):
        require(type(word) is int and 0<=word<=0xffffffff,'float word')
        value=struct.unpack('<f',struct.pack('<I',word))[0]
        require(math.isfinite(value),'nonfinite UV')
        return value
    def color(raw):
        require(len(raw)==4 and all(type(v) is int and 0<=v<=255 for v in raw),'color bytes')
        # Export labels are historical; native DX11 binds these bytes as BGRA8.
        return [raw[2],raw[1],raw[0],raw[3]]
    result=[]
    for index in indices:
        require(type(index) is int and 0<=index<len(scene['vertices']),'vertex identity')
        v=scene['vertices'][index];require(len(v)==11,'vertex size')
        result.append(dict(source_vertex=index,uv=[number(v[3]),number(v[4])],
                           uv1=[number(v[7]),number(v[8])],color_rgba=color(v[5]),
                           offset_rgba=color(v[6]),color1_rgba=color(v[9]),offset1_rgba=color(v[10]),
                           original_vertex=v,normal=None))
    return result


def source_textures(scene,materials,ordinals,directory):
    result=bind(scene,materials,ordinals)
    ids=sorted({b['asset'] for d in result['draws'] for b in d['bindings'] if b['asset'] is not None})
    textures={};total=0
    for aid in ids:
        asset=materials['assets'][aid];fmt=asset['dxgi_format']
        require(fmt in (85,86,87),'selected source format unsupported')
        require(0<len(asset['mips'])<=13,'mip bound')
        levels=[]
        for level,mip in enumerate(asset['mips']):
            w,h=mip['width'],mip['height'];bpp=4 if fmt==87 else 2
            require(mip['level']==level and 0<w<=4096 and 0<h<=4096,'mip dimensions')
            require(re.fullmatch(r'asset-\d+-mip-\d+\.raw',mip['file']) is not None,'asset path')
            size=w*h*bpp;total+=size
            require(total<=64*1024*1024 and mip['bytes']==size and mip['row_bytes']==w*bpp,'byte bound/packing')
            path=directory/mip['file'];require(path.stat().st_size==size,'asset size')
            raw=path.read_bytes();require(fnv(raw)==mip['fnv64'],'asset hash')
            # Exact source RGBA conversion only: no gamma or lighting removal.
            rgba=decode(fmt,raw,w,h).tobytes()
            levels.append(dict(width=w,height=h,rgba=rgba,source_hash=mip['fnv64']))
        textures[aid]=dict(levels=levels,semantic='source-color-not-physical-albedo')
    return dict(result,textures=textures,raw_asset_bytes_verified=True,
                sampling_and_shading_baked=False)


def bind(scene, materials, ordinals):
    require(scene['schema']=='flycast-pvr-scene-v2' and materials['schema']=='flycast-source-materials-v1','schema')
    for field in ('frame_id','game_id','git_sha'):
        require(scene[field]==materials[field],'identity '+field)
    require(materials['scene_sha']==scene['git_sha'],'scene SHA')
    require(0<len(ordinals)<=128 and len(set(ordinals))==len(ordinals),'selection bound')
    result=[]
    for ordinal in ordinals:
        draws=[d for d in scene['draws'] if d['list']==0 and d['ordinal']==ordinal]
        require(len(draws)==1,'selected draw identity')
        draw=draws[0];slots=[]
        for slot in (0,1):
            matches=[b for b in materials['bindings'] if (b['list'],b['ordinal'],b['slot'])==(0,ordinal,slot)]
            require(len(matches)==1,'binding identity')
            b=matches[0];suffix='1' if slot else '';generation=draw['texture'+suffix]
            require((b['asset'] is None)==(generation is None),'binding presence')
            if generation is not None:
                for key in ('upload_generation','rtt_generation','palette_hash'):
                    require(b[key]==generation[key],'generation '+key)
                for key in ('tcw','tsp'):
                    require(b[key]==draw[key+suffix],'state '+key)
                aid=b['asset'];require(type(aid) is int and 0<=aid<len(materials['assets']),'asset index')
                require(materials['assets'][aid]['id']==aid,'asset identity')
            slots.append(dict(b))
        result.append(dict(source_draw=ordinal,original_draw=dict(draw),state=draw_state(draw),bindings=slots,normal_provenance='unknown'))
    return dict(frame_id=scene['frame_id'],draws=result,raw_asset_bytes_verified=False,
                physical_albedo_proven=False,renderable_by_remix_adapter=False)
