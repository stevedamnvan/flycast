// SPDX-License-Identifier: GPL-2.0-or-later
// Offline camera-relative approximation. Not a production or Remix renderer.
#include "harness.h"
#include "rend/neural/pvr_scene_capture.h"
#include "version.h"
#include "json/json.hpp"
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace neuraltest {
namespace {
struct V3 { double x=0,y=0,z=0; };
V3 sub(V3 a,V3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V3 cross(V3 a,V3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(V3 a,V3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 unit(V3 a) {double d=std::sqrt(dot(a,a)); return d>1e-15?V3{a.x/d,a.y/d,a.z/d}:V3{};}
V3 view(V3 p,double tangent,int w,int h,bool wrongDepth=false) {
 // Explicit hypothesis: view depth is reciprocal raw PVR z, not recovered truth.
 double z=wrongDepth?p.z:1/p.z;
 return {(p.x/w*2-1)*tangent*w/h*z,(1-p.y/h*2)*tangent*z,z};
}
V3 normal(const std::array<V3,3>& p,double tangent,int w,int h,bool wrong=false) {
 V3 a=view(p[0],tangent,w,h,wrong), b=view(p[1],tangent,w,h,wrong),c=view(p[2],tangent,w,h,wrong);
 V3 n=unit(cross(sub(b,a),sub(c,a)));
 // Two-sided camera-facing geometric approximation, not captured game normals.
 if(dot(n,a)>0) n={-n.x,-n.y,-n.z};
 return n;
}
double gain(V3 n,double intensity) {
 const V3 light=unit({-.6,.7,-.4});
 return 1+intensity*(std::max(0.,dot(n,light))-.35);
}
void require(bool b,const char* reason) {if(!b)throw std::runtime_error(reason);}
nlohmann::json manifest(const std::filesystem::path& path) {
 require(std::filesystem::file_size(path)<=1048576,"preview-manifest-bound");
 std::ifstream stream(path);
 return nlohmann::json::parse(stream,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){require(depth<32,"preview-manifest-depth");return true;});
}
void controls() {
 const double t=std::tan(60.*3.141592653589793/360.);
 const V3 p=view({480,120,.5},t,640,480);
 require(std::abs(p.z-2)<1e-12 && std::abs(p.x-4*t/3)<1e-12 && std::abs(p.y-t)<1e-12,"preview-unprojection-golden");
 require(std::abs(view({480,120,.5},t,640,480,true).z-2)>1,"preview-wrong-depth-control");
 require(std::abs(view({480,120,.5},std::tan(30.*3.141592653589793/360.),640,480).x-p.x)>.3,"preview-wrong-projection-control");
 const std::array<V3,3> tri{{{100,100,.5},{500,100,.5},{100,400,.5}}};
 const V3 n=normal(tri,t,640,480);
 require(std::abs(n.z+1)<1e-12 && gain(n,0)==1,"preview-normal-light-off-golden");
 require(std::abs(gain(n,1)-gain({-n.x,-n.y,-n.z},1))>.3,"preview-inverted-normal-control");
}
struct Surface {std::vector<double> depth; std::vector<V3> normals; std::vector<unsigned char> blocked;};
Surface raster(const flycast::rend::neural::PvrDecodedPacket& packet,int w,int h,double tangent,bool wrongDepth=false) {
 Surface s{std::vector<double>(w*h),std::vector<V3>(w*h),std::vector<unsigned char>(w*h)};
 std::vector<double> uncertain(w*h);
 const auto& m=packet.viewport;
 // Current scope is one affine screen-domain pass. No guessed matrix support.
 require(packet.passes.size()==1 && packet.passes[0].zClear,"preview-single-clear-pass-required");
 require(m[1]==0 && m[2]==0 && m[3]==0 && m[4]==0 && m[6]==0 && m[7]==0 && m[8]==0 && m[9]==0 && m[10]==1 && m[11]==0 && m[14]==0 && m[15]==1,"preview-affine-viewport-required");
 for(const auto& draw:packet.draws) {
  const auto& state=draw.state;
  std::array<V3,3> tri{}; unsigned seen=0;
  for(unsigned j=0;j<state.count;++j) {
   auto index=packet.indices[state.first+j];
   if(index==0xffffffffu) {seen=0;continue;}
   require(index<packet.vertices.size(),"preview-index");
   const auto& v=packet.vertices[index];
   V3 p{(m[0]*v.x+m[12]+1)*w*.5,(1-m[5]*v.y-m[13])*h*.5,v.z};
   if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.z<=0) {seen=0;continue;}
   tri[0]=tri[1];tri[1]=tri[2];tri[2]=p;if(++seen<3)continue;
   const auto a=tri[0],b=tri[1],c=tri[2];
   double area=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
   if(std::abs(area)<1e-8)continue;
   // Unsupported/translucent/punch-through geometry protects its footprint where
   // not behind the nearest opaque approximation, using raw PVR z ordering.
   // This intentionally overprotects; no texture alpha or culling is fabricated.
   bool trusted=draw.list==0 && state.isp.DepthMode==4 && !state.isp.ZWriteDis
    && (state.tileclip>>28)==0 && std::max({a.z,b.z,c.z})/std::min({a.z,b.z,c.z})<8;
   V3 n=normal(tri,tangent,w,h,wrongDepth);
   trusted=trusted && dot(n,n)>.9;
   int x0=int(std::max(0.,std::min(double(w),std::floor(std::min({a.x,b.x,c.x})))));
   int x1=int(std::max(0.,std::min(double(w),std::ceil(std::max({a.x,b.x,c.x})))));
   int y0=int(std::max(0.,std::min(double(h),std::floor(std::min({a.y,b.y,c.y})))));
   int y1=int(std::max(0.,std::min(double(h),std::ceil(std::max({a.y,b.y,c.y})))));
   for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
    double u=((b.y-c.y)*(x+.5-c.x)+(c.x-b.x)*(y+.5-c.y))/area;
    double v2=((c.y-a.y)*(x+.5-c.x)+(a.x-c.x)*(y+.5-c.y))/area, q=1-u-v2;
    if(u<0||v2<0||q<0)continue;
    size_t i=size_t(y)*w+x;
    double z=u*a.z+v2*b.z+q*c.z;
    if(!trusted) {uncertain[i]=std::max(uncertain[i],z);continue;}
    if(z>s.depth[i]) {s.depth[i]=z;s.normals[i]=n;}
   }
  }
 }
 for(size_t i=0;i<s.depth.size();++i)s.blocked[i]=uncertain[i]>=s.depth[i]*.999999;
 return s;
}
}

bool RunRemakePreview(const std::filesystem::path& input,const std::filesystem::path& output,
 const std::string& game,unsigned first,unsigned frames,unsigned fov,std::string& error) {
 try {
  require(frames>0&&frames<=30&&first>0&&first<=1000000&&fov>=30&&fov<=100,"preview-argument-bounds");
  require(!std::filesystem::exists(output),"preview-output-must-be-new");
  controls();
  std::filesystem::create_directories(output);
  std::ofstream report(output/"frames.csv");
  report<<"frame,eligible_pixels,changed_pixels,protected_mismatches,light_off_mismatches,inverted_normal_pixels,wrong_depth_pixels,wrong_projection_pixels\n";
  std::string sourceSha;
  const double tangent=std::tan(fov*3.141592653589793/360.);
  for(unsigned frame=first;frame<first+frames;++frame) {
   std::ostringstream name;name<<"frame-"<<std::setw(6)<<std::setfill('0')<<frame;
   const auto dir=input/name.str(),dest=output/name.str();
   flycast::rend::neural::PvrDecodedPacket p;
   if(!flycast::rend::neural::ReadPvrScenePacket(dir/"pvr-scene.json",frame,game,p,error))throw std::runtime_error(error);
   if(sourceSha.empty())sourceSha=p.gitSha;
   require(sourceSha==p.gitSha,"preview-mixed-source-sha");
   require(p.framebufferSize[0]==640&&p.framebufferSize[1]==480,"preview-packet-size");
   const auto sourceManifest=manifest(dir/"manifest.json");
   require(sourceManifest.at("frame_id")==frame && sourceManifest.at("game_id")==game && sourceManifest.at("git_sha")==p.gitSha,"preview-manifest-identity");
   int iw=0,ih=0,ic=0;
   require(stbi_info((dir/"native-pvr-color.png").string().c_str(),&iw,&ih,&ic)&&iw==640&&ih==480,"preview-source-size");
   Image source;if(!ReadPng(dir/"native-pvr-color.png",source,error))throw std::runtime_error(error);
   require(source.width==640&&source.height==480,"preview-only-640x480-world-crop");
   // This bounded native lane uses the captured RGBA input contract. Reject
   // another layout rather than silently accepting either channel ordering.
   std::uint64_t rawHash=14695981039346656037ull;for(auto byte:source.rgba){rawHash^=byte;rawHash*=1099511628211ull;}
   std::ostringstream hash;hash<<std::uppercase<<std::hex<<std::setw(16)<<std::setfill('0')<<rawHash;
   require(sourceManifest.at("contract_hashes").at("color_fnv64")==hash.str(),"preview-native-input-hash");
   auto s=raster(p,640,480,tangent),wrong=raster(p,640,480,tangent,true),wrongFov=raster(p,640,480,tangent*.5);
   Image result=source,off=source,normals=source,coverage=source,inverted=source,wrongImage=source,wrongProjection=source;
   size_t eligible=0,changed=0,invertedCount=0,wrongCount=0,projectionCount=0;
   for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x) {
    size_t i=size_t(y)*640+x,k=i*4;
    // Explicit world crop, not an automatic HUD classifier. Erode boundaries.
    bool use=y>=144&&y<432&&x>=2&&x<638&&!s.blocked[i]&&s.depth[i]>0;
    if(use)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
     size_t j=size_t(int(y)+dy)*640+int(x)+dx;
     if(s.blocked[j]||s.depth[j]<=0||dot(s.normals[i],s.normals[j])<.9)use=false;
    }
    auto n=s.normals[i];
    for(unsigned c=0;c<3;++c) {
     normals.rgba[k+c]=use?static_cast<unsigned char>(std::clamp((c==0?n.x:c==1?n.y:n.z)*127+128,0.,255.)):0;
     coverage.rgba[k+c]=use?255:0;
    }
    if(!use)continue;
    ++eligible;
    const double g=gain(n,1.4),gi=gain({-n.x,-n.y,-n.z},1.4),gw=gain(wrong.normals[i],1.4),gp=gain(wrongFov.normals[i],1.4);
    for(unsigned c=0;c<3;++c) {
     auto shade=[&](double factor){return static_cast<unsigned char>(std::clamp(std::round(source.rgba[k+c]*factor),0.,255.));};
     result.rgba[k+c]=shade(g);off.rgba[k+c]=shade(gain(n,0));
     inverted.rgba[k+c]=shade(gi);wrongImage.rgba[k+c]=shade(gw);wrongProjection.rgba[k+c]=shade(gp);
    }
    auto differs=[&](const Image& a,const Image& b){return a.rgba[k]!=b.rgba[k]||a.rgba[k+1]!=b.rgba[k+1]||a.rgba[k+2]!=b.rgba[k+2];};
    changed+=differs(source,result);invertedCount+=differs(result,inverted);wrongCount+=differs(result,wrongImage);projectionCount+=differs(result,wrongProjection);
   }
   require(off.rgba==source.rgba,"preview-light-off-not-identical");
   for(auto item:{std::make_pair("native.png",&source),{"approximation.png",&result},{"normals.png",&normals},{"coverage.png",&coverage},{"light-off.png",&off},{"inverted-normal.png",&inverted},{"wrong-depth.png",&wrongImage},{"wrong-projection.png",&wrongProjection}})
   if(!WritePng(dest/item.first,*item.second,error))throw std::runtime_error(error);
   std::ofstream identity(dest/"source-manifest.json");identity<<sourceManifest.dump(2);require(bool(identity),"preview-identity-write");
   // Verify protected pixels independently from shading-loop counters.
   size_t protectedMismatch=0;for(size_t i=0;i<640*480;++i)if(!coverage.rgba[i*4])for(int c=0;c<4;++c)protectedMismatch+=source.rgba[i*4+c]!=result.rgba[i*4+c];
   require(protectedMismatch==0,"preview-protection-mismatch");
   report<<frame<<','<<eligible<<','<<changed<<','<<protectedMismatch<<",0,"<<invertedCount<<','<<wrongCount<<','<<projectionCount<<'\n';
   report.flush();
   require(eligible>100&&changed>100&&invertedCount>100&&wrongCount>100&&projectionCount>100,"preview-insufficient-coverage-or-inert-control");
  }
  require(bool(report),"preview-report-write");
  std::ofstream metadata(output/"preview.json");
  const nlohmann::json record={
   {"schema","flycast-camera-relative-preview-v1"},{"implementation_sha",GIT_HASH},
   {"source_sha",sourceSha},{"game_id",game},{"first_frame",first},{"frames",frames},
   {"assumed_fov_degrees",fov},{"assumed_aspect",4./3},{"assumed_depth","1/raw_pvr_z arbitrary units"},
   {"crop_xywh",{0,144,640,288}},{"light_direction",{-.6,.7,-.4}},{"intensity",1.4},
   {"material","matched native image with baked lighting; SDR byte-domain multiplier"},
   {"normals","derived face normals oriented toward assumed camera"},
   {"limitations",{"not recovered camera or world geometry","modifier volumes retained only as baked source lighting",
    "no offscreen surfaces or actual materials","uncertain footprints protect where not behind nearest opaque raw depth; no texture alpha or cull emulation",
    "world crop is not HUD classification","no shadow rays, material separation or physically based light transport"}},
   {"remix",false},{"path_tracing",false},{"dlss5",false}};
  metadata<<record.dump(2);
  require(bool(metadata),"preview-metadata-write");return true;
 } catch(const std::exception& e) {error=e.what();return false;}
}
}
