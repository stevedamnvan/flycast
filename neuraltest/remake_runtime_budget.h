// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <optional>
namespace neuraltest::remake {
inline std::optional<long> RemakeWorkerFrameLimit(bool worker, bool extendedReturn, long requested) noexcept {
 if(worker)return extendedReturn&&requested==660?std::optional<long>{10000}:std::nullopt;
 if(requested<1||requested>(extendedReturn?660:120))return {};
 return requested;
}
// Whole-process diagnostic budget, never a GPU wait or throughput setting.
inline std::optional<unsigned> RemakeRuntimeBudget(bool diagnosticCapture,
 bool extendedReturn, long frames) noexcept {
 if(diagnosticCapture && !extendedReturn)return {};
 if(diagnosticCapture)return 300;
 return extendedReturn && frames>120 ? 120u : 30u;
}
}
