#include "ta_ctx.h"
#ifdef FLYCAST_ENABLE_NEURAL
#include "rend/neural/remake_cpu_scope.h"
#include <chrono>
#endif
#include "spg.h"
#include "cfg/option.h"
#include "Renderer_if.h"
#include "serialize.h"
#include "stdclass.h"
#include "hw/sh4/sh4_sched.h"
#ifdef FLYCAST_ENABLE_NEURAL
#include "rend/neural/source_sq_scope.h"
#include "rend/neural/source_transform.h"
#include "rend/neural/source_arithmetic.h"
#include "rend/neural/source_hook_attribution.h"
#include "rend/neural/source_observation_scope.h"
#endif

#include <mutex>
#include <vector>
#include <cstdlib>

extern u32 fskip;
static int RenderCount;

TA_context* ta_ctx;
tad_context ta_tad;

static void tactx_Recycle(TA_context* ctx);
static TA_context *tactx_Find(u32 addr, bool allocnew = false);

void SetCurrentTARC(u32 addr)
{
	if (addr != TACTX_NONE)
	{
		if (ta_ctx)
			SetCurrentTARC(TACTX_NONE);

		verify(ta_ctx == 0);
		//set new context
		ta_ctx = tactx_Find(addr,true);

		//copy cached params
		ta_tad = ta_ctx->tad;
	}
	else
	{
		//Flush cache to context
		verify(ta_ctx != 0);
		ta_ctx->tad=ta_tad;
		
		//clear context
		ta_ctx=0;
		ta_tad.Reset(0);
	}
}

static TA_context* rqueue;
static cResetEvent frame_finished;
#ifdef FLYCAST_ENABLE_NEURAL
// D-217 diagnostics: where the emulation thread waits for the renderer.
static unsigned emuWaitFrameFinishedCount=0,emuFramePeriodCount=0;
static std::uint64_t emuLastCycles=0;
static std::chrono::steady_clock::time_point emuLastQueued{};
#endif
static flycast::rend::neural::ProducerIdentityClock captureProducerClock;
void ResetCaptureProducerIdentity()
{
	captureProducerClock.Reset();
#ifdef FLYCAST_ENABLE_NEURAL
	flycast::rend::neural::ResetSourceSqWriters();
#endif
}

