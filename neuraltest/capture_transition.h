// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "json/json.hpp"
#include <cstdint>

namespace neuraltest {
inline bool ValidCaptureSaveMarker(const nlohmann::json& marker,
	std::uint32_t after, std::uint32_t delay)
{
	if (!marker.is_object() || after == 0 || after > 10000 || delay == 0 || delay > 10000)
		return false;
	for (const char* key : {"completed", "in_memory", "saved", "loaded", "save_allowed"})
		if (!marker.contains(key) || !marker[key].is_boolean() || !marker[key].get<bool>())
			return false;
	for (const char* key : {"save_main_frame", "load_main_frame", "state_bytes", "schema"})
		if (!marker.contains(key) || !marker[key].is_number_unsigned()) return false;
	if (marker["schema"].get<std::uint64_t>() != 1 || marker["state_bytes"].get<std::uint64_t>() == 0)
		return false;
	const auto saved = marker["save_main_frame"].get<std::uint64_t>();
	const auto loaded = marker["load_main_frame"].get<std::uint64_t>();
	return saved >= after && loaded >= saved && loaded - saved >= delay;
}
}
