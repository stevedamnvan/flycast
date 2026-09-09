// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_scene.h"
#include <remix_c.h>

namespace neuraltest::remake {
// Developer-only M1 adapter: synthetic, untextured, explicit identity meshes.
// The caller owns runtime startup, GPU completion, Present, and shutdown.
// Keep this object alive through frame consumption; destruction does not wait.
// A submission failure requires discarding the entire frame, not presentation.
class RemixScene {
public:
 explicit RemixScene(remixapi_Interface api, bool zeroLightControl=false, bool reverseLightControl=false) : api_(api), zeroLightControl_(zeroLightControl), reverseLightControl_(reverseLightControl) {}
 ~RemixScene();
 RemixScene(const RemixScene&) = delete;
 RemixScene& operator=(const RemixScene&) = delete;
 Result Submit(const Packet&, std::uint64_t expectedFrame, const std::string& expectedGame);
 Result SubmitDiagnostic(const Packet&, std::uint64_t, const std::string&, bool callerSuppliedClips);
 bool IsDiagnostic() const { return diagnostic_; }
 const std::vector<std::string>& RetainedOmissions() const { return packet_.omissions; }
 // Reuse the immutable submitted geometry; only the explicitly supplied camera changes.
 Result Redraw(const Camera&);
private:
 Result SubmitChecked(const Packet&, std::uint64_t, const std::string&, bool diagnostic);
 bool diagnostic_ = false;
 Result DrawFrame(const Camera&);
 Packet packet_;
 bool ready_ = false;
 remixapi_Interface api_{};
 bool zeroLightControl_=false;
 bool reverseLightControl_=false; // Explicit diagnostic direction, not recovered game lighting.
 std::vector<remixapi_MeshHandle> meshes_;
 std::vector<remixapi_MaterialHandle> materials_;
 std::vector<std::wstring> texturePaths_;
 remixapi_LightHandle light_ = nullptr;
 bool attempted_ = false;
 // Own the memory referenced by the public API through consumption.
 std::vector<std::vector<remixapi_HardcodedVertex>> vertices_;
 std::vector<std::vector<std::uint32_t>> indices_;
};
}