bool QueueRender(TA_context* ctx)
{
	verify(ctx != 0);
	
	bool skipFrame = !rend_is_enabled();
	if (!skipFrame)
	{
		RenderCount++;
		if (RenderCount % (config::SkipFrame + 1) != 0)
			skipFrame = true;
		else if (config::ThreadedRendering && rqueue != nullptr
				&& (config::AutoSkipFrame == 0 || (config::AutoSkipFrame == 1 && SH4FastEnough)))
			// The previous render hasn't completed yet so we wait.
			// If autoskipframe is enabled (normal level), we only do so if the CPU is running
			// fast enough over the last frames
		{
#ifdef FLYCAST_ENABLE_NEURAL
			flycast::rend::neural::RemakeCpuScope timing("emu-wait-frame-finished",0,emuWaitFrameFinishedCount);
#endif
			frame_finished.Wait();
		}
	}

	if (skipFrame || rqueue)
	{
		tactx_Recycle(ctx);
		if (rend_is_enabled())
			fskip++;
		return false;
	}
	// disable net rollbacks until the render thread has processed the frame
	rend_disable_rollback();
	frame_finished.Reset();
	verify(rqueue == nullptr);
	ctx->rend.captureProducer = {};
#ifdef FLYCAST_ENABLE_NEURAL
	const auto* asyncRemake = std::getenv("FLYCAST_REMAKE_ASYNC_CHANNEL");
	if ((config::NeuralCaptureFrames.get() > 0 || (asyncRemake && *asyncRemake)) && !ctx->rend.isRTT && !settings.platform.isNaomi2())
		ctx->rend.captureProducer = captureProducerClock.Stamp(sh4_sched_now64());
	for (TA_context* child = ctx; child != nullptr; child = child->nextContext)
		if (child->sourceObservations)
			child->sourceObservations->Seal(ctx->rend.captureProducer, child->sourceObservations->Size());
	// Diagnostic guest-frame digest (FLYCAST_REMAKE_GUEST_FRAME_DIGEST=1): a
	// hash of the raw TA bytes the guest submitted for this frame with its
	// producer cycle. It depends only on the emulated guest, never on an
	// observation, so two runs whose digests agree at the same cycle showed the
	// same guest scene; a difference elsewhere is then host-side bookkeeping.
	{
		static const bool guestDigest=[](){const char* v=std::getenv("FLYCAST_REMAKE_GUEST_FRAME_DIGEST");return v&&std::strcmp(v,"1")==0;}();
		if(guestDigest&&!ctx->rend.isRTT) {
			std::uint64_t h=1469598103934665603ull;std::size_t bytes=0;
			for (TA_context* child = ctx; child != nullptr; child = child->nextContext) {
				const u8* b=child->tad.thd_root;const u8* e=child->tad.End();
				for(const u8* p=b;p<e;++p){h^=*p;h*=1099511628211ull;}
				bytes+=std::size_t(e-b);
			}
			NOTICE_LOG(RENDERER,"Remake guest frame digest: producer=%llu cycle=%llu ta_bytes=%u digest=%016llx diagnostic=true",
				(unsigned long long)ctx->rend.captureProducer.ordinal,(unsigned long long)sh4_sched_now64(),unsigned(bytes),(unsigned long long)h);
		}
	}
	if(flycast::rend::neural::SourceObservationScopeRequested()) {
		static bool announced=false;
		if(!announced) {
			announced=true;
			const char* gates=std::getenv("FLYCAST_REMAKE_OBSERVATION_SCOPE_GATES");const char* parts=std::getenv("FLYCAST_REMAKE_OBSERVATION_SCOPE_PARTS");
			NOTICE_LOG(RENDERER,"Remake observation scope requested: gates=%s parts=%s (diagnostic knobs; absent means all)",gates?gates:"<all>",parts?parts:"<all>");
		}
	}
	if(flycast::rend::neural::SourceObservationScopeRequested()&&flycast::rend::neural::SourceObservationScopePartEnabled(flycast::rend::neural::ScopePartCtrl)) {
		using namespace flycast::rend::neural;
		auto& scope=sourceObservationScope;
		static unsigned scopeFrames=0;
		const auto event=scope.EndFrame(TakeSourceHookCount(sourceObservationFrameComplete),TakeSourceHookCount(sourceObservationFrameSubmissions));
		if(event!=SourceObservationScopeEvent::None){ApplySourceObservationScopeFlags();ForgetSourceObservationContributorCache();}
		if(event!=SourceObservationScopeEvent::None||++scopeFrames%600==0)
			NOTICE_LOG(RENDERER,"Remake observation scope: event=%s mode=%s regions=%u region_bytes=%u watched_blocks=%u/%u frames_observed=%u stable_frames=%u widened=%u narrowings=%u exhausted=%d scope=experimental",
				SourceObservationScopeEventName(event),SourceObservationScopeName(scope.mode),unsigned(scope.regions.size()),unsigned(scope.RegionBytes()),
				unsigned(sourceObservationWatchedBlocks),unsigned(sourceObservationBlocks.size()),scope.framesObserved,scope.stableFrames,scope.widened,scope.narrowings,int(scope.exhausted));
	}
	if(ctx->sourceObservations && ctx->rend.captureProducer.Available() && flycast::rend::neural::sourceTransforms) {
		const auto serial=flycast::rend::neural::sourceTransformSerial;
		NOTICE_LOG(RENDERER,"PVR transform arithmetic: producer=%llu observed=%llu rejected=%llu",
			static_cast<unsigned long long>(ctx->rend.captureProducer.ordinal),
			static_cast<unsigned long long>(flycast::rend::neural::sourceArithmeticSerial),
			static_cast<unsigned long long>(flycast::rend::neural::sourceArithmeticRejected));
		const auto& last=(*flycast::rend::neural::sourceTransforms)[serial%4096];
		NOTICE_LOG(RENDERER,"PVR direct transform stores: producer=%llu total=%llu",
			static_cast<unsigned long long>(ctx->rend.captureProducer.ordinal),static_cast<unsigned long long>(flycast::rend::neural::sourceDirectTransformStores));
		NOTICE_LOG(RENDERER,"PVR executed transform observation: producer=%llu serial=%llu last-pc=%08x correspondence=unproven",
			static_cast<unsigned long long>(ctx->rend.captureProducer.ordinal),static_cast<unsigned long long>(serial),last.pc);
	}
#endif
	rqueue = ctx;
#ifdef FLYCAST_ENABLE_NEURAL
	if(flycast::rend::neural::RemakeFrameTimingActive.load(std::memory_order_relaxed)) {
		const auto now=std::chrono::steady_clock::now();
		if(emuLastQueued.time_since_epoch().count()&&emuFramePeriodCount<600)
			if(const char* v=std::getenv("FLYCAST_REMAKE_CPU_TIMING");v&&std::strcmp(v,"1")==0) {
				++emuFramePeriodCount;
				// Per-hook cycle accounting costs about two time-stamp reads per hook
				// call (several ms per frame); it is a separate opt-in.
				if(const char* cycles=std::getenv("FLYCAST_REMAKE_HOOK_CYCLES");cycles&&std::strcmp(cycles,"1")==0)
					flycast::rend::neural::SourceHookCycleTiming=true;
				const double periodMs=std::chrono::duration<double,std::milli>(now-emuLastQueued).count();
				NOTICE_LOG(RENDERER,"Remake CPU scope: frame=0 stage=emu-frame-period elapsed_ms=%.6f includes_driver_wait=true diagnostic=true",periodMs);
				{
					using namespace flycast::rend::neural;
					const auto cyclesNow=SourceHookCycles::ReadCycles();
					const double cyclesPerMs=emuLastCycles&&periodMs>0?double(cyclesNow-emuLastCycles)/periodMs:0;
					emuLastCycles=cyclesNow;
					const auto ms=[&](std::uint64_t& cycles){const auto v=TakeSourceHookCount(cycles);return cyclesPerMs>0?double(v)/cyclesPerMs:0.0;};
					NOTICE_LOG(RENDERER,"Remake source hook cycles: stores_ms=%.3f sq_writes_ms=%.3f arithmetic_ms=%.3f ftrv_ms=%.3f block_entries_ms=%.3f boundaries_ms=%.3f copies_ms=%.3f reads_ms=%.3f cycles_per_ms=%.0f diagnostic=true",
						ms(SourceHookStoreCycles),ms(SourceHookSqWriteCycles),ms(SourceHookArithmeticCycles),ms(SourceHookFtrvCycles),
						ms(SourceHookBlockEntryCycles),ms(SourceHookBoundaryCycles),ms(SourceHookCopyCycles),ms(SourceHookReadCycles),cyclesPerMs);
				}
				NOTICE_LOG(RENDERER,"Remake source hook calls: stores=%llu sq_writes=%llu arithmetic=%llu ftrv=%llu block_entries=%llu invalidations=%llu boundaries=%llu diagnostic=true",
					(unsigned long long)flycast::rend::neural::TakeSourceHookCount(flycast::rend::neural::SourceHookStoreCalls),
					(unsigned long long)flycast::rend::neural::TakeSourceHookCount(flycast::rend::neural::SourceHookSqWriteCalls),
					(unsigned long long)flycast::rend::neural::TakeSourceHookCount(flycast::rend::neural::SourceHookArithmeticCalls),
					(unsigned long long)flycast::rend::neural::TakeSourceHookCount(flycast::rend::neural::SourceHookFtrvCalls),
					(unsigned long long)flycast::rend::neural::TakeSourceHookCount(flycast::rend::neural::SourceHookBlockEntryCalls),
					(unsigned long long)flycast::rend::neural::TakeSourceHookCount(flycast::rend::neural::SourceHookInvalidateCalls),
					(unsigned long long)flycast::rend::neural::TakeSourceHookCount(flycast::rend::neural::SourceHookBoundaryCalls));
			}
		// D-240 groundwork: per-block hook attribution, reported every 120
		// emulated frames (at most ten reports); counting only, diagnostic.
		if(const char* attribution=std::getenv("FLYCAST_REMAKE_HOOK_ATTRIBUTION");attribution&&std::strcmp(attribution,"1")==0) {
			using namespace flycast::rend::neural;
			SourceHookAttributionEnabled=true;
			static unsigned attributionFrames=0,attributionReports=0;
			if(++attributionFrames>=120&&attributionReports<10) {
				const auto summary=SummarizeSourceHookAttribution(sourceHookPcTallies,sourceHookContributingPcs,
					[](std::uint32_t pc){return SourceHookBlockOf(pc);},16);
				std::string kinds;
				for(unsigned k=0;k<SourceHookKindCount;++k)
					kinds+=std::string(SourceHookKindName(k))+"="+std::to_string(summary.total[k])+"/"+std::to_string(summary.fromContributingPcs[k])
						+"/"+std::to_string(summary.fromContributingBlocks[k])+" ";
				NOTICE_LOG(RENDERER,"Remake source hook attribution: frames=%u pcs=%u blocks=%u contributing_pcs=%u contributing_blocks=%u %s(total/from_contributing_pcs/from_contributing_blocks) diagnostic=true",
					attributionFrames,unsigned(summary.pcs),unsigned(summary.blocks),unsigned(summary.contributingPcs),unsigned(summary.contributingBlocks),kinds.c_str());
				for(std::size_t i=0;i<summary.topBlocks.size();++i) {
					const auto& b=summary.topBlocks[i];
					NOTICE_LOG(RENDERER,"Remake source hook block: rank=%u start=%08x calls=%llu contributing=%d stores=%llu reads=%llu arithmetic=%llu block_entries=%llu boundaries=%llu sq_writes=%llu ftrv=%llu diagnostic=true",
						unsigned(i+1),b.start,(unsigned long long)b.calls,int(b.contributing),
						(unsigned long long)b.byKind[unsigned(SourceHookKind::Store)],(unsigned long long)b.byKind[unsigned(SourceHookKind::Read)],
						(unsigned long long)b.byKind[unsigned(SourceHookKind::Arithmetic)],(unsigned long long)b.byKind[unsigned(SourceHookKind::BlockEntry)],
						(unsigned long long)b.byKind[unsigned(SourceHookKind::Boundary)],(unsigned long long)b.byKind[unsigned(SourceHookKind::SqWrite)],
						(unsigned long long)b.byKind[unsigned(SourceHookKind::Ftrv)]);
				}
				sourceHookPcTallies.clear();attributionFrames=0;++attributionReports;
			}
		}
		emuLastQueued=now;
	}
#endif

	return true;
}

