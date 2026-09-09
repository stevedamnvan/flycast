// SPDX-License-Identifier: GPL-2.0-or-later
#include "remake_artifact_loader.h"
#include <iostream>
#include <iomanip>
int wmain(int argc,wchar_t** argv) {
 try {
  if(argc!=5)throw std::invalid_argument("usage: artifact assets clipNear clipFar");
  std::size_t n=0,f=0;const float clipNear=std::stof(argv[3],&n),clipFar=std::stof(argv[4],&f);
  if(n!=std::wstring(argv[3]).size()||f!=std::wstring(argv[4]).size())throw std::invalid_argument("clip syntax");
  const auto p=neuraltest::remake::LoadDiagnosticArtifact(argv[1],argv[2],clipNear,clipFar);
  std::size_t vertices=0;for(const auto& m:p.meshes)vertices+=m.vertices.size();
  std::cout<<std::setprecision(9)<<"diagnostic-artifact meshes="<<p.meshes.size()<<" vertices="<<vertices
   <<" frame="<<p.frame<<" game="<<p.game<<" source_sha="<<p.sourceGitSha<<" aspect="<<p.camera.aspect
   <<" omissions="<<p.omissions.size()<<" runtime_loaded=false rendered=false\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"artifact rejected: "<<e.what()<<'\n';return 1;}
}
