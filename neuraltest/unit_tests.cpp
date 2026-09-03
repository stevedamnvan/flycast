// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "hw/pvr/ta_ctx.h"
#include "rend/neural/instrumentation.h"
#include "rend/neural/dlss5_hook.h"
#include "rend/neural/motion_reference.h"
#include "rend/neural/neural_stage.h"
#include "rend/neural/presentation_cadence.h"
#include "rend/neural/quality_profile.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace neuraltest {
namespace {

using namespace flycast::rend::neural;

struct Suite {
	int passed = 0;
	int failed = 0;

	void Expect(bool condition, const std::string& name)
	{
		if (condition)
		{
			++passed;
			std::cout << "PASS " << name << '\n';
		}
		else
		{
			++failed;
			std::cerr << "FAIL " << name << '\n';
		}
	}
};

DrawRecord BaseDraw(std::uint16_t ordinal = 4)
{
	DrawRecord draw{};
	draw.list = 0;
	draw.pass = 1;
	draw.stateSig = 0x12345678;
	draw.texId = 27;
	draw.firstVertex = 9;
	draw.vertexCount = 6;
	draw.firstIndex = 12;
	draw.indexCount = 6;
	draw.stripCount = 1;
	draw.uvSig = 0xabc;
	draw.topologySig = 0xdef;
	draw.textureGeneration = 3;
	draw.paletteGeneration = 7;
	draw.zMin = .2f;
	draw.zMax = .7f;
	draw.bboxMin[0] = 10;
	draw.bboxMin[1] = 20;
	draw.bboxMax[0] = 90;
	draw.bboxMax[1] = 100;
	draw.ordinal = ordinal;
	return draw;
}

bool Near(float a, float b, float epsilon = 1e-4f)
{
	return std::abs(a - b) <= epsilon;
}

} // namespace