TA_context* DequeueRender()
{
	if (rqueue != nullptr)
		FrameCount++;

	return rqueue;
}

void FinishRender(TA_context* ctx)
{
	if (ctx != nullptr)
	{
		verify(rqueue == ctx);
		rqueue = nullptr;
		tactx_Recycle(ctx);
	}
	frame_finished.Set();
}

static std::mutex mtx_pool;
using Lock = std::lock_guard<std::mutex>;

static std::vector<TA_context*> ctx_pool;
static std::vector<TA_context*> ctx_list;

TA_context *tactx_Alloc()
{
	TA_context *ctx = nullptr;
	{
		Lock _(mtx_pool);
		if (!ctx_pool.empty()) {
			ctx = ctx_pool.back();
			ctx_pool.pop_back();
		}
	}

	if (ctx == nullptr) {
		ctx = new TA_context();
		ctx->Alloc();
	}
	return ctx;
}

static void tactx_Recycle(TA_context* ctx)
{
	if (ctx->nextContext != nullptr)
		tactx_Recycle(ctx->nextContext);
	Lock _(mtx_pool);
	if (ctx_pool.size() > 3) {
		delete ctx;
	}
	else {
		ctx->Reset();
		ctx_pool.push_back(ctx);
	}
}

static TA_context *tactx_Find(u32 addr, bool allocnew)
{
	TA_context *oldCtx = nullptr;
	for (TA_context *ctx : ctx_list)
	{
		if (ctx->Address == addr) {
			ctx->lastFrameUsed = FrameCount;
			return ctx;
		}
		if (FrameCount - ctx->lastFrameUsed > 60)
			oldCtx = ctx;
	}

	if (allocnew)
	{
		TA_context *ctx;
		if (oldCtx != nullptr)
		{
			ctx = oldCtx;
			ctx->Reset();
		}
		else
		{
			ctx = tactx_Alloc();
			ctx_list.push_back(ctx);
		}
		ctx->Address = addr;
		ctx->lastFrameUsed = FrameCount;

		return ctx;
	}
	return nullptr;
}

