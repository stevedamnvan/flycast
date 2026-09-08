"""Verify bounded material sidecars and show source textures, not physical albedo."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

def require(test, reason):
    if not test:
        raise ValueError(reason)

def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f"{value:016X}"

def decode(fmt, raw, width, height, palette=None, base=0):
    count = width * height
    require(len(raw) == count * {28:4,87:4,85:2,86:2,115:2,65:1}[fmt], "decode-size")
    if fmt in (28,87):
        p = np.frombuffer(raw, np.uint8).reshape(count,4)
        result = p if fmt == 28 else p[:,[2,1,0,3]]
    elif fmt == 65:
        require(palette is not None and len(palette)==4096, "missing-palette")
        index = np.frombuffer(raw,np.uint8).astype(np.uint32) + base
        require(index.max() < 1024, "palette-index-bound")
        result = np.frombuffer(palette,np.uint8).reshape(1024,4)[index][:,[2,1,0,3]]
    else:
        p = np.frombuffer(raw, "<u2").astype(np.uint32)
        def channel(shift, mask):
            return (((p >> shift) & mask) * 255 + mask//2) // mask
        if fmt == 86:
            result = np.stack([channel(10,31),channel(5,31),channel(0,31),channel(15,1)],axis=1)
        elif fmt == 115:
            result = np.stack([channel(8,15),channel(4,15),channel(0,15),channel(12,15)],axis=1)
        else:
            result = np.stack([channel(11,31),channel(5,63),channel(0,31),np.full(count,255)],axis=1)
    return Image.fromarray(result.astype(np.uint8).reshape(height,width,4))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture",type=Path)
    parser.add_argument("--out",type=Path,required=True)
    args = parser.parse_args()
    require(not args.out.exists(),"output-must-be-new")
    frames = sorted(args.capture.glob("frame-*/materials/manifest.json"))
    require(0 < len(frames) <= 30,"frame-count-bound")
    reports=[]
    sheet_entries=[]
    prior_frame=None
    for path in frames:
        require(path.stat().st_size <= 8*1024*1024,"manifest-size-bound")
        m=json.loads(path.read_text());parent=path.parent.parent
        require((parent/"pvr-scene.json").stat().st_size<=32*1024*1024 and (parent/"manifest.json").stat().st_size<=1024*1024,"source-file-bounds")
        scene=json.loads((parent/"pvr-scene.json").read_text())
        capture=json.loads((parent/"manifest.json").read_text())
        require(scene["schema"] in ("flycast-pvr-scene-v1", "flycast-pvr-scene-v2") and len(scene["draws"])<=8192,"scene-schema-draw-bound")
        require(m["schema"]=="flycast-source-materials-v1","schema")
        for field in ("frame_id","game_id","git_sha"):
            require(m[field]==scene[field]==capture[field],"identity-"+field)
        require(m["scene_sha"]==scene["git_sha"],"scene-sha")
        if prior_frame is not None:
            require(m["frame_id"]==prior_frame+1,"nonconsecutive-frame")
        prior_frame=m["frame_id"]
        require(len(m["assets"])<=256 and m["readback_bytes"]<=64*1024*1024,"asset-bound")
        assets=[];total=0
        for i, asset in enumerate(m["assets"]):
            require(asset["id"]==i and 0<len(asset["mips"])<=13,"asset-id-mips")
            bpp={28:4,87:4,85:2,86:2,115:2,65:1}[asset["dxgi_format"]]
            levels=[]
            for level, mip in enumerate(asset["mips"]):
                require(mip["level"]==level and 0<mip["width"]<=4096 and 0<mip["height"]<=4096,"mip-dimensions")
                if level:
                    require(mip["width"]==max(1,asset["mips"][0]["width"]>>level) and mip["height"]==max(1,asset["mips"][0]["height"]>>level),"mip-chain")
                size=mip["width"]*mip["height"]*bpp
                require(mip["row_bytes"]==mip["width"]*bpp and mip["bytes"]==size,"packing")
                total+=size;require(total<=64*1024*1024,"byte-bound")
                require(re.fullmatch(r"asset-\d+-mip-\d+\.raw",mip["file"]),"asset-path")
                rawpath=path.parent/mip["file"]
                require(rawpath.stat().st_size==size,"file-size")
                raw=rawpath.read_bytes();require(fnv(raw)==mip["fnv64"],"content-hash")
                levels.append(raw)
            assets.append(levels)
        require(total<=m["readback_bytes"] and 0<=m["logical_resources"]<=255,"readback-resource-accounting")
        draws={(d["list"],d["ordinal"]):d for d in scene["draws"]}
        require(len(m["bindings"])==len(draws)*2,"binding-count")
        seen=set();previews=set();palette=None
        if m["palette_asset"] is not None:
            pid=m["palette_asset"];require(isinstance(pid,int) and 0<=pid<len(assets),"palette-asset-index");a=m["assets"][pid]
            require(a["dxgi_format"]==87 and a["mips"][0]["width"]==32 and a["mips"][0]["height"]==32 and len(a["mips"])==1,"palette-layout")
            palette=assets[pid][0]
        for binding in m["bindings"]:
            key=(binding["list"],binding["ordinal"],binding["slot"])
            require(key not in seen and key[2] in (0,1),"binding-duplicate-slot");seen.add(key)
            draw=draws[key[:2]];slot=key[2];generation=draw["texture1" if slot else "texture"]
            require((binding["asset"] is None)==(generation is None),"binding-presence")
            if generation is None:
                continue
            for field in ("upload_generation","rtt_generation","palette_hash"):
                require(binding[field]==generation[field],"generation-"+field)
            require(binding["tcw"]==draw["tcw1" if slot else "tcw"] and binding["tsp"]==draw["tsp1" if slot else "tsp"],"draw-state")
            aid=binding["asset"];require(isinstance(aid,int) and 0<=aid<len(assets),"binding-asset-index");asset=m["assets"][aid]
            if binding["gpu_palette"]:
                require(asset["dxgi_format"]==65 and palette is not None,"palette-binding")
            else:
                require(asset["dxgi_format"]!=65,"unresolved-index-texture")
            previews.add((aid,binding["palette_base"] if binding["gpu_palette"] else -1))
        if not reports:
            require(len(previews)<=256,"preview-count-bound")
            args.out.mkdir(parents=True)
            for aid,base in sorted(previews):
                asset=m["assets"][aid];mip=asset["mips"][0]
                image=decode(asset["dxgi_format"],assets[aid][0],mip["width"],mip["height"],palette,base if base>=0 else 0)
                name=f"asset-{aid}-palette-{base}.png";image.save(args.out/name)
                sheet_entries.append((image,f"Asset {aid} | {mip['width']}x{mip['height']} | palette {base}"))
        reports.append({"frame":m["frame_id"],"game_id":m["game_id"],"git_sha":m["git_sha"],"assets":len(assets),"bindings":len(seen),"bytes":total,"logical_resources":m["logical_resources"],"palette_asset":m["palette_asset"],"scene_sha256":hashlib.sha256((parent/"pvr-scene.json").read_bytes()).hexdigest()})
    columns=6;cellw=190;cellh=162
    sheet=Image.new("RGB",(columns*cellw,48+((len(sheet_entries)+columns-1)//columns)*cellh),"#20262d")
    draw=ImageDraw.Draw(sheet);draw.text((12,12),"CAPTURED SOURCE TEXTURES | may contain painted lighting | NOT recovered PBR materials",fill="white")
    for i,(image,label) in enumerate(sheet_entries):
        x=(i%columns)*cellw;y=48+(i//columns)*cellh
        image.thumbnail((180,130),Image.Resampling.NEAREST)
        sheet.paste(image,(x+5,y),image.getchannel("A"));draw.text((x+5,y+134),label,fill="white")
    sheet.save(args.out/"source-textures.png")
    (args.out/"verification.json").write_text(json.dumps({"frames":reports,"verified_raw_content_and_bindings":True,"camera_recovered":False},indent=2))
    print(f"Verified {len(reports)} frames; first frame {reports[0]['assets']} assets, {len(sheet_entries)} texture/palette previews")

if __name__=="__main__":
    main()
