// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

namespace flycast::rend::neural {

constexpr std::uint32_t EvidenceMarkerWidth = 32;
constexpr std::uint32_t EvidenceMarkerHeight = 32;

struct EvidenceMarkerOrigin {
	std::uint32_t x = 0;
	std::uint32_t y = 0;
};

constexpr EvidenceMarkerOrigin GetEvidenceMarkerOrigin(std::uint32_t width,
	std::uint32_t height, bool bottomRight) noexcept
{
	if (!bottomRight)
		return {};
	return {
		width > EvidenceMarkerWidth ? width - EvidenceMarkerWidth : 0,
		height > EvidenceMarkerHeight ? height - EvidenceMarkerHeight : 0,
	};
}

constexpr bool EvidenceMarkerIsCyan(std::uint32_t localX, std::uint32_t localY) noexcept
{
	return ((localX / 8) + (localY / 8)) % 2 != 0;
}

} // namespace flycast::rend::neural