TA_context *tactx_Pop(u32 addr)
{
	for (size_t i = 0; i < ctx_list.size(); i++)
	{
		if (ctx_list[i]->Address == addr)
		{
			TA_context *ctx = ctx_list[i];
			
			if (::ta_ctx == ctx)
				SetCurrentTARC(TACTX_NONE);

			ctx_list.erase(ctx_list.begin() + i);

			return ctx;
		}
	}
	return nullptr;
}

void tactx_Term()
{
	ResetCaptureProducerIdentity();
	if (ta_ctx != nullptr)
		SetCurrentTARC(TACTX_NONE);

	for (TA_context *ctx : ctx_list)
		delete ctx;
	ctx_list.clear();

	Lock _(mtx_pool);
	for (TA_context *ctx : ctx_pool)
		delete ctx;
	ctx_pool.clear();
}

const u32 NULL_CONTEXT = ~0u;

static void serializeContext(Serializer& ser, const TA_context *ctx)
{
	if (ser.dryrun())
	{
		// Maximum size: address, size, data
		ser.skip(4 + 4 + TA_DATA_SIZE);
		return;
	}
	if (ctx == nullptr)
	{
		ser << NULL_CONTEXT;
		return;
	}
	ser << ctx->Address;
	const tad_context& tad = ctx == ::ta_ctx ? ta_tad : ctx->tad;
	const u32 taSize = tad.thd_data - tad.thd_root;
	ser << taSize;
	ser.serialize(tad.thd_root, taSize);
}

