// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include "log/Log.h"

#ifdef FLYCAST_ENABLE_NEURAL
namespace flycast::rend::neural {
// Installed by the host before rendering starts. Standalone contract tools
// have no emulator logger and leave this unset.
using RemakeCpuReporter = void (*)(std::uint64_t, const char*, double);
inline RemakeCpuReporter ReportRemakeCpuScope=nullptr;
// Whole-frame scopes sample only once the remake lane has evaluated an image,
// so the bounded sample budget covers the lane, not the warmup.
inline std::atomic<bool> RemakeFrameTimingActive{false};
// D-217 diagnostics: how often the source-observation JIT hooks run per frame.
inline thread_local std::uint64_t SourceHookStoreCalls=0,SourceHookSqWriteCalls=0,SourceHookArithmeticCalls=0,SourceHookFtrvCalls=0,
 SourceHookBlockEntryCalls=0,SourceHookInvalidateCalls=0,SourceHookBoundaryCalls=0;
inline std::uint64_t TakeSourceHookCount(std::uint64_t& counter){const auto v=counter;counter=0;return v;}
// D-218 diagnostics: time-stamp-counter cycles spent inside each hook while
// the CPU timing diagnostic is on (plain thread-local accumulators; the
// emulation thread both accumulates and reports them).
inline bool SourceHookCycleTiming=false;
inline thread_local std::uint64_t SourceHookStoreCycles=0,SourceHookSqWriteCycles=0,SourceHookArithmeticCycles=0,SourceHookFtrvCycles=0,
 SourceHookBlockEntryCycles=0,SourceHookBoundaryCycles=0,SourceHookCopyCycles=0,SourceHookReadCycles=0;
struct SourceHookCycles {
	std::uint64_t& accumulator;std::uint64_t start;
	explicit SourceHookCycles(std::uint64_t& accumulator) noexcept:accumulator(accumulator),start(SourceHookCycleTiming?ReadCycles():0){}
	~SourceHookCycles(){if(start)accumulator+=ReadCycles()-start;}
	static std::uint64_t ReadCycles() noexcept {
#if defined(_MSC_VER)
		return __rdtsc();
#elif defined(__x86_64__)||defined(__i386__)
		return __builtin_ia32_rdtsc();
#else
		return 0;
#endif
	}
};
// Bounded, opt-in elapsed CPU diagnostics (FLYCAST_REMAKE_CPU_TIMING=1, at
// most 600 samples per stage). Includes driver blocking; never GPU time and
// never performance evidence.
class RemakeCpuScope {
	const char* label;
	std::uint64_t frame;
	bool enabled;
	std::chrono::steady_clock::time_point start;
public:
	RemakeCpuScope(const char* label, std::uint64_t frame, unsigned& count)
		: label(label), frame(frame), enabled(false) {
		const char* value=std::getenv("FLYCAST_REMAKE_CPU_TIMING");
		enabled=ReportRemakeCpuScope&&value&&std::strcmp(value,"1")==0&&count<600&&RemakeFrameTimingActive.load(std::memory_order_relaxed);
		if(enabled){++count;start=std::chrono::steady_clock::now();}
	}
	RemakeCpuScope(const RemakeCpuScope&)=delete;
	RemakeCpuScope& operator=(const RemakeCpuScope&)=delete;
	void End(){if(enabled){report();enabled=false;}}
	~RemakeCpuScope(){End();}
private:
	void report()const {
		const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
		ReportRemakeCpuScope(frame,label,ms);
	}
};
}
#endif
