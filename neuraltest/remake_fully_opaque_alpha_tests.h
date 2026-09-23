// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rend/neural/remake_alpha_cutout.h"
template<class Suite> void TestRemakeFullyOpaqueAlpha(Suite& suite) {
 using namespace flycast::rend::neural;
 const auto dds=[](unsigned char alpha){
  std::vector<unsigned char> d(148+8*8*4,0);
  const auto word=[&](unsigned o,unsigned v){for(unsigned i=0;i<4;++i)d[o+i]=static_cast<unsigned char>(v>>(i*8));};
  word(0,0x20534444);word(4,124);word(12,8);word(16,8);word(84,0x30315844);word(128,28);
  for(unsigned i=0;i<64;++i)d[148+i*4+3]=alpha;return d;
 };
 const auto mesh=[&](unsigned char alpha){
  remake::Mesh m;m.id=(2ull<<32)|1;m.sourceAlphaBlend=true;m.sourceTsp=(4u<<29)|(5u<<26)|(3u<<6)|(1u<<20);
  m.texture={123,1,0,0,true};m.material.emplace();m.material->sourceTexture=m.texture;m.material->sourceDdsBytes=dds(alpha);
  for(auto uv:{std::pair{.25f,.25f},std::pair{.75f,.25f},std::pair{.25f,.75f}}){remake::Vertex v;v.position={uv.first,uv.second,1+uv.second};v.u=uv.first;v.v=uv.second;v.publicColor=0xffffffffu;m.vertices.push_back(v);}
  m.indices={0,1,2};return m;
 };
 auto m=mesh(255);RemakeAlphaPlane p;
 suite.Expect(DecodeRemakeDdsAlphaPlane(m.material->sourceDdsBytes,p),"fully opaque source DDS decodes");
 auto rectangle=MeasureRemakeAlphaCutout(p,m);auto stats=MeasureRemakeAlphaTriangles(p,m,rectangle);
 suite.Expect(RemakeAlphaFullyOpaque(stats)&&stats.fullyOpaque==stats.texels&&!RemakeAlphaCutoutQualifies(stats),"exact opaque is separate from clear-dependent cutout");
 // Force the triangle route through a failing rectangle, with a gradient outside its expanded union.
 RemakeAlphaPlane atlas;atlas.width=atlas.height=64;atlas.alpha.assign(4096,128);
 auto islands=m;islands.vertices.clear();
 for(auto uv:{std::pair{.1f,.1f},std::pair{.2f,.1f},std::pair{.1f,.2f},std::pair{.8f,.8f},std::pair{.9f,.8f},std::pair{.8f,.9f}}){remake::Vertex v;v.u=uv.first;v.v=uv.second;islands.vertices.push_back(v);}
 islands.indices={0,1,2,3,4,5};
 for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)if((x<18&&y<18)||(x>46&&y>46))atlas.alpha[y*64+x]=255;
 auto box=MeasureRemakeAlphaCutout(atlas,islands);auto tri=MeasureRemakeAlphaTriangles(atlas,islands,box);
 suite.Expect(!RemakeAlphaFullyOpaque(box)&&RemakeAlphaFullyOpaque(tri)&&tri.texels<box.texels,"exact opaque triangle union excludes unrelated atlas gradient");
 RemakeAlphaPlane border=p;
 for(unsigned y=0;y<8;++y)border.alpha[y*8+1]=254;
 auto borderBox=MeasureRemakeAlphaCutout(border,m);
 auto borderTri=MeasureRemakeAlphaTriangles(border,m,borderBox);
 suite.Expect(borderBox.fullyOpaque==borderBox.texels&&!RemakeAlphaFullyOpaque(borderBox)
  &&!RemakeAlphaFullyOpaque(borderTri),"unexpanded opaque rectangle cannot hide alpha254 filter border");
 auto invalid=m;invalid.indices[0]=999;
 suite.Expect(!RemakeAlphaFullyOpaque(MeasureRemakeAlphaTriangles(p,invalid,rectangle)),"invalid triangle fallback cannot certify local opaque rectangle");
 const auto promoted=[&](remake::Mesh x){remake::Packet packet;packet.meshes.push_back(std::move(x));RemakeAlphaPlaneCache cache;return PromoteRemakeAlphaCutouts(packet,cache,true);};
 suite.Expect(promoted(m).promoted==1,"exact opaque sampled mesh promotes under existing path");
 suite.Expect(promoted(mesh(254)).promoted==0,"alpha254 remains native instead of rounded opaque");
 suite.Expect(promoted(mesh(128)).promoted==0,"uniform intermediate alpha remains native");
 auto faint=m;faint.vertices[0].publicColor=0xfeffffffu;
 suite.Expect(promoted(faint).promoted==0,"exact opaque route rejects enabled vertex alpha254");
 faint.sourceTsp=*faint.sourceTsp&~(1u<<20);
 suite.Expect(promoted(faint).promoted==1,"disabled vertex alpha does not attenuate exact opaque texture");
 auto flat=m;for(auto& v:flat.vertices)v.position.z=1;
 suite.Expect(promoted(flat).promoted==0,"exact opaque route preserves depth extent safeguard");
 auto unknown=m;unknown.sourceTsp.reset();
 suite.Expect(promoted(unknown).promoted==0,"exact opaque route requires known vertex alpha state");
}