static void deserializeContext(Deserializer& deser, TA_context **pctx)
{
	u32 address;
	deser >> address;
	if (address == NULL_CONTEXT)
	{
		*pctx = nullptr;
		return;
	}
	*pctx = tactx_Find(address, true);
	(*pctx)->rend.captureProducer = {};
	u32 size;
	deser >> size;
	tad_context& tad = (*pctx)->tad;
	deser.deserialize(tad.thd_root, size);
	tad.thd_data = tad.thd_root + size;
	if (deser.version() < Deserializer::V26)
	{
		u32 render_pass_count;
		deser >> render_pass_count;
		deser.skip(sizeof(u32) * render_pass_count);
	}
}

void SerializeTAContext(Serializer& ser)
{
	ser << (u32)ctx_list.size();
	int curCtx = -1;
	for (const auto& ctx : ctx_list)
	{
		if (ctx == ::ta_ctx)
			curCtx = (int)(&ctx - &ctx_list[0]);
		serializeContext(ser, ctx);
	}
	ser << curCtx;
}

void DeserializeTAContext(Deserializer& deser)
{
	ResetCaptureProducerIdentity();
	if (::ta_ctx != nullptr)
		SetCurrentTARC(TACTX_NONE);
	if (deser.version() >= Deserializer::V25)
	{
		u32 listSize;
		deser >> listSize;
		for (const auto& ctx : ctx_list)
			tactx_Recycle(ctx);
		ctx_list.clear();
		for (u32 i = 0; i < listSize; i++)
		{
			TA_context *ctx;
			deserializeContext(deser, &ctx);
		}
		int curCtx;
		deser >> curCtx;
		if (curCtx >= 0 && curCtx < (int)ctx_list.size())
			SetCurrentTARC(ctx_list[curCtx]->Address);
	}
	else
	{
		TA_context *ta_cur_ctx;
		deserializeContext(deser, &ta_cur_ctx);
		if (ta_cur_ctx != nullptr)
			SetCurrentTARC(ta_cur_ctx->Address);
		if (deser.version() >= Deserializer::V20)
			deserializeContext(deser, &ta_cur_ctx);
	}
}
