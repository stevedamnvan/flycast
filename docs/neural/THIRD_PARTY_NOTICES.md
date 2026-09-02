# Third-party and distribution note

Flycast is distributed under GPLv2 or later. NVIDIA NGX SDK materials are
separately licensed. This change does not commit or ship NVIDIA headers,
libraries, runtime DLLs, community add-ons, or interceptor binaries. NGX is an
OFF-by-default local build option; distribution of an NGX-enabled Flycast
binary requires maintainer review of both licenses. No definitive legal
compatibility claim is made here.

DLSS5-Feeder v0.10.0-beta.2 (`b60a8ffe4073dd65f8dbf804e47886607919b6b6`,
MIT) is consulted as a behavioral reference for public NGX call shape,
exception containment, and recovery. No source has been copied. The optional
NIGos bridge pattern will be consulted only if D3D11On12 fails an acceptance
gate; it is not currently a dependency.
