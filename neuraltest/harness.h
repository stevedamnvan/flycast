// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "rend/neural/dlss5_hook.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace neuraltest {

struct Vertex {
	float x;
	float y;
	float z;
	float r;
	float g;
	float b;
	float a;
};

struct Fixture {
	std::string name;
	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;
	bool dynamic = false;
	bool hasDepthGroundTruth = false;
	bool hasMotionGroundTruth = false;
};

struct Image {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> rgba;
};

struct RenderOptions {
	std::string renderer = "dx11";
	std::uint32_t scale = 1;
	std::uint32_t frame = 0;
	bool jitter = false;
	bool warp = false;
};

struct RenderResult {
	Image color;
	std::string adapter;
	std::string driver = "test-only-d3d11";
	std::uint64_t hash = 0;
};

struct NeuralRunResult {
	Image output;
	std::string adapter;
	std::string surface;
	std::string status;
	std::string reason;
	std::uint64_t submissions = 0;
	std::uint64_t busySkips = 0;
	std::uint64_t fallbacks = 0;
	std::uint64_t invalidFrames = 0;
	std::uint64_t outputChanges = 0;
	std::uint64_t outputHash = 0;
	std::uint32_t maxTemporalChangedPixels = 0;
	std::uint8_t maxTemporalDelta = 0;
	double minTemporalPsnr = 0.;
	std::int32_t lastNgxResult = 0;
	std::uint32_t lastExceptionCode = 0;
	std::uint64_t compatibilityRebuilds = 0;
	std::uint64_t compatibilityRebuildAttempts = 0;
	std::uint64_t compatibilityRebuildFailures = 0;
	flycast::rend::neural::Dlss5RebuildReason compatibilityRebuildReason =
		flycast::rend::neural::Dlss5RebuildReason::None;
	bool dlss5ContractEvaluated = false;
	flycast::rend::neural::Dlss5HookRoute dlss5Route =
		flycast::rend::neural::Dlss5HookRoute::None;
	flycast::rend::neural::Dlss5HookReadiness dlss5Readiness =
		flycast::rend::neural::Dlss5HookReadiness::Disabled;
	flycast::rend::neural::Dlss5HookComponents dlss5Components{};
};

struct DepthContractResult {
	std::string surface;
	std::string adapter;
	Image correctColor;
	Image reversedColor;
	Image wrongColor;
	std::vector<float> correctDepth;
	std::vector<float> reversedDepth;
	std::vector<float> wrongDepth;
	float clearDepth = 0.f;
	float farDepth = 0.f;
	float nearDepth = 0.f;
	float punchDepth = 0.f;
	float expectedFarDepth = 0.f;
	float expectedNearDepth = 0.f;
	bool nearIsGreater = false;
	bool clearIsNoGeometry = false;
	bool visibleOrderingAgrees = false;
	bool punchThroughAgrees = false;
	bool reversedSubmissionStable = false;
	bool wrongOrderFailed = false;
	bool nativeExportExact = false;
};

struct MotionContractResult {
	std::string adapter;
	Image previousColor;
	Image currentColor;
	std::vector<std::uint16_t> correctMotion;
	std::vector<std::uint16_t> reversedMotion;
	std::vector<std::uint16_t> doubledMotion;
	float staticX = 0.f;
	float staticY = 0.f;
	float translateX = 0.f;
	float translateY = 0.f;
	float verticalX = 0.f;
	float verticalY = 0.f;
	float cameraX = 0.f;
	float cameraY = 0.f;
	float deformationX = 0.f;
	float deformationY = 0.f;
	float expectedDeformationX = 0.f;
	float expectedDeformationY = 0.f;
	float jitterX = 0.f;
	float jitterY = 0.f;
	double correctReprojectionError = 0.;
	double reversedReprojectionError = 0.;
	double doubledReprojectionError = 0.;
	bool analyticTruth = false;
	bool negativeControlsFail = false;
};

struct ProductionMotionResult {
	std::string surface;
	std::string adapter;
	float trustedX = 0.f;
	float trustedY = 0.f;
	std::uint8_t trustedMask = 0;
	std::uint8_t trustedConfidence = 0;
	std::uint16_t trustedDrawId = 0;
	std::uint16_t trustedPreviousDrawId = 0;
	float invalidX = 0.f;
	float invalidY = 0.f;
	std::uint8_t invalidMask = 0;
	std::uint8_t invalidConfidence = 0;
	float oversizedX = 0.f;
	float oversizedY = 0.f;
	std::uint8_t oversizedMask = 0;
	std::uint8_t oversizedConfidence = 0;
	bool analyticTruth = false;
	bool invalidProtected = false;
	bool magnitudeProtected = false;
};

