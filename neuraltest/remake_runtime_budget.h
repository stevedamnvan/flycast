// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <optional>
namespace neuraltest::remake {
inline std::optional<long> RemakeWorkerFrameLimit(bool worker, bool extendedReturn, long requested) noexcept {
 if(worker)return extendedReturn&&requested==660?std::optional<long>{10000}:std::nullopt;
 if(requested<1||requested>(extendedReturn?660:120))return {};
 return requested;
}
// Bounded wait for the next live source inside a session. A session worker
// tolerates 2D screens between rounds; ordinary helpers keep the 5 second
// handover bound. The runtime watchdog still bounds the whole process.
inline unsigned RemakeLiveIdleWaitMs(bool worker) noexcept { return worker?60000u:5000u; }
// Whole-process diagnostic budget, never a GPU wait or throughput setting. A
// session worker starts at the first publish and must outlive the host's
// explicit 420 second manual bound (LOG773: the 300 second budget ended a
// healthy session first); ordinary helpers keep their budgets.
inline std::optional<unsigned> RemakeRuntimeBudget(bool diagnosticCapture,
 bool extendedReturn, long frames, bool worker=false) noexcept {
 if(diagnosticCapture && !extendedReturn)return {};
 if(diagnosticCapture)return worker?420u:300u;
 return extendedReturn && frames>120 ? 120u : 30u;
}
}