int RunSelfTests()
{
	Suite suite;
	{
		const auto faithful = ResolveQualityProfile(0, 0);
		const auto enhanced = ResolveQualityProfile(1, 3);
		const auto sprite = ResolveQualityProfile(2, 6);
		suite.Expect(faithful.faithful && faithful.conservativeTemporalMask
			&& faithful.protectCharacters && !faithful.bypassGenerative,
			"Faithful Dreamcast Remaster is the conservative default profile");
		suite.Expect(enhanced.faithful && !enhanced.conservativeTemporalMask
			&& enhanced.protectCharacters && std::string(enhanced.styleName) == "Cel-shaded",
			"Enhanced Materials retains character protection and style metadata");
		suite.Expect(!sprite.faithful && sprite.bypassGenerative
			&& sprite.externalRecommendation.find("user controlled") != std::string::npos,
			"sprite-heavy Photoreal profile remains explicit and recommends bypass");
	}
	{
		std::string error;
		const bool valid = ValidateProductionExportShader(error);
		suite.Expect(valid, "production native and neural-export vertex/pixel shaders compile");
		if (!valid && !error.empty())
			std::cerr << error << '\n';
	}
	const DrawRecord base = BaseDraw();
	suite.Expect(DrawSignature(base) == DrawSignature(base), "draw signature deterministic");
	auto changed = base;
	changed.texId++;
	suite.Expect(DrawSignature(base) != DrawSignature(changed), "draw signature changes with identity");
	changed = base;
	changed.zMax += .01f;
	suite.Expect(DrawSignature(base) != DrawSignature(changed), "draw signature covers depth bounds");
	changed = base;
	changed.centroid[0] += 24.f;
	changed.bboxMin[0] += 24;
	changed.bboxMax[0] += 24;
	suite.Expect(DrawStructuralSignature(base) == DrawStructuralSignature(changed)
		&& DrawSignature(base) != DrawSignature(changed),
		"structural identity is independent of pose");
	changed = base;
	changed.textureGeneration++;
	suite.Expect(DrawStructuralSignature(base) == DrawStructuralSignature(changed),
		"texture content generation is separate from structural identity");

	{
		DrawRecord previous[] = {base};
		DrawRecord current[] = {base};
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result.size() == 1 && result[0].tier == 1 && Near(result[0].confidence, 1.f),
			"matcher tier 1 exact");
	}
	{
		DrawRecord previous[] = {base};
		DrawRecord current[] = {base};
		current[0].topologySig++;
		current[0].ordinal = 30;
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result[0].tier == 2 && result[0].confidence >= .5f
			&& result[0].confidence < .8f && result[0].bestCost > 0.f,
			"matcher tier 2 reordered structural");
	}
	{
		DrawRecord previous[] = {BaseDraw(3), BaseDraw(8)};
		previous[1].topologySig = 0x999;
		DrawRecord current[] = {previous[1], previous[0]};
		const auto result = MatchDraws({previous, 2}, {current, 2});
		suite.Expect(result[0].prevOrdinal == 8 && result[1].prevOrdinal == 3,
			"matcher one-to-one duplicate textures");
	}
	{
		DrawRecord previous[] = {BaseDraw(0), BaseDraw(1)};
		previous[0].centroid[0] = 10.f;
		previous[1].centroid[0] = 110.f;
		DrawRecord current[] = {BaseDraw(0), BaseDraw(1)};
		current[0].centroid[0] = 108.f;
		current[1].centroid[0] = 12.f;
		const auto result = MatchDraws({previous, 2}, {current, 2});
		suite.Expect(result[0].prevOrdinal == 1 && result[1].prevOrdinal == 0
			&& result[0].bestCost < result[0].secondBestCost
			&& result[1].bestCost < result[1].secondBestCost,
			"minimum-cost assignment follows repeated-object pose across reorder");
	}
	{
		DrawRecord previous[] = {BaseDraw(11), BaseDraw(12)};
		previous[0].topologySig = 101;
		previous[0].centroid[0] = 10.f;
		previous[1].topologySig = 102;
		previous[1].centroid[0] = 110.f;
		DrawRecord current[] = {BaseDraw(0), BaseDraw(1)};
		current[0].topologySig = 201;
		current[0].centroid[0] = 108.f;
		current[1].topologySig = 202;
		current[1].centroid[0] = 12.f;
		const auto result = MatchDraws({previous, 2}, {current, 2});
		suite.Expect(result[0].tier == 2 && result[1].tier == 2
			&& result[0].prevOrdinal == 12 && result[1].prevOrdinal == 11
			&& result[0].bestCost < result[0].secondBestCost
			&& result[1].bestCost < result[1].secondBestCost,
			"structural bucket uses minimum-cost one-to-one assignment");
	}
	{
		DrawRecord previous[9];
		DrawRecord current[9];
		for (std::uint16_t i = 0; i < 9; ++i)
		{
			previous[i] = BaseDraw(i);
			current[i] = BaseDraw(i);
		}
		const auto result = MatchDraws({previous, 9}, {current, 9});
		const bool allAmbiguous = std::all_of(result.begin(), result.end(), [](const DrawMatch& match) {
			return match.confidence == 0.f &&
				match.reason == static_cast<std::uint8_t>(MatchReason::Ambiguous);
		});
		suite.Expect(allAmbiguous, "large repeated bucket is reactive-ambiguous with zero motion trust");
	}
	{
		for (int generationKind = 0; generationKind < 3; ++generationKind)
		{
			DrawRecord previous[] = {base};
			DrawRecord current[] = {base};
			if (generationKind == 0) ++current[0].textureGeneration;
			if (generationKind == 1) ++current[0].paletteGeneration;
			if (generationKind == 2) ++current[0].rttGeneration;
			const auto result = MatchDraws({previous, 1}, {current, 1});
			suite.Expect(result[0].tier == 0 && result[0].confidence == 0.f,
				generationKind == 0 ? "same-address texture replacement is untrusted" :
				generationKind == 1 ? "palette replacement is untrusted" :
				"rendered-texture replacement is untrusted");
		}
	}
	{
		DrawRecord previous[] = {base};
		DrawRecord current[] = {base};
		current[0].uvSig++;
		current[0].ordinal = 20;
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result[0].tier == 3 && Near(result[0].confidence, .5f),
			"matcher tier 3 similarity");
	}
	{
		DrawRecord previous = base;
		DrawRecord current = base;
		current.vertexCount += 2;
		current.indexCount += 2;
		current.topologySig++;
		current.ordinal += 8;
		current.bboxMin[0] += 4;
		const auto result = MatchDraws({&previous, 1}, {&current, 1});
		suite.Expect(result[0].tier == 2 && StripCoverage(previous, current) >= .9f,
			"strip-level changed-count match retains covered region");
	}
	{
		DrawRecord reactive = base;
		reactive.list = 2;
		reactive.bboxMax[0] = reactive.bboxMin[0] + 8;
		reactive.bboxMax[1] = reactive.bboxMin[1] + 8;
		suite.Expect(IsReactive(reactive), "small translucent draw is reactive");
		DrawRecord previous[] = {reactive};
		DrawRecord current[] = {reactive};
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result[0].confidence == 0.f &&
			result[0].reason == static_cast<std::uint8_t>(MatchReason::Reactive),
			"reactive draw emits no trusted match");
	}
	{
		DrawRecord hud = base;
		hud.list = 2;
		hud.flags = DrawScreenAligned;
		hud.zMin = hud.zMax = .5f;
		hud.bboxMin[0] = 2; hud.bboxMin[1] = 8;
		hud.bboxMax[0] = 42; hud.bboxMax[1] = 28;
		hud.ordinal = 18;
		suite.Expect(IsHighConfidenceOverlay(hud, 20, 320, 240, 3, 4),
			"stable late edge-aligned repeated-texture draw is a high-confidence overlay");
		DrawRecord interior = hud;
		interior.bboxMin[0] = 100; interior.bboxMax[0] = 140;
		interior.bboxMin[1] = 80; interior.bboxMax[1] = 100;
		DrawRecord physical = hud;
		physical.list = 0;
		DrawRecord perspective = hud;
		perspective.zMax += .02f;
		suite.Expect(!IsHighConfidenceOverlay(interior, 20, 320, 240, 3, 4)
			&& !IsHighConfidenceOverlay(physical, 20, 320, 240, 3, 4)
			&& !IsHighConfidenceOverlay(perspective, 20, 320, 240, 3, 4)
			&& !IsHighConfidenceOverlay(hud, 20, 320, 240, 2, 4)
			&& !IsHighConfidenceOverlay(hud, 20, 320, 240, 3, 1),
			"interior world, opaque, perspective, unstable, and unique-texture controls stay world geometry");
	}
	{
		DrawRecord menu[4]{};
		for (std::size_t i = 0; i < 4; ++i)
		{
			menu[i].flags = DrawScreenAligned;
			menu[i].zMin = menu[i].zMax = .5f;
			menu[i].bboxMin[0] = static_cast<std::int16_t>((i % 2) * 160);
			menu[i].bboxMin[1] = static_cast<std::int16_t>((i / 2) * 120);
			menu[i].bboxMax[0] = menu[i].bboxMin[0] + 160;
			menu[i].bboxMax[1] = menu[i].bboxMin[1] + 120;
		}
		suite.Expect(IsPredominantly2DFrame({menu, 4}, 320, 240),
			"tiled screen-aligned menu is conservatively classified as predominantly 2D");
		DrawRecord mixed[5] = {menu[0], menu[1], menu[2], menu[3], menu[0]};
		mixed[4].flags = 0;
		mixed[4].zMin = .1f; mixed[4].zMax = .9f;
		mixed[4].bboxMin[0] = 20; mixed[4].bboxMin[1] = 20;
		mixed[4].bboxMax[0] = 300; mixed[4].bboxMax[1] = 220;
		suite.Expect(!IsPredominantly2DFrame({mixed, 5}, 320, 240),
			"mixed 3D scene with screen-aligned HUD does not trigger the 2D bypass");
		std::uint8_t enter = 0;
		std::uint8_t exit = 0;
		bool active = false;
		active = UpdateConservativeBypass(true, active, enter, exit);
		active = UpdateConservativeBypass(false, active, enter, exit);
		active = UpdateConservativeBypass(true, active, enter, exit);
		suite.Expect(!active, "transient 2D classification cannot enter the conservative bypass");
		active = UpdateConservativeBypass(true, active, enter, exit);
		active = UpdateConservativeBypass(true, active, enter, exit);
		suite.Expect(active, "three consecutive 2D frames enter the conservative bypass");
		active = UpdateConservativeBypass(false, active, enter, exit);
		active = UpdateConservativeBypass(true, active, enter, exit);
		active = UpdateConservativeBypass(false, active, enter, exit);
		suite.Expect(active, "transient 3D classification cannot leave the conservative bypass");
		active = UpdateConservativeBypass(false, active, enter, exit);
		active = UpdateConservativeBypass(false, active, enter, exit);
		suite.Expect(!active, "three consecutive 3D frames leave the conservative bypass");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(6);
		for (std::size_t i = 0; i < context.verts.size(); ++i)
		{
			context.verts[i].x = static_cast<float>(10 + i * 11);
			context.verts[i].y = static_cast<float>(20 + i * 7);
			context.verts[i].z = .2f + static_cast<float>(i) * .01f;
		}
		context.idx = {0, 1, 2, ~u32{0}, 3, 4, 5};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = static_cast<u32>(context.idx.size());
		poly.tcw.full = 77;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& first = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		instrumentation->MarkEvaluated(first.frameId);
		for (auto& vertex : context.verts) vertex.x += 4.f;
		const auto& moved = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(moved.matches.data[0].confidence >= .5f && moved.historyValid
			&& !moved.sceneCut && instrumentation->TrustedPreviousVertexCount() == 6,
			"primitive-restart strip breaks preserve trusted previous positions");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(12);
		auto setQuad = [&](int baseVertex, float left, float top, float right, float bottom,
			float uvOffset) {
			const float positions[4][2] = {{left, top}, {left, bottom},
				{right, top}, {right, bottom}};
			for (int i = 0; i < 4; ++i)
			{
				auto& vertex = context.verts[baseVertex + i];
				vertex.x = positions[i][0]; vertex.y = positions[i][1]; vertex.z = .5f;
				vertex.u = uvOffset + (i >= 2 ? .1f : 0.f);
				vertex.v = i % 2 ? .1f : 0.f;
			}
		};
		setQuad(0, 2, 8, 34, 24, 0.f);
		setQuad(4, 2, 32, 48, 48, .2f);
		setQuad(8, 270, 70, 310, 100, .4f);
		context.idx = {0,1,2,3, 4,5,6,7, 8,9,10,11};
		for (int i = 0; i < 3; ++i)
		{
			PolyParam poly{};
			poly.init(); poly.first = i * 4; poly.count = 4; poly.tcw.full = 71;
			context.global_param_tr.push_back(poly);
		}
		RenderPass pass{};
		pass.tr_count = 3;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
		{
			if (frameIndex != 0)
			{
				for (int i = 8; i < 12; ++i) context.verts[i].y += 1.f;
			}
			const auto& frame = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
				320, 240, {0,0,320,240}, {});
			instrumentation->MarkEvaluated(frame.frameId);
		}
		suite.Expect(instrumentation->OverlayDrawCount() == 2
			&& instrumentation->IsOverlayOrdinal(0)
			&& instrumentation->IsOverlayOrdinal(1)
			&& !instrumentation->IsOverlayOrdinal(2),
			"accepted-frame overlay classifier protects stable HUD quads and rejects moving world control");
	}
	{
		DrawRecord current = base;
		current.list = 3;
		DrawRecord previous[] = {base};
		const auto result = MatchDraws({previous, 1}, {&current, 1});
		suite.Expect(result[0].confidence == 0.f && result[0].tier == 0,
			"unmatched draw has zero confidence");
	}
	{
		Point2 current[] = {{-1.f, 0.f}, {1.f, 0.f}, {0.f, 2.f}};
		Point2 previous[] = {{3.f, -3.f}, {3.f, 1.f}, {-1.f, -1.f}};
		const auto fit = FitSimilarity({previous, 3}, {current, 3});
		bool roundTrip = fit.valid;
		for (int i = 0; i < 3; ++i)
		{
			const auto value = fit.Apply(current[i]);
			roundTrip = roundTrip && Near(value.x, previous[i].x) && Near(value.y, previous[i].y);
		}
		suite.Expect(roundTrip, "rigid similarity fit round-trip");
	}
	{
		const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
			0, 0, 1, 0, 0, 0, 0, 1};
		float translated[16];
		std::copy(std::begin(identity), std::end(identity), translated);
		translated[12] = 4.f;
		translated[13] = -3.f;
		const auto projected = ProjectNaomi2(translated, identity, identity, {2.f, 5.f, 1.f});
		suite.Expect(Near(projected.x, 6.f) && Near(projected.y, 2.f),
			"Naomi 2 exact matrix path follows column-major shader transform");
	}
	{
		DrawMatch unmatched{};
		const auto rejected = ClassifyMotion(unmatched, {40.f, -7.f}, false, false);
		DrawMatch exact{};
		exact.confidence = 1.f;
		exact.reason = static_cast<std::uint8_t>(MatchReason::Exact);
		const auto trusted = ClassifyMotion(exact, {4.f, 0.f}, false, false);
		const auto tooLarge = ClassifyMotion(exact, {129.f, 0.f}, false, false);
		suite.Expect(!rejected.trusted && rejected.biasCurrentColor && Near(rejected.motion.x, 0.f),
			"unmatched draw emits zero motion and current-color bias");
		suite.Expect(trusted.trusted && !trusted.biasCurrentColor && Near(trusted.motion.x, 4.f),
			"trusted motion survives classification");
		suite.Expect(!tooLarge.trusted && tooLarge.biasCurrentColor && Near(tooLarge.motion.x, 0.f),
			"motion above 128 native pixels is rejected");
	}
	{
		HistoryTracker history;
		suite.Expect(history.ConsumeReset() && !history.ConsumeReset(), "initial reset consumed once");
		history.Evaluated(17);
		history.Skipped();
		suite.Expect(history.EvaluatedFrameId() == 17 && history.ConsecutiveSkips() == 1 &&
			history.ConsumeReset() && !history.ConsumeReset(), "skip preserves reference and requests one reset");
		history.Discontinuity();
		suite.Expect(history.Generation() == 1 && history.ConsumeReset(), "discontinuity increments generation");
	}
	{
		PresentationCadence cadence;
		cadence.Observe(10, 10, 10, true);
		cadence.Observe(11, 11, 11, true);
		cadence.Observe(12, 0, 11, true);
		cadence.Observe(13, 13, 0, true);
		cadence.Observe(14, 14, 14, true);
		cadence.Observe(15, 15, 0, false);
		const auto& stats = cadence.Stats();
		suite.Expect(stats.observedPresents == 5 && stats.missingPresents == 1
			&& stats.acceptedEvaluations == 5 && stats.neuralPresents == 4
			&& stats.nativePresents == 1 && stats.acceptedNotPresented == 2
			&& stats.outputFrameRepeats == 1 && stats.sourceFrameRepeats == 0
			&& stats.nativeNeuralAlternations == 2 && stats.latencySamples == 4
			&& stats.latencyFramesTotal == 1 && stats.latencyFramesMax == 1
			&& stats.frameIdentityMismatches == 0,
			"presentation cadence counts accepted drops, repeats, alternation, and latency");
	}
	{
		RecoveryController recovery;
		recovery.SetReady();
		recovery.RecordTransientFailure(1, 100);
		recovery.RecordTransientFailure(30, 200);
		recovery.RecordTransientFailure(60, 300);
		suite.Expect(recovery.State() == RecoveryState::FallbackHold && recovery.HoldEntries() == 1,
			"three failures in sliding window enter hold once");
		for (int i = 0; i < 60; ++i) recovery.OnHostPresent();
		suite.Expect(!recovery.CanEvaluate(1299), "hold waits at least one second");
		suite.Expect(recovery.CanEvaluate(1300) && recovery.ConsumeResumeReset() &&
			!recovery.ConsumeResumeReset(), "hold resumes with exactly one reset");
		recovery.RecordTransientFailure(200, 1400);
		suite.Expect(recovery.State() == RecoveryState::Ready,
			"hold exit clears stale failure window");
		RecoveryController removed;
		removed.SetReady();
		removed.DeviceRemoved();
		for (int i = 0; i < 120; ++i) removed.OnHostPresent();
		suite.Expect(!removed.CanEvaluate(10000)
			&& removed.State() == RecoveryState::DeviceRemoved,
			"device-removed state stays on native fallback until stage recreation");
	}
	{
		StageConfig config;
		config.mode = NeuralMode::Passthrough;
		NeuralStage stage(config);
		std::uint32_t identity = 1;
		NeuralFrame frame;
		frame.frameId = 9;
		frame.color.api = TextureApi::D3D11;
		frame.color.resource = &identity;
		suite.Expect(stage.TrySubmit(frame) == SubmitStatus::Submitted &&
			stage.TrySubmit(frame) == SubmitStatus::Submitted && stage.GetStats().submissions == 1,
			"stage evaluates an emulated frame once");
		frame.frameId = 10;
		frame.source = FrameSource::FramebufferDirect;
		suite.Expect(stage.TrySubmit(frame) == SubmitStatus::Unsupported,
			"framebuffer-direct bypasses stage");
	}
	{
		Dlss5CompatibilityRebuildPolicy policy;
		policy.Configure(3, 2);
		policy.Observe(Dlss5HookReadiness::MissingComponents, 20);
		policy.Observe(Dlss5HookReadiness::ComponentsPresent, 20);
		policy.Observe(Dlss5HookReadiness::ContractEvaluated, 22);
		const bool beforeGrace = !policy.ConsumeReleaseRequest();
		policy.Observe(Dlss5HookReadiness::ContractEvaluated, 23);
		const bool firstRelease = policy.ConsumeReleaseRequest()
			&& !policy.ConsumeReleaseRequest() && policy.BeginCreateAttempt();
		policy.CompleteCreateAttempt(false);
		const bool retry = policy.RetryAvailable() && policy.BeginCreateAttempt()
			&& policy.LastReason() == Dlss5RebuildReason::RetryAfterCreateFailure;
		policy.CompleteCreateAttempt(true);
		suite.Expect(beforeGrace && firstRelease && retry
			&& policy.Attempts() == 2 && policy.Failures() == 1
			&& policy.SuccessfulRebuilds() == 1 && !policy.RecreatePending(),
			"DLSS 5 readiness transition debounces one rebuild and bounds its retry");

		Dlss5CompatibilityRebuildPolicy disabled;
		disabled.Configure(0, 0);
		disabled.Observe(Dlss5HookReadiness::ComponentsPresent, 0);
		suite.Expect(!disabled.ConsumeReleaseRequest() && disabled.Attempts() == 0,
			"DLSS 5 compatibility rebuild can be disabled by configuration");
	}
	{
		auto experimental = CreateNeuralBackend(NeuralMode::Dlss5Experimental, Api::D3D12);
		suite.Expect(experimental->Initialize({}, nullptr, nullptr) == BackendEvalStatus::Unsupported
			&& std::string(experimental->GetStatusReason()).find(
#ifdef FLYCAST_ENABLE_NGX
				"D3D12 device"
#else
				"D3D11On12"
#endif
			) != std::string::npos,
			"experimental DLSS 5 D3D12 candidate reports an uninitialized public-NGX seam");
		Dlss5HookComponents complete{true, true, true, true};
		const auto missingRoute = AssessDlss5Hook(true, Dlss5HookRoute::None, complete, false);
		const auto missingComponents = AssessDlss5Hook(true, Dlss5HookRoute::D3D11External, {}, false);
		const auto ready = AssessDlss5Hook(true, Dlss5HookRoute::D3D11External, complete, false);
		const auto evaluated = AssessDlss5Hook(true, Dlss5HookRoute::D3D11On12, complete, true);
		suite.Expect(!missingRoute.componentsPresent
			&& missingRoute.readiness == Dlss5HookReadiness::MissingRoute
			&& !missingComponents.componentsPresent
			&& missingComponents.readiness == Dlss5HookReadiness::MissingComponents
			&& ready.componentsPresent && ready.readiness == Dlss5HookReadiness::ComponentsPresent
			&& evaluated.componentsPresent && evaluated.readiness == Dlss5HookReadiness::ContractEvaluated,
			"DLSS 5 readiness distinguishes route, components, and evaluated contract");
		auto experimentalD3D11 = CreateNeuralBackend(NeuralMode::Dlss5Experimental, Api::D3D11);
		suite.Expect(experimentalD3D11->Initialize({}, nullptr, nullptr) == BackendEvalStatus::Unsupported
			&& std::string(experimentalD3D11->GetStatusReason()).find(
#ifdef FLYCAST_ENABLE_NGX
				"D3D11 device"
#else
				"without FLYCAST_NEURAL_NGX"
#endif
			) != std::string::npos,
			"experimental DLSS 5 accepts the D3D11 public-NGX candidate route");
		auto d3d12 = CreateNeuralBackend(NeuralMode::DlaaHook, Api::D3D12);
		suite.Expect(d3d12->Initialize({}, nullptr, nullptr) == BackendEvalStatus::Unsupported
			&& std::string(d3d12->GetStatusReason()).find(
#ifdef FLYCAST_ENABLE_NGX
				"D3D12 device"
#else
				"D3D11On12"
#endif
			) != std::string::npos,
			"D3D12 backend reports the uninitialized surface precisely");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(3);
		context.verts[0].x = 10; context.verts[0].y = 20; context.verts[0].z = .2f;
		context.verts[1].x = 80; context.verts[1].y = 25; context.verts[1].z = .3f;
		context.verts[2].x = 40; context.verts[2].y = 90; context.verts[2].z = .4f;
		context.idx = {0, 1, 2};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = 3;
		poly.tcw.full = 55;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& first = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const bool firstReset = first.resetHistory;
		const auto firstDrawCount = first.draws.size;
		const auto firstHash = instrumentation->DrawSnapshotHash();
		const auto firstFrameId = first.frameId;
		instrumentation->MarkEvaluated(firstFrameId);
		const auto& second = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(firstDrawCount == 1 && firstReset,
			"rend_context snapshot emits first-frame reset");
		suite.Expect(second.draws.size == 1 && second.matches.data[0].tier == 1 &&
			second.historyValid && instrumentation->DrawSnapshotHash() == firstHash,
			"rend_context snapshot and draw hash are deterministic");
		suite.Expect(!second.truncated, "atomic frame carries draw-overflow state");
		std::uint32_t resources[6]{};
		const TextureRef refs[] = {
			{TextureApi::D3D11, &resources[0], nullptr, 28},
			{TextureApi::D3D11, &resources[1], nullptr, 41},
			{TextureApi::D3D11, &resources[2], nullptr, 34},
			{TextureApi::D3D11, &resources[3], nullptr, 61},
			{TextureApi::D3D11, &resources[4], nullptr, 61},
			{TextureApi::D3D11, &resources[5], nullptr, 57},
		};
		const auto& attached = instrumentation->AttachTextures(refs[0], refs[1], refs[2],
			refs[3], refs[4], refs[5]);
		suite.Expect(attached.color.resource == &resources[0]
			&& attached.depth.resource == &resources[1]
			&& attached.motion.resource == &resources[2]
			&& attached.mask.resource == &resources[3]
			&& attached.confidence.resource == &resources[4]
			&& attached.drawId.resource == &resources[5]
			&& attached.frameId == second.frameId,
			"atomic frame attaches the complete GPU export set");
		context.global_param_op[0].tcw.full = 56;
		const auto& skipped = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(skipped.matches.data[0].confidence == 0.f && skipped.sceneCut
			&& skipped.resetHistory,
			"unmatched scene cut rejects motion and resets history");
		context.global_param_op[0].tcw.full = 55;
		const auto& afterSkip = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(afterSkip.matches.data[0].tier == 1,
			"matching uses last successfully evaluated draw history");
		const auto generation = afterSkip.historyGeneration;
		const auto& direct = instrumentation->CaptureSource(FrameSource::FramebufferDirect,
			{}, 320, 240, 320, 240, {0, 0, 320, 240});
		suite.Expect(direct.source == FrameSource::FramebufferDirect && direct.draws.empty() &&
			direct.resetHistory && direct.historyGeneration == generation + 1,
			"framebuffer-direct package is geometry-free and resets history");
		const auto& geometryAgain = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(geometryAgain.resetHistory &&
			geometryAgain.historyGeneration == generation + 2,
			"framebuffer-direct to geometry transition increments history generation");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(3);
		context.verts[0].x = 10; context.verts[0].y = 20; context.verts[0].z = .2f;
		context.verts[1].x = 80; context.verts[1].y = 25; context.verts[1].z = .3f;
		context.verts[2].x = 40; context.verts[2].y = 90; context.verts[2].z = .4f;
		context.idx = {0, 1, 2};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = 3;
		poly.tcw.full = 55;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& original = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto originalFrame = original.frameId;
		const auto originalStructure = DrawStructuralSignature(original.draws.data[0]);
		instrumentation->MarkEvaluated(originalFrame);
		for (auto& vertex : context.verts) { vertex.x += 24.f; vertex.y -= 7.f; }
		const auto& moved = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto previousPositions = instrumentation->PreviousPositions();
		const float movedConfidence = moved.matches.data[0].confidence;
		suite.Expect(moved.matches.data[0].tier == 1 &&
			DrawStructuralSignature(moved.draws.data[0]) == originalStructure &&
			moved.draws.data[0].centroid[0] > 40.f,
			"production draw capture retains identity across pose translation");
		suite.Expect(previousPositions.size == 3 &&
			instrumentation->TrustedPreviousVertexCount() == 3 &&
			previousPositions.data[0].valid == 1.f && previousPositions.data[0].x == 10.f &&
			previousPositions.data[1].valid == 1.f && previousPositions.data[1].x == 80.f &&
			previousPositions.data[2].valid == 1.f && previousPositions.data[2].x == 40.f,
			"accepted exact topology emits prior positions by strip index");
		for (auto& vertex : context.verts) vertex.x += 6.f;
		const auto& afterOneSkip = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto afterSkippedPositions = instrumentation->PreviousPositions();
		const float oneSkipConfidence = afterOneSkip.matches.data[0].confidence;
		suite.Expect(afterOneSkip.historyAge == 2 && afterOneSkip.skippedFrameCount == 1
			&& oneSkipConfidence >= .5f && oneSkipConfidence < movedConfidence
			&& afterSkippedPositions.size == 3 &&
			afterSkippedPositions.data[0].x == 10.f &&
			afterSkippedPositions.data[1].x == 80.f &&
			afterSkippedPositions.data[2].x == 40.f,
			"one skipped pose ages confidence without replacing accepted history");
		const auto& afterTwoSkips = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const bool twoSkipsRetainBoundedTrust = afterTwoSkips.historyAge == 3
			&& afterTwoSkips.skippedFrameCount == 2
			&& afterTwoSkips.matches.data[0].confidence >= .5f
			&& afterTwoSkips.matches.data[0].confidence < oneSkipConfidence;
		const auto& afterThreeSkips = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(twoSkipsRetainBoundedTrust && afterThreeSkips.historyAge == 4
			&& afterThreeSkips.skippedFrameCount == 3
			&& afterThreeSkips.matches.data[0].confidence == 0.f
			&& instrumentation->TrustedPreviousVertexCount() == 0
			&& std::all_of(instrumentation->PreviousPositions().begin(),
				instrumentation->PreviousPositions().end(),
				[](const PreviousPosition& position) { return position.valid == 0.f; }),
			"history age degrades then rejects confidence after three skipped frames");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(3);
		context.verts[0].x = 10; context.verts[0].y = 20; context.verts[0].z = .2f;
		context.verts[1].x = 80; context.verts[1].y = 25; context.verts[1].z = .3f;
		context.verts[2].x = 40; context.verts[2].y = 90; context.verts[2].z = .4f;
		context.idx = {0, 1, 2};
		PolyParam poly{};
		poly.init();
		poly.first = 0;
		poly.count = 3;
		poly.tcw.full = 63;
		context.global_param_op.push_back(poly);
		RenderPass pass{};
		pass.op_count = 1;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& original = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		instrumentation->MarkEvaluated(original.frameId);
		for (auto& vertex : context.verts) { vertex.x += 12.f; vertex.y -= 4.f; }
		context.idx = {0, 2, 1};
		const auto& reindexed = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		const auto fittedPositions = instrumentation->PreviousPositions();
		suite.Expect(reindexed.matches.data[0].tier == 2
			&& reindexed.matches.data[0].confidence >= .5f
			&& Near(reindexed.matches.data[0].fitResidual, 0.f, .001f)
			&& instrumentation->TrustedPreviousVertexCount() == 3
			&& fittedPositions.data[0].valid == 1.f && Near(fittedPositions.data[0].x, 10.f)
			&& fittedPositions.data[1].valid == 1.f && Near(fittedPositions.data[1].x, 80.f)
			&& fittedPositions.data[2].valid == 1.f && Near(fittedPositions.data[2].x, 40.f),
			"reindexed rigid geometry uses bounded low-residual similarity fit");
		context.verts[2].x += 25.f;
		const auto& deformed = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(deformed.matches.data[0].tier != 0
			&& deformed.matches.data[0].fitResidual > .25f
			&& deformed.matches.data[0].confidence == 0.f
			&& instrumentation->TrustedPreviousVertexCount() == 0
			&& std::all_of(instrumentation->PreviousPositions().begin(),
				instrumentation->PreviousPositions().end(),
				[](const PreviousPosition& position) { return position.valid == 0.f; }),
			"reindexed deformation above residual threshold rejects false motion");
	}
	{
		rend_context context{};
		context.framebufferWidth = 320;
		context.framebufferHeight = 240;
		context.verts.resize(6);
		for (int i = 0; i < 3; ++i)
		{
			context.verts[i].x = static_cast<float>(i * 10);
			context.verts[i].y = static_cast<float>(i * 5);
			context.verts[i].z = .5f;
			context.verts[i + 3] = context.verts[i];
			context.verts[i + 3].x += 100.f;
		}
		context.idx = {0, 1, 2, 3, 4, 5};
		PolyParam first{};
		first.init();
		first.first = 0;
		first.count = 3;
		first.tcw.full = 71;
		PolyParam second = first;
		second.first = 3;
		context.global_param_op = {first, second};
		RenderPass pass{};
		pass.op_count = 2;
		context.render_passes.push_back(pass);
		auto instrumentation = std::make_unique<NeuralInstrumentation>();
		instrumentation->SetEnabled(true);
		const auto& previous = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		instrumentation->MarkEvaluated(previous.frameId);
		context.idx = {0, 1, 2, 0, 1, 2};
		instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(instrumentation->TrustedPreviousVertexCount() == 0 &&
			instrumentation->MatchForOrdinal(0)->confidence == 0.f &&
			instrumentation->MatchForOrdinal(1)->confidence == 0.f &&
			std::all_of(instrumentation->PreviousPositions().begin(),
				instrumentation->PreviousPositions().end(),
				[](const PreviousPosition& position) { return position.valid == 0.f; }),
			"shared current vertices with conflicting history are invalidated");
	}
	{
		const auto jitter = HaltonJitter(0, 8);
		suite.Expect(Near(jitter.x, 0.f) && Near(jitter.y, -1.f / 6.f), "Halton first phase");
		suite.Expect(JitterPhaseCount(1920, 1920) == 8 && JitterPhaseCount(960, 1920) == 32,
			"jitter phase count");
	}
	suite.Expect(IsSceneCut(34, 100) && !IsSceneCut(35, 100), "scene-cut threshold");
	suite.Expect(NextHistorySafeRingSlot(0, 1, 3, false) == 1
		&& NextHistorySafeRingSlot(0, 1, 3, true) == 2
		&& NextHistorySafeRingSlot(2, 1, 3, true) == 0,
		"guidance ring cannot overwrite retained accepted history after skips");
	{
		const auto fourThree = ComputeContentRect(1920, 1080, 4.f / 3.f, false, 480);
		const auto widescreen = ComputeContentRect(1920, 1080, 16.f / 9.f, false, 480);
		suite.Expect(fourThree.x == 240 && fourThree.y == 0 && fourThree.width == 1440 &&
			fourThree.height == 1080 && widescreen.x == 0 && widescreen.width == 1920,
			"content rect respects 4:3 and widescreen aspect");
		const auto matchHd = ComputeMatchOutputRasterSize(1920, 1080, 4.f / 3.f, false);
		const auto matchQhd = ComputeMatchOutputRasterSize(2560, 1440, 4.f / 3.f, false);
		const auto matchUhd = ComputeMatchOutputRasterSize(3840, 2160, 4.f / 3.f, false);
		const auto matchWide = ComputeMatchOutputRasterSize(3840, 2160, 16.f / 9.f, false);
		suite.Expect(matchHd.width == 1440 && matchHd.height == 1080
			&& matchQhd.width == 1920 && matchQhd.height == 1440
			&& matchUhd.width == 2880 && matchUhd.height == 2160
			&& matchWide.width == 3840 && matchWide.height == 2160,
			"match-output raster uses exact post-aspect content dimensions");
		suite.Expect(RoundManualRasterWidth(640.f * (320.f / 480.f), false) == 426
			&& RoundManualRasterWidth(640.f * (320.f / 480.f), true) == 427,
			"Quality SR exact-width path avoids the one-pixel NGX contract mismatch");
		suite.Expect(UsesMatchOutputRaster(2) && UsesMatchOutputRaster(3)
			&& UsesMatchOutputRaster(8) && !UsesMatchOutputRaster(0)
			&& !UsesMatchOutputRaster(1) && !UsesMatchOutputRaster(4),
			"match-output raster is limited to target-native lanes");
	}
	{
		const float sourceDepth = .375f;
		const float legacyEncoded = std::log2(1.f + 100000.f * sourceDepth) / 34.f;
		const float nativeEncoded = std::log2(1.f + 100000.f / sourceDepth) / 34.f;
		suite.Expect(Near(InvertLegacyDepth(legacyEncoded, false), sourceDepth, 1e-3f) &&
			Near(InvertLegacyDepth(nativeEncoded, true), sourceDepth, 1e-3f),
			"depth visualization inverse covers legacy and DIV_POS_Z paths");
	}
	{
		DepthContractResult native;
		DepthContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunDepthContractFixture(false, native, fixtureError);
		suite.Expect(nativeOk, "production depth contract passes on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunDepthContractFixture(true, on12, fixtureError);
		suite.Expect(on12Ok, "production depth contract passes on D3D11On12");
		suite.Expect(nativeOk && on12Ok && native.correctDepth == on12.correctDepth
			&& native.correctColor.rgba == on12.correctColor.rgba,
			"production depth contract is exact across D3D11 surfaces");
	}
	{
		MotionContractResult motion;
		std::string fixtureError;
		const bool motionOk = RunMotionContractFixture(motion, fixtureError);
		suite.Expect(motionOk && motion.analyticTruth,
			"GPU motion contract matches analytic render-pixel truth");
		suite.Expect(motionOk && motion.negativeControlsFail,
			"reversed and doubled motion fail pixel reprojection");
	}
	{
		ProductionMotionResult native;
		ProductionMotionResult on12;
		std::string fixtureError;
		const bool nativeOk = RunProductionMotionFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk && native.analyticTruth,
			"production PVR shader rasterizes accepted motion on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunProductionMotionFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok && on12.analyticTruth,
			"production PVR shader rasterizes accepted motion on D3D11On12");
		suite.Expect(nativeOk && on12Ok && Near(native.trustedX, on12.trustedX)
			&& Near(native.trustedY, on12.trustedY)
			&& native.trustedMask == on12.trustedMask
			&& native.trustedConfidence == on12.trustedConfidence
			&& native.trustedDrawId == on12.trustedDrawId
			&& native.trustedPreviousDrawId == on12.trustedPreviousDrawId,
			"production motion guidance is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.invalidProtected && on12.invalidProtected
			&& native.magnitudeProtected && on12.magnitudeProtected,
			"invalid and excessive production motion are current-color protected");
	}
	{
		ColorContractResult color;
		std::string fixtureError;
		const bool colorOk = RunColorContractFixture(color, fixtureError);
		suite.Expect(colorOk && color.byteExact && color.channelsExact
			&& color.grayscaleExact && color.alphaIndependent,
			"production SDR quad path preserves color and alpha exactly");
		suite.Expect(colorOk && color.contentRectsExact,
			"content rectangle examples and odd-size rounding are exact");
	}
	{
		DisocclusionContractResult native;
		DisocclusionContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunDisocclusionContractFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk, "production disocclusion contract passes on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunDisocclusionContractFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok, "production disocclusion contract passes on D3D11On12");
		suite.Expect(nativeOk && on12Ok
			&& native.resolvedMask.rgba == on12.resolvedMask.rgba,
			"production disocclusion mask is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.wrongMissedPixels > 0
			&& native.wrongTrailEnergy > native.correctTrailEnergy,
			"wrong disocclusion control has measurable trail energy");
	}
	{
		TransparencyContractResult native;
		TransparencyContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunTransparencyContractFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk, "production OIT visible fragments are reactive on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunTransparencyContractFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok, "production OIT visible fragments are reactive on D3D11On12");
		suite.Expect(nativeOk && on12Ok
			&& native.reactiveMask.rgba == on12.reactiveMask.rgba,
			"production OIT reactive coverage is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.emptyAndModifierClear
			&& native.singleLayerReactive && native.multiLayerReactive
			&& native.wrongControlFailed && native.mergePreservesBase,
			"empty/modifier-only pixels stay clear while single and multi-layer translucency reject the omitted-mask control");
	}
	{
		OverlayContractResult native;
		OverlayContractResult on12;
		std::string fixtureError;
		const bool nativeOk = RunOverlayContractFixture(false, native, fixtureError);
		if (!nativeOk && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(nativeOk, "protected overlay pixels composite byte-exactly on native D3D11");
		fixtureError.clear();
		const bool on12Ok = RunOverlayContractFixture(true, on12, fixtureError);
		if (!on12Ok && !fixtureError.empty()) std::cerr << fixtureError << '\n';
		suite.Expect(on12Ok, "protected overlay pixels composite byte-exactly on D3D11On12");
		suite.Expect(nativeOk && on12Ok && native.composited.rgba == on12.composited.rgba,
			"overlay composition is exact across D3D11 surfaces");
		suite.Expect(nativeOk && on12Ok && native.worldChanged == 0
			&& native.wrongProtectedMismatch == native.protectedPixels,
			"default overlay composite preserves unclassified world and omitted-composite control fails");
	}

	for (const auto& test : CaptureComparisonSelfTests())
		suite.Expect(test.second, test.first);
	std::cout << "selftest passed=" << suite.passed << " failed=" << suite.failed << '\n';
	return suite.failed == 0 ? 0 : 1;
}

} // namespace neuraltest