struct ColorContractResult {
	std::string adapter;
	Image source;
	Image roundTrip;
	bool byteExact = false;
	bool channelsExact = false;
	bool grayscaleExact = false;
	bool alphaIndependent = false;
	bool contentRectsExact = false;
	std::uint32_t differingPixels = 0;
	std::uint8_t maxDelta = 0;
};

struct DisocclusionContractResult {
	std::string surface;
	std::string adapter;
	Image resolvedMask;
	Image wrongMask;
	std::uint32_t protectedPixels = 0;
	std::uint32_t wrongMissedPixels = 0;
	std::uint64_t correctTrailEnergy = 0;
	std::uint64_t wrongTrailEnergy = 0;
	bool staticTrusted = false;
	bool cameraPanTrusted = false;
	bool depthToleranceTrusted = false;
	bool revealProtected = false;
	bool crossingProtected = false;
	bool depthProtected = false;
	bool outsideProtected = false;
	bool sceneCutProtected = false;
	bool newlyVisibleProtected = false;
};

struct TransparencyContractResult {
	std::string surface;
	std::string adapter;
	Image reactiveMask;
	bool emptyAndModifierClear = false;
	bool singleLayerReactive = false;
	bool multiLayerReactive = false;
	bool wrongControlFailed = false;
	bool mergePreservesBase = false;
};

struct OverlayContractResult {
	std::string surface;
	std::string adapter;
	Image original;
	Image neural;
	Image mask;
	Image composited;
	std::uint32_t protectedPixels = 0;
	std::uint32_t protectedMismatch = 0;
	std::uint32_t worldChanged = 0;
	std::uint32_t wrongProtectedMismatch = 0;
};

const std::vector<std::string>& FixtureNames();
bool MakeFixture(const std::string& name, std::uint32_t frame, Fixture& fixture, std::string& error);
bool RenderFixture(const Fixture& fixture, const RenderOptions& options, RenderResult& result, std::string& error);
bool WritePng(const std::filesystem::path& path, const Image& image, std::string& error);
bool WriteRaw(const std::filesystem::path& path, const Image& image, std::string& error);
bool ReadPng(const std::filesystem::path& path, Image& image, std::string& error);
std::uint64_t HashImage(const Image& image);
double ComputePsnr(const Image& a, const Image& b, std::uint32_t& differingPixels, std::uint8_t& maxDelta);
bool WriteRenderPackage(const std::filesystem::path& root, const Fixture& fixture,
	const RenderOptions& options, const RenderResult& result, std::string& error);
bool ValidateProductionExportShader(std::string& error);
bool RunDepthContractFixture(bool d3d11On12, DepthContractResult& result, std::string& error);
bool RunMotionContractFixture(MotionContractResult& result, std::string& error);
bool RunProductionMotionFixture(bool d3d11On12, ProductionMotionResult& result,
	std::string& error);
bool RunColorContractFixture(ColorContractResult& result, std::string& error);
bool RunDisocclusionContractFixture(bool d3d11On12,
	DisocclusionContractResult& result, std::string& error);
bool RunTransparencyContractFixture(bool d3d11On12,
	TransparencyContractResult& result, std::string& error);
bool RunOverlayContractFixture(bool d3d11On12,
	OverlayContractResult& result, std::string& error);
bool RunLiveNeuralD3D11(const Image& input, const std::string& backend,
	const std::string& mode, std::uint32_t outputWidth, std::uint32_t outputHeight,
	bool disableNgx, bool warp, bool depthInverted, const Image *previousInput,
	float motionX, float motionY, std::uint32_t frames, std::uint32_t dlssPreset,
	NeuralRunResult& result, std::string& error);
bool RunLiveNeuralD3D12(const Image& input, const std::string& backend,
	const std::string& mode, std::uint32_t outputWidth, std::uint32_t outputHeight,
	bool disableNgx, bool warp, bool depthInverted, const Image *previousInput,
	float motionX, float motionY, std::uint32_t frames, std::uint32_t dlssPreset,
	NeuralRunResult& result, std::string& error);
int RunSelfTests();
int CompareCaptureSequences(const std::filesystem::path& a,
	const std::filesystem::path& b, const std::filesystem::path& output,
	const std::string& aOutput, const std::string& bOutput);
std::vector<std::pair<std::string, bool>> CaptureComparisonSelfTests();

} // namespace neuraltest
