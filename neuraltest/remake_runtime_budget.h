// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <optional>
namespace neuraltest::remake {
// Whole-process diagnostic budget, never a GPU wait or throughput setting.
inline std::optional<unsigned> RemakeRuntimeBudget(bool diagnosticCapture,
 bool extendedReturn, long frames) noexcept {
 if(diagnosticCapture && !extendedReturn)return {};
 if(diagnosticCapture)return 300;
 return extendedReturn && frames>120 ? 120u : 30u;
}
}
