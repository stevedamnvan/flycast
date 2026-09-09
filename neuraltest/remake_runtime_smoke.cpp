// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone developer bring-up, NOT a GPU/readback acceptance gate.
#include "remake_remix_adapter.h"
#include "remake_artifact_loader.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

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
 if((argc!=5 && argc!=12) || std::wstring(argv[1])!=L"--runtime" || std::wstring(argv[3])!=L"--frames") {
  std::cerr<<"Usage: remake-runtime-smoke --runtime ABSOLUTE_DLL --frames 1..120 [--artifact ABSOLUTE_JSON --assets ABSOLUTE_DIR --clips NEAR FAR]\n";return 2;
 }
 wchar_t* end=nullptr;
 const long frames=wcstol(argv[4],&end,10);
 const std::filesystem::path runtime(argv[2]);
 if(!*argv[4] || *end || frames<1 || frames>120 || !runtime.is_absolute()) {
  std::cerr<<"invalid bounded arguments\n";return 2;
 }
 std::optional<Packet> snapshot;
 if(argc==12) {
  try {
   if(std::wstring(argv[5])!=L"--artifact" || std::wstring(argv[7])!=L"--assets" || std::wstring(argv[9])!=L"--clips")
    throw std::invalid_argument("artifact options");
   std::size_t a=0,b=0;float clipNear=std::stof(argv[10],&a),clipFar=std::stof(argv[11],&b);
   if(a!=std::wstring(argv[10]).size()||b!=std::wstring(argv[11]).size())throw std::invalid_argument("clip syntax");
   snapshot=LoadDiagnosticArtifact(argv[6],argv[8],clipNear,clipFar);
   std::cout<<"diagnostic_snapshot=true source_frame="<<snapshot->frame<<" source_sha="<<snapshot->sourceGitSha
    <<" omissions="<<snapshot->omissions.size()<<" moving_gameplay_proven=false\n";
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
 status=api.Startup(&startup);
 int outcome=0,accepted=0;
 if(status!=REMIXAPI_ERROR_CODE_SUCCESS) { std::cerr<<"startup failed code="<<int(status)<<"\n";outcome=9; }
 else {
  ShowWindow(window,SW_SHOW);
  // Retain all submitted CPU buffers/resources across the bounded sequence.
  // Destruction/Shutdown ordering follows public API usage, not a proved GPU fence.
  RemixScene retained(api);
  for(long frame=0;frame<frames;frame++) {
   MSG msg{}; bool quit=false;
   while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
    if(msg.message==WM_QUIT) quit=true;
    TranslateMessage(&msg);DispatchMessageW(&msg);
   }
   if(quit) { outcome=10;break; }
   auto packet=snapshot?*snapshot:Synthetic(frame+1,frames==1?0.f:float(frame)/float(frames-1)*.5f);
   RECT client{};GetClientRect(window,&client);
   if(client.right<=0 || client.bottom<=0) {outcome=10;break;}
   if(!snapshot)packet.camera.aspect=float(client.right)/float(client.bottom);
   const auto submitted=frame==0?(snapshot?retained.SubmitDiagnostic(packet,packet.frame,packet.game,true)
    :retained.Submit(packet,packet.frame,packet.game)):retained.Redraw(packet.camera);
   if(!submitted.ok) { std::cerr<<"submit failed reason="<<submitted.reason<<"\n";outcome=11;break; }
   remixapi_PresentInfo present{};present.sType=REMIXAPI_STRUCT_TYPE_PRESENT_INFO;
   status=api.Present(&present);
   if(status!=REMIXAPI_ERROR_CODE_SUCCESS) { std::cerr<<"Present rejected code="<<int(status)<<"\n";outcome=12;break; }
   accepted++;
  }
 }
 status=api.Shutdown();
 if(status!=REMIXAPI_ERROR_CODE_SUCCESS) { std::cerr<<"shutdown failed code="<<int(status)<<"\n";outcome=13; }
 // Do not unload a module whose shutdown failed; exit the isolated process.
 if(status==REMIXAPI_ERROR_CODE_SUCCESS) FreeLibrary(module);
 DestroyWindow(window);
 std::cout<<"remake-runtime-smoke runtime_loaded=true present_api_success="<<accepted
  <<" gpu_image_proven=false readback_proven=false completion_lifetime_proven=false outcome="<<outcome<<"\n";
 return outcome;
}
