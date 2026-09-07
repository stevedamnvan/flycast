#pragma once

#include <string>

namespace flycast::rend::neural {

struct ExternalControlValues {
	int overall = 100;
	int structure = 100;
	int globalTone = 100;
	int localTone = 100;
	int style = 0;
	bool autoMask = true;
	bool uiCorrection = true;
};

// Builds only the companion's documented controls, never a shell command.
bool BuildExternalControlArguments(const ExternalControlValues& values,
	std::string& arguments);
struct ExternalControlResult {
	bool success = false;
	std::string message;
};
// Called on a UI-owned worker only after an explicit Apply click. The supplied
// companion owns backup/write/readback. No runtime binary is loaded here.
ExternalControlResult ApplyExternalControls(const std::string& helper,
	const std::string& config, const ExternalControlValues& values);

}
