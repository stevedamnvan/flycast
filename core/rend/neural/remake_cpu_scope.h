// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include "log/Log.h"

#ifdef FLYCAST_ENABLE_NEURAL
namespace flycast::rend::neural {
// Whole-frame scopes sample only once the remake lane has evaluated an image,
// so the bounded sample budget covers the lane, not the warmup.
inline std::atomic<bool> RemakeFrameTimingActive{false};
// D-217 diagnostics: how often the source-observation JIT hooks run per frame.
inline thread_local std::uint64_t SourceHookStoreCalls=0,SourceHookSqWriteCalls=0,SourceHookArithmeticCalls=0,SourceHookFtrvCalls=0,
 SourceHookBlockEntryCalls=0,SourceHookInvalidateCalls=0,SourceHookBoundaryCalls=0;
inline std::uint64_t TakeSourceHookCount(std::uint64_t& counter){const auto v=counter;counter=0;return v;}
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
		enabled=value&&std::strcmp(value,"1")==0&&count<600;
		if(enabled){++count;start=std::chrono::steady_clock::now();}
	}
	RemakeCpuScope(const RemakeCpuScope&)=delete;
	RemakeCpuScope& operator=(const RemakeCpuScope&)=delete;
	void End(){if(enabled){report();enabled=false;}}
	~RemakeCpuScope(){End();}
private:
	void report()const {
		const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
		NOTICE_LOG(RENDERER,"Remake CPU scope: frame=%llu stage=%s elapsed_ms=%.6f includes_driver_wait=true diagnostic=true",
			(unsigned long long)frame,label,ms);
	}
};
}
#endif
