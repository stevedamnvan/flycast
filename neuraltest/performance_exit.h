// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

inline constexpr int PerformanceExitCode(bool reportWritten, bool checksPassed,
                                        bool forcedTermination) noexcept
{
    return reportWritten && checksPassed && !forcedTermination ? 0 : 1;
}
