// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "hw/pvr/ta_ctx.h"
#include "rend/neural/instrumentation.h"
#include "rend/neural/motion_reference.h"
#include "rend/neural/neural_stage.h"

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
		const auto& second = instrumentation->CaptureGeometry(context, {}, {}, 320, 240,
			320, 240, {0, 0, 320, 240}, {});
		suite.Expect(firstDrawCount == 1 && firstReset,
			"rend_context snapshot emits first-frame reset");
		suite.Expect(second.draws.size == 1 && second.matches.data[0].tier == 1 &&
			second.historyValid && instrumentation->DrawSnapshotHash() == firstHash,
			"rend_context snapshot and draw hash are deterministic");
		suite.Expect(!second.truncated, "atomic frame carries draw-overflow state");
	}
	{
		const auto jitter = HaltonJitter(0, 8);
		suite.Expect(Near(jitter.x, 0.f) && Near(jitter.y, -1.f / 6.f), "Halton first phase");
		suite.Expect(JitterPhaseCount(1920, 1920) == 8 && JitterPhaseCount(960, 1920) == 32,
			"jitter phase count");
	}
	suite.Expect(IsSceneCut(34, 100) && !IsSceneCut(35, 100), "scene-cut threshold");

	std::cout << "selftest passed=" << suite.passed << " failed=" << suite.failed << '\n';
	return suite.failed == 0 ? 0 : 1;
}

} // namespace neuraltest
