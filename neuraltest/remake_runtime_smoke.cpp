// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone developer bring-up, NOT a GPU/readback acceptance gate.
#include "remake_remix_adapter.h"
#include "remake_artifact_loader.h"
#include "remake_triangle_transport.h"
#include "remake_d3d9_dynamic.h"
#include "remake_d3d9_scene.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <d3d9.h>
#include <vector>
#include <algorithm>

namespace {
struct Watchdog {
 std::mutex mutex;
 std::condition_variable cv;
 bool done=false;
 std::thread thread{[this] {
  std::unique_lock<std::mutex> lock(mutex);
  if(!cv.wait_for(lock,std::chrono::seconds(30),[this]{return done;})) {
   std::cerr<<"remake-runtime-smoke timeout gpu_image_proven=false\n"<<std::flush;
   TerminateProcess(GetCurrentProcess(),124);
  }
 }};
 ~Watchdog() { {std::lock_guard<std::mutex> lock(mutex);done=true;} cv.notify_one();thread.join(); }
};
LRESULT CALLBACK windowProc(HWND window,UINT msg,WPARAM w,LPARAM l) {
 if(msg==WM_CLOSE) { PostQuitMessage(0);return 0; }
 return DefWindowProcW(window,msg,w,l);
}
}

int wmain(int argc,wchar_t** argv) {
 using namespace neuraltest::remake;
 if((argc!=5 && argc!=7 && argc!=9 && argc!=12 && argc!=14 && argc!=18) || std::wstring(argv[1])!=L"--runtime" || std::wstring(argv[3])!=L"--frames") {
  std::cerr<<"Usage: remake-runtime-smoke --runtime ABSOLUTE_DLL --frames 1..120 [--artifact ABSOLUTE_JSON --assets ABSOLUTE_DIR --clips NEAR FAR] [--capture|--capture-normals ABSOLUTE_NEW_BMP]\n";return 2;
 }
 wchar_t* end=nullptr;
 const long frames=wcstol(argv[4],&end,10);
 const std::filesystem::path runtime(argv[2]);
 if(!*argv[4] || *end || frames<1 || frames>120 || !runtime.is_absolute()) {
  std::cerr<<"invalid bounded arguments\n";return 2;
 }
 std::optional<Packet> snapshot;
 std::vector<Packet> sequence;
 std::filesystem::path capture;
 std::filesystem::path replacementTexture;
 bool reverseCamera=false;
 bool zeroLight=false;
 bool reverseLight=false;
 bool skinning=false,wrongSkinning=false;
 bool affine=false,affineReference=false,wrongAffineNormal=false;
 bool materialReplace=false,materialRepeat=false;
 bool gradientReference=false;
 bool gradientBlend=false;
 bool sourceColor=false;
 bool retainedTriangles=false;
 bool rebuiltFrozen=false,settledFrozen=false;
 bool legacyDynamic=false,legacyFrozen=false,legacyGame=false,legacyFrozenAttributes=false;
 bool legacyBackbuffer=false,legacyRaster=false;
 bool reverseOrder=false;
 bool emptyScene=false;
 auto captureType=REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_FINAL_COLOR;
 if(argc==7 || argc==9 || argc==14 || argc==18) {
  const int captureIndex=(argc==7||argc==9)?5:argc==18?16:12;
  capture=argv[captureIndex+1];
  const std::wstring captureOption=argv[captureIndex];
  legacyFrozen=captureOption==L"--capture-d3d9-frozen-color";
  legacyRaster=captureOption==L"--capture-d3d9-scene-raster"||captureOption==L"--capture-d3d9-scene-raster-frozen";
  legacyBackbuffer=captureOption==L"--capture-d3d9-backbuffer"||legacyRaster;
  legacyFrozenAttributes=captureOption==L"--capture-d3d9-scene-frozen-attributes"||captureOption==L"--capture-d3d9-scene-raster-frozen";
  if(legacyFrozenAttributes&&argc!=18)return 2;
  legacyGame=captureOption==L"--capture-d3d9-scene"||legacyFrozenAttributes||legacyRaster;
  legacyDynamic=captureOption==L"--capture-d3d9-dynamic"||legacyFrozen||legacyBackbuffer||legacyGame;
  if(legacyDynamic&&!legacyGame&&argc!=7)return 2;
  if(legacyGame&&argc!=14&&argc!=18)return 2;
  // Public NORMALS selects packed R32_UINT, not an XYZ float image.
  // D3D9 float-target blitting is not a valid typed readback of that resource.
  if(captureOption==L"--capture-normals") {
   std::cerr<<"packed normal capture unsupported; typed integer readback required\n";return 2;
  }
  reverseCamera=captureOption==L"--capture-reverse-camera";
  zeroLight=captureOption==L"--capture-zero-light";
  retainedTriangles=captureOption==L"--capture-retained-frozen-attributes";
  rebuiltFrozen=captureOption==L"--capture-rebuilt-frozen-attributes";
  settledFrozen=captureOption==L"--capture-settled-frozen-attributes";
  if((retainedTriangles||rebuiltFrozen||settledFrozen)&&argc!=18)return 2;
  sourceColor=captureOption==L"--capture-source-color" || retainedTriangles || rebuiltFrozen || settledFrozen || legacyGame;
  if(sourceColor&&argc!=14&&argc!=18){std::cerr<<"source color control requires artifact\n";return 2;}
  reverseLight=captureOption==L"--capture-reverse-light" || sourceColor;
  wrongSkinning=captureOption==L"--capture-skinning-reversed";
  affine=captureOption==L"--capture-affine-retained";
  wrongAffineNormal=captureOption==L"--capture-affine-wrong-normal";
  materialReplace=captureOption==L"--capture-material-replace";
  materialRepeat=captureOption==L"--capture-material-repeat";
  gradientBlend=captureOption==L"--capture-gradient-blend";
  gradientReference=captureOption==L"--capture-gradient-reference" || gradientBlend;
  if(gradientReference&&argc!=7)return 2;
  if(argc==9) {
   if(!(materialReplace||materialRepeat)||std::wstring(argv[7])!=L"--replacement-texture")return 2;
   replacementTexture=argv[8];
   if(!replacementTexture.is_absolute()||!std::filesystem::is_regular_file(replacementTexture))return 2;
   auto check=Synthetic();for(auto& mesh:check.meshes){mesh.material->sourceDds=replacementTexture;mesh.material->sourceColorExperiment=true;}
   if(!ReadyForAdapter(check,check.frame,check.game).ok)return 2;
  }
  if((materialReplace||materialRepeat)&&argc!=7&&argc!=9){std::cerr<<"material control is synthetic only\n";return 2;}
  affineReference=captureOption==L"--capture-affine-reference" || wrongAffineNormal;
  skinning=wrongSkinning || captureOption==L"--capture-skinning" || affine;
  if(affineReference && argc!=7){std::cerr<<"affine reference is synthetic only\n";return 2;}
  if(skinning && argc!=7){std::cerr<<"skinning control is synthetic only\n";return 2;}
  reverseOrder=captureOption==L"--capture-depth-reverse-order";
  emptyScene=captureOption==L"--capture-empty";
  if(captureOption==L"--capture-depth" || reverseOrder)captureType=REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_DEPTH;
  if((captureOption!=L"--capture" && captureOption!=L"--capture-normals" && captureOption!=L"--capture-depth" && !reverseCamera && !zeroLight && !reverseLight && !reverseOrder && !emptyScene && !skinning && !affineReference && !materialReplace && !materialRepeat && !gradientReference && !legacyDynamic) || !capture.is_absolute() || std::filesystem::exists(capture) || std::filesystem::exists(capture.wstring()+L".rgba32f") || ((reverseCamera || zeroLight || reverseOrder || emptyScene) && argc==14)) {
   std::cerr<<"capture requires new absolute BMP path\n";return 2;
  }
 }
 if(argc==12 || argc==14 || argc==18) {
  try {
   if(std::wstring(argv[5])!=L"--artifact" || std::wstring(argv[7])!=L"--assets" || std::wstring(argv[9])!=L"--clips")
    throw std::invalid_argument("artifact options");
   std::size_t a=0,b=0;float clipNear=std::stof(argv[10],&a),clipFar=std::stof(argv[11],&b);
   if(a!=std::wstring(argv[10]).size()||b!=std::wstring(argv[11]).size())throw std::invalid_argument("clip syntax");
   snapshot=LoadDiagnosticArtifact(argv[6],argv[8],clipNear,clipFar);
   if(argc==18) {
    if(frames!=63 || std::wstring(argv[12])!=L"--next" || std::wstring(argv[14])!=L"--next" ||
       (std::wstring(argv[16])!=L"--capture" && !reverseLight))throw std::invalid_argument("sequence requires 60 warmup plus three frames and final color capture");
    sequence.push_back(*snapshot);
    for(int i: {13,15}) {
     const std::filesystem::path root(argv[i]);
     auto next=LoadDiagnosticArtifact(root/L"scene.json",root/L"assets",clipNear,clipFar);
     if(!DiagnosticContinuation(sequence.back(),next))throw std::invalid_argument("sequence identity/origin mismatch");
     // Diagnostic isolation: no temporal resource identity claim across endpoints.
     if(!retainedTriangles&&!rebuiltFrozen&&!settledFrozen&&!legacyGame)for(auto& mesh:next.meshes)mesh.id+=sequence.size()*0x100000000ull;
     sequence.push_back(std::move(next));
    }
    if(retainedTriangles||rebuiltFrozen||settledFrozen)for(auto& endpoint:sequence)endpoint=TriangleBatches(endpoint);
    if(rebuiltFrozen||settledFrozen)for(std::size_t f=1;f<sequence.size();++f) {
     if(sequence[f].meshes.size()!=sequence[0].meshes.size())throw std::invalid_argument("frozen control topology");
     for(std::size_t m=0;m<sequence[f].meshes.size();++m) {
      auto& current=sequence[f].meshes[m];const auto& base=sequence[0].meshes[m];
      if(current.id!=base.id||current.indices!=base.indices||current.vertices.size()!=base.vertices.size())throw std::invalid_argument("frozen control topology");
      current.material=base.material;current.texture=base.texture;
      for(std::size_t v=0;v<current.vertices.size();++v) {
       current.vertices[v].u=base.vertices[v].u;current.vertices[v].v=base.vertices[v].v;
       current.vertices[v].publicColor=base.vertices[v].publicColor;
      }
      current.id+=f*0x100000000ull;
     }
    }
    for(const auto& endpoint:sequence) {
     if(std::filesystem::exists(capture.wstring()+L".frame-"+std::to_wstring(endpoint.frame)+L".bmp"))
      throw std::invalid_argument("sequence capture already exists");
    }
   }
   std::cout<<"diagnostic_snapshot=true source_frame="<<snapshot->frame<<" source_sha="<<snapshot->sourceGitSha
    <<" omissions="<<snapshot->omissions.size()<<" moving_gameplay_proven=false\n";
   if(!snapshot->diagnosticEmbeddingProvenance.empty())std::cout<<"diagnostic_embedding="<<snapshot->diagnosticEmbeddingProvenance<<'\n';
  }catch(const std::exception& e){std::cerr<<"artifact rejected before runtime load: "<<e.what()<<'\n';return 2;}
 }
 std::error_code error;
 if(!std::filesystem::is_regular_file(runtime,error)) {
  std::cerr<<"runtime unavailable runtime_loaded=false gpu_image_proven=false\n";return 3;
 }
 Watchdog watchdog;
 // Only the explicitly supplied directory and Windows system directory are searched.
 HMODULE module=LoadLibraryExW(runtime.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!module) { std::cerr<<"runtime load failed win32="<<GetLastError()<<"\n";return 4; }
 const auto initialize=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary"));
 if(!initialize) { FreeLibrary(module);std::cerr<<"public entry point missing\n";return 5; }
 remixapi_Interface api{};
 remixapi_InitializeLibraryInfo init{};
 init.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
 init.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
 auto status=initialize(&init,&api);
 if(status!=REMIXAPI_ERROR_CODE_SUCCESS) {
  std::cerr<<"initialize failed code="<<int(status)<<"\n";FreeLibrary(module);return 6;
 }
 if(!api.Startup || !api.Shutdown || !api.Present || !api.CreateMaterial || !api.DestroyMaterial
    || !api.CreateMesh || !api.DestroyMesh || !api.SetupCamera || !api.DrawInstance
    || !api.CreateLight || !api.DestroyLight || !api.DrawLightInstance) {
  std::cerr<<"incomplete public interface; process exit retains module, no unsafe unload\n";return 7;
 }
 WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"FlycastRemixSmoke";
 if(!RegisterClassW(&cls)) { api.Shutdown();FreeLibrary(module);return 8; }
 RECT bounds{0,0,640,480};
 AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
 HWND window=CreateWindowW(cls.lpszClassName,L"Experimental Remix synthetic bring-up",WS_OVERLAPPEDWINDOW,
  CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,cls.hInstance,nullptr);
 if(!window) { api.Shutdown();FreeLibrary(module);return 8; }
 remixapi_StartupInfo startup{};startup.sType=REMIXAPI_STRUCT_TYPE_STARTUP_INFO;startup.hwnd=window;
 startup.combineGuiInFinalColor=0;
 IDirect3D9Ex* ownedD3D=nullptr;
 IDirect3DDevice9Ex* ownedDevice=nullptr;
 std::cerr<<"phase=startup begin\n"<<std::flush;
 if(capture.empty())status=api.Startup(&startup);
 else {
  if(legacyDynamic&&!legacyRaster) {
   using Create9Ex=HRESULT (WINAPI*)(UINT,IDirect3D9Ex**);
   const auto create=reinterpret_cast<Create9Ex>(GetProcAddress(module,"Direct3DCreate9Ex"));
   status=create&&SUCCEEDED(create(D3D_SDK_VERSION,&ownedD3D))?REMIXAPI_ERROR_CODE_SUCCESS:REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
   std::cerr<<"legacy_factory=Direct3DCreate9Ex draw_conversion_expected=true\n";
  }else status=api.dxvk_CreateD3D9?api.dxvk_CreateD3D9(0,&ownedD3D):REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  if(legacyRaster)std::cerr<<"raster_contract_only=true factory=public_api draw_conversion_expected=false remix_output_proof=false\n";
  if(status==REMIXAPI_ERROR_CODE_SUCCESS && ownedD3D) {
   D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=640;pp.BackBufferHeight=480;
   // Capture reads the backbuffer after Present; DISCARD cannot preserve it.
   pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_COPY;
   pp.hDeviceWindow=window;pp.Windowed=TRUE;
   if(legacyDynamic){pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;}
   const HRESULT hr=ownedD3D->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
    D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,nullptr,&ownedDevice);
   status=SUCCEEDED(hr)&&ownedDevice&&api.dxvk_RegisterD3D9Device?
    api.dxvk_RegisterD3D9Device(ownedDevice):REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  }
 }
 std::cerr<<"phase=startup end code="<<int(status)<<'\n'<<std::flush;
 int outcome=0,accepted=0;
 if(status!=REMIXAPI_ERROR_CODE_SUCCESS) { std::cerr<<"startup failed code="<<int(status)<<"\n";outcome=9; }
 else {
  std::cerr<<"phase=show-window begin\n"<<std::flush;
  ShowWindow(window,SW_SHOW);
  std::cerr<<"phase=show-window end\n"<<std::flush;
  // Retain all submitted CPU buffers/resources across the bounded sequence.
  // Destruction/Shutdown ordering follows public API usage, not a proved GPU fence.
  std::cerr<<"diagnostic_light_direction=0,0,"<<(reverseLight?-1:1)<<" recovered_game_lighting=false\n";
  std::cerr<<"explicit_vertex_color="<<(gradientBlend||sourceColor)<<" baked_lighting_removed=false\n";
  RemixScene retained(api,zeroLight,reverseLight,skinning,affine||affineReference||materialReplace||materialRepeat||gradientReference,gradientBlend||sourceColor,retainedTriangles);
  if(retainedTriangles)std::cerr<<"retained_triangle_control=true frozen_source_attributes=true recovered_skeleton=false\n";
  if(rebuiltFrozen||settledFrozen)std::cerr<<"rebuilt_frozen_attributes=true settled_reference="<<settledFrozen<<" recovered_skeleton=false\n";
  if(affine||affineReference)std::cerr<<"affine_diagnostic_radiance=0.03 wrong_reference_normal="<<wrongAffineNormal<<'\n';
  std::vector<std::unique_ptr<RemixScene>> sequenceResources;
  DynamicD3D9Fixture legacyFixture(ownedDevice,api);
  D3D9PacketScene legacyScene(ownedDevice,api);
  for(long frame=0;frame<frames;frame++) {
   MSG msg{}; bool quit=false;
   while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
    if(msg.message==WM_QUIT) quit=true;
    TranslateMessage(&msg);DispatchMessageW(&msg);
   }
   if(quit) { outcome=10;break; }
   const auto sequenceIndex=settledFrozen?2:frame<60?0:frame-60;
   auto packet=!sequence.empty()?sequence.at(sequenceIndex):snapshot?*snapshot:Synthetic(frame+1,frames==1?0.f:float(frame)/float(frames-1)*.5f);
   if(legacyFrozenAttributes) {
    const auto& first=sequence.front();
    for(std::size_t m=0;m<packet.meshes.size();++m) {
     if(!LegacyResourceCompatible(first.meshes.at(m),packet.meshes[m])) {outcome=11;break;}
     for(std::size_t v=0;v<packet.meshes[m].vertices.size();++v) {
      auto& current=packet.meshes[m].vertices[v];const auto& original=first.meshes[m].vertices[v];
      current.u=original.u;current.v=original.v;current.publicColor=original.publicColor;
     }
    }
    if(outcome)break;
   }
   if(reverseCamera)packet.camera.position.x=-packet.camera.position.x;
   if(skinning || affineReference || materialReplace || materialRepeat || gradientReference)packet.camera.position.x=0;
   if(gradientReference)for(auto& mesh:packet.meshes) {
    mesh.material->albedo={1,1,1};
    mesh.vertices[0].publicColor=PublicColorFromBgra({160,120,80,255});
    mesh.vertices[1].publicColor=PublicColorFromBgra({120,80,160,255});
    mesh.vertices[2].publicColor=PublicColorFromBgra({80,160,120,255});
   }
   if(affineReference) {
    const float length=std::sqrt(1.25f*1.25f+.5f*.5f);
    for(auto& mesh:packet.meshes)for(auto& vertex:mesh.vertices) {
     const auto p=vertex.position,n=*vertex.normal;
     vertex.position={1.25f*p.x-.5f/length*p.z,p.y,.5f*p.x+1.25f/length*p.z};
     if(!wrongAffineNormal)vertex.normal=Vec3{1.25f*n.x-.5f/length*n.z,n.y,.5f*n.x+1.25f/length*n.z};
    }
   }
   if(reverseOrder)std::reverse(packet.meshes.begin(),packet.meshes.end());
   RECT client{};GetClientRect(window,&client);
   if(client.right<=0 || client.bottom<=0) {outcome=10;break;}
   if(!snapshot)packet.camera.aspect=float(client.right)/float(client.bottom);
   std::cerr<<"phase=submit begin frame="<<frame<<'\n'<<std::flush;
   Result submitted{false,"not-submitted"};
   if(legacyGame) {
    const auto hr=legacyScene.Draw(packet);
    submitted={SUCCEEDED(hr),"legacy-scene-draw-not-presentation-proof"};
    std::cerr<<"legacy_scene_hresult="<<hr<<" source_frame="<<packet.frame<<" current_vertex_attributes="<<!legacyFrozenAttributes<<" frozen_attribute_control="<<legacyFrozenAttributes<<'\n';
   }else if(legacyDynamic) {
    const auto hr=legacyFixture.Draw(frames==1?0.f:float(frame)/float(frames-1),legacyFrozen);
    submitted={SUCCEEDED(hr),"legacy-dynamic-draw-not-presentation-proof"};
    std::cerr<<"legacy_dynamic_hresult="<<hr<<" frame="<<frame<<" frozen_color="<<legacyFrozen<<'\n';
   }else if(retainedTriangles) {
    submitted=frame==0?retained.SubmitDiagnostic(packet,packet.frame,packet.game,true):
     frame<=60?retained.Redraw(packet.camera):retained.RedrawFrozenAttributeTriangles(packet);
    std::cerr<<"retained_source_frame="<<packet.frame<<" frozen_source_attributes=true\n";
   }else if(!sequence.empty()) {
    if(sequenceResources.empty()||(!settledFrozen&&sequenceResources.size()<=size_t(sequenceIndex))) {
     sequenceResources.push_back(std::make_unique<RemixScene>(api,false,reverseLight,false,false,sourceColor));
     submitted=sequenceResources.back()->SubmitDiagnostic(packet,packet.frame,packet.game,true);
    }else submitted=sequenceResources.back()->Redraw(packet.camera);
    std::cerr<<"sequence_source_frame="<<packet.frame<<" warmup="<<(frame<60)<<" source_sha="<<packet.sourceGitSha<<" temporal_identity_proven=false\n"<<std::flush;
   }else submitted=emptyScene?Result{true,"empty-scene-control"}:frame==0?(snapshot?retained.SubmitDiagnostic(packet,packet.frame,packet.game,true)
    :retained.Submit(packet,packet.frame,packet.game)):(frame==1&&(materialReplace||materialRepeat))?
     retained.RedrawSyntheticMaterial(packet.camera,materialReplace,replacementTexture):affine?retained.RedrawSyntheticAffine(packet.camera):skinning?
     retained.RedrawSyntheticSkinning(packet.camera,(wrongSkinning?-1.f:1.f)*float(frame)/float(frames-1)*.5f):retained.Redraw(packet.camera);
   std::cerr<<"phase=submit end frame="<<frame<<" ok="<<submitted.ok<<'\n'<<std::flush;
   if(!submitted.ok) { std::cerr<<"submit failed reason="<<submitted.reason<<"\n";outcome=11;break; }
   const auto presentFrame=[&]() {
   remixapi_PresentInfo present{};present.sType=REMIXAPI_STRUCT_TYPE_PRESENT_INFO;
   std::cerr<<"phase=present begin frame="<<frame<<'\n'<<std::flush;
   status=api.Present(&present);
   std::cerr<<"phase=present end frame="<<frame<<" code="<<int(status)<<'\n'<<std::flush;
   if(status!=REMIXAPI_ERROR_CODE_SUCCESS) { std::cerr<<"Present rejected code="<<int(status)<<"\n";outcome=12;return false; }
   accepted++;
   return true;
   };
   if(!legacyRaster&&!presentFrame())break;
  if(!capture.empty() && (frame+1==frames || (!sequence.empty() && frame>=60&&!settledFrozen)) && ownedDevice) {
   const std::filesystem::path capturePath=sequence.empty()?capture:
    std::filesystem::path(capture.wstring()+L".frame-"+std::to_wstring(packet.frame)+L".bmp");
   IDirect3DSurface9* gpu=nullptr;IDirect3DSurface9* cpu=nullptr;
   const bool floatOutput=captureType==REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_DEPTH;
   const auto format=floatOutput?D3DFMT_A32B32G32R32F:D3DFMT_A8R8G8B8;
   HRESULT hr=legacyBackbuffer?ownedDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&gpu):
    ownedDevice->CreateRenderTarget(640,480,format,D3DMULTISAMPLE_NONE,0,FALSE,&gpu,nullptr);
   if(SUCCEEDED(hr))hr=ownedDevice->CreateOffscreenPlainSurface(640,480,format,D3DPOOL_SYSTEMMEM,&cpu,nullptr);
   if(SUCCEEDED(hr)) {
    auto copied=legacyBackbuffer?REMIXAPI_ERROR_CODE_SUCCESS:api.dxvk_CopyRenderingOutput?api.dxvk_CopyRenderingOutput(gpu,captureType):REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    hr=copied==REMIXAPI_ERROR_CODE_SUCCESS?ownedDevice->GetRenderTargetData(gpu,cpu):E_FAIL;
   }
   D3DLOCKED_RECT locked{};
   if(SUCCEEDED(hr))hr=cpu->LockRect(&locked,nullptr,D3DLOCK_READONLY);
   if(SUCCEEDED(hr)) {
    std::vector<unsigned char> pixels(640*480*4);
    std::vector<unsigned char> raw(floatOutput?640*480*16:0);
    for(int y=0;y<480;y++) {
     const auto row=static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch;
     if(floatOutput)memcpy(raw.data()+y*640*16,row,640*16);
     if(!floatOutput)memcpy(pixels.data()+y*640*4,row,640*4);
     else for(int x=0;x<640;x++) {
      float value[4];memcpy(value,row+x*16,16);
      for(int c=0;c<3;c++) {
       float mapped=value[0]/10.f;
       pixels[(y*640+x)*4+c]=static_cast<unsigned char>(mapped>=1?255:mapped>0?mapped*255:0);
      }
      pixels[(y*640+x)*4+3]=255;
     }
    }
    cpu->UnlockRect();
    bool rawOk=true;
    if(floatOutput) {
     const auto rawPath=capturePath.wstring()+L".rgba32f";
     HANDLE rawFile=CreateFileW(rawPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
     DWORD bytes=0;
     rawOk=rawFile!=INVALID_HANDLE_VALUE;
     if(rawOk)rawOk=WriteFile(rawFile,raw.data(),DWORD(raw.size()),&bytes,nullptr)&&bytes==raw.size();
     if(rawFile!=INVALID_HANDLE_VALUE)CloseHandle(rawFile);
     std::cerr<<"raw_guidance width=640 height=480 channels=RGBA type=float32 rows=top-down frame="<<accepted
      <<" output_type="<<int(captureType)<<" write_ok="<<rawOk<<'\n'<<std::flush;
    }
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+DWORD(pixels.size());
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=640;info.biHeight=-480;info.biPlanes=1;info.biBitCount=32;info.biSizeImage=DWORD(pixels.size());
    HANDLE out=CreateFileW(capturePath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    DWORD written=0;
    bool ok=out!=INVALID_HANDLE_VALUE;
    if(ok)ok=WriteFile(out,&file,sizeof(file),&written,nullptr)&&written==sizeof(file);
    if(ok)ok=WriteFile(out,&info,sizeof(info),&written,nullptr)&&written==sizeof(info);
    if(ok)ok=WriteFile(out,pixels.data(),DWORD(pixels.size()),&written,nullptr)&&written==pixels.size();
    if(out!=INVALID_HANDLE_VALUE)CloseHandle(out);
    hr=ok&&rawOk?S_OK:E_FAIL;
   }
   if(cpu)cpu->Release();if(gpu)gpu->Release();
   std::cerr<<"capture_readback_hresult="<<hr<<" source_frame="<<packet.frame<<" image_validation_pending=true backbuffer_only="<<legacyBackbuffer<<" pre_present="<<legacyRaster<<"\n"<<std::flush;
   if(FAILED(hr)){outcome=14;break;}
  }
   if(legacyRaster&&!presentFrame())break;
  }
 }
 // Window destruction can dispatch callbacks installed by the runtime.
 if(ownedDevice) {
  IDirect3DQuery9* completion=nullptr;
  HRESULT completed=ownedDevice->CreateQuery(D3DQUERYTYPE_EVENT,&completion);
  if(SUCCEEDED(completed) && completion) {
   completed=completion->Issue(D3DISSUE_END);
   if(SUCCEEDED(completed)) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    do {
     completed=completion->GetData(nullptr,0,D3DGETDATA_FLUSH);
     if(completed!=S_FALSE)break;
     SwitchToThread();
    } while(std::chrono::steady_clock::now()<deadline);
   }
   completion->Release();
  }
  std::cerr<<"diagnostic_completion_hresult="<<completed<<" bounded_ms=2000\n"<<std::flush;
  if(completed!=S_OK)outcome=15;
 }
 // Keep the runtime alive until those callbacks can no longer run.
 std::cerr<<"phase=destroy-window begin\n"<<std::flush;
 DestroyWindow(window);
 std::cerr<<"phase=destroy-window end\n"<<std::flush;
 std::cerr<<"phase=shutdown begin\n"<<std::flush;
 status=api.Shutdown();
 std::cerr<<"phase=shutdown end code="<<int(status)<<'\n'<<std::flush;
 if(status!=REMIXAPI_ERROR_CODE_SUCCESS) { std::cerr<<"shutdown failed code="<<int(status)<<"\n";outcome=13; }
 // Do not unload a module whose shutdown failed; exit the isolated process.
 std::cerr<<"phase=unload begin\n"<<std::flush;
 if(status==REMIXAPI_ERROR_CODE_SUCCESS) FreeLibrary(module);
 std::cerr<<"phase=unload end\n"<<std::flush;
 std::cout<<"remake-runtime-smoke runtime_loaded=true present_api_success="<<accepted
  <<" gpu_image_proven=false readback_proven=false completion_lifetime_proven=false outcome="<<outcome<<"\n";
 return outcome;
}
