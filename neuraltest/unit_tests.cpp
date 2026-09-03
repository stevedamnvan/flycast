// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "hw/pvr/ta_ctx.h"
#include "rend/neural/instrumentation.h"
#include "rend/neural/motion_reference.h"
#include "rend/neural/neural_stage.h"

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
	draw.geomSig = 0xdef;
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
	const DrawRecord base = BaseDraw();
	suite.Expect(DrawSignature(base) == DrawSignature(base), "draw signature deterministic");
	auto changed = base;
	changed.texId++;
	suite.Expect(DrawSignature(base) != DrawSignature(changed), "draw signature changes with identity");
	changed = base;
	changed.zMax += .01f;
	suite.Expect(DrawSignature(base) != DrawSignature(changed), "draw signature covers depth bounds");

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
		current[0].geomSig++;
		current[0].ordinal = 30;
		const auto result = MatchDraws({previous, 1}, {current, 1});
		suite.Expect(result[0].tier == 2 && Near(result[0].confidence, .8f),
			"matcher tier 2 reordered structural");
	}
	{
		DrawRecord previous[] = {BaseDraw(3), BaseDraw(8)};
		previous[1].geomSig = 0x999;
		DrawRecord current[] = {previous[1], previous[0]};
		const auto result = MatchDraws({previous, 2}, {current, 2});
		suite.Expect(result[0].prevOrdinal == 8 && result[1].prevOrdinal == 3,
			"matcher one-to-one duplicate textures");
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
		current.geomSig++;
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
		context.global_param_op[0].tcw.full = 56;
		const auto& skipped = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(skipped.matches.data[0].confidence == 0.f,
			"unevaluated draw does not replace history reference");
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
		const auto jitter = HaltonJitter(0, 8);
		suite.Expect(Near(jitter.x, 0.f) && Near(jitter.y, -1.f / 6.f), "Halton first phase");
		suite.Expect(JitterPhaseCount(1920, 1920) == 8 && JitterPhaseCount(960, 1920) == 32,
			"jitter phase count");
	}
	suite.Expect(IsSceneCut(34, 100) && !IsSceneCut(35, 100), "scene-cut threshold");
	{
		const auto fourThree = ComputeContentRect(1920, 1080, 4.f / 3.f, false, 480);
		const auto widescreen = ComputeContentRect(1920, 1080, 16.f / 9.f, false, 480);
		suite.Expect(fourThree.x == 240 && fourThree.y == 0 && fourThree.width == 1440 &&
			fourThree.height == 1080 && widescreen.x == 0 && widescreen.width == 1920,
			"content rect respects 4:3 and widescreen aspect");
	}
	{
		const float sourceDepth = .375f;
		const float legacyEncoded = std::log2(1.f + 100000.f * sourceDepth) / 34.f;
		const float nativeEncoded = std::log2(1.f + 100000.f / sourceDepth) / 34.f;
		suite.Expect(Near(InvertLegacyDepth(legacyEncoded, false), sourceDepth, 1e-3f) &&
			Near(InvertLegacyDepth(nativeEncoded, true), sourceDepth, 1e-3f),
			"depth visualization inverse covers legacy and DIV_POS_Z paths");
	}

	std::cout << "selftest passed=" << suite.passed << " failed=" << suite.failed << '\n';
	return suite.failed == 0 ? 0 : 1;
}

} // namespace neuraltest
