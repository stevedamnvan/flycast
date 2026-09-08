// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <limits>

namespace flycast::rend::neural {

// Process-local diagnostic identity. Never serialized into emulated game state.
// Clock domain: SH4 scheduler cycles at accepted PVR queue submission.
struct ProducerIdentity {
	std::uint64_t epoch = 0;
	std::uint64_t ordinal = 0;
	std::uint64_t cycle = 0;
	bool Available() const noexcept { return epoch != 0 && ordinal != 0; }
};

// Owned by the emulation producer thread; consumers only receive copied stamps.
class ProducerIdentityClock {
public:
	void Reset() noexcept
	{
		if (epoch_ == (std::numeric_limits<std::uint64_t>::max)())
			exhausted_ = true;
		else
			++epoch_;
		ordinal_ = 0;
		lastCycle_ = 0;
	}
	ProducerIdentity Stamp(std::uint64_t cycle) noexcept
	{
		if (exhausted_) return {};
		if (ordinal_ != 0 && cycle < lastCycle_) Reset();
		if (ordinal_ == (std::numeric_limits<std::uint64_t>::max)()) Reset();
		if (exhausted_) return {};
		lastCycle_ = cycle;
		return {epoch_, ++ordinal_, cycle};
	}
	bool Owns(const ProducerIdentity& identity) const noexcept
	{
		return !exhausted_ && identity.Available() && identity.epoch == epoch_
			&& identity.ordinal <= ordinal_;
	}
private:
	std::uint64_t epoch_ = 1;
	std::uint64_t ordinal_ = 0;
	std::uint64_t lastCycle_ = 0;
	bool exhausted_ = false;
};

} // namespace flycast::rend::neural
