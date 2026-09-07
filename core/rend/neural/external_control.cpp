#include "external_control.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>
#include <windows.h>

namespace flycast::rend::neural {

bool BuildExternalControlArguments(const ExternalControlValues& v, std::string& args)
{
	args.clear();
	for (int n : {v.overall, v.structure, v.globalTone, v.localTone})
		if (n < 0 || n > 200) return false;
	if (v.style < 0 || v.style > 2) return false;
	std::ostringstream out;
	out.imbue(std::locale::classic());
	out << std::fixed << std::setprecision(2)
		<< " --apply --overall " << v.overall / 100.0
		<< " --structure " << v.structure / 100.0
		<< " --global-tone " << v.globalTone / 100.0
		<< " --local-tone " << v.localTone / 100.0
		<< " --style " << (v.style == 2 ? "cinematic" : v.style == 1 ? "natural" : "default")
		<< " --auto-mask " << (v.autoMask ? "on" : "off")
		<< " --ui-correction " << (v.uiCorrection ? "on" : "off");
	args = out.str();
	return true;
}

namespace {
struct Handle {
	HANDLE value = nullptr;
	~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
}

ExternalControlResult ApplyExternalControls(const std::string& helper,
	const std::string& config, const ExternalControlValues& values)
{
	std::string args;
	if (!BuildExternalControlArguments(values, args))
		return {false, "Invalid control value; nothing applied."};
	// Reject quote/control characters before quoting paths for CreateProcess.
	if (helper.empty() || config.empty()
		|| helper.find_first_of("\"\r\n") != std::string::npos
		|| config.find_first_of("\"\r\n") != std::string::npos)
		return {false, "Choose the companion executable and existing consumer configuration."};
	std::error_code ec;
	auto exe = std::filesystem::absolute(std::filesystem::u8path(helper), ec);
	if (ec || !std::filesystem::is_regular_file(exe, ec))
		return {false, "Companion executable is unavailable; nothing applied."};
	auto ini = std::filesystem::absolute(std::filesystem::u8path(config), ec);
	if (ec || !std::filesystem::is_regular_file(ini, ec))
		return {false, "Consumer configuration is unavailable; nothing applied."};
	if (ini.wstring().size() >= MAX_PATH)
		return {false, "This companion requires a configuration path shorter than 260 characters."};
	if (_wcsicmp(exe.filename().c_str(), L"flycast-nr-control.exe") != 0)
		return {false, "Select flycast-nr-control.exe, the external settings companion."};
	// The installed companion currently uses ANSI file APIs. Reject paths it
	// cannot represent instead of risking a write to a substituted pathname.
	BOOL substituted = FALSE;
	WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, ini.c_str(), -1,
		nullptr, 0, nullptr, &substituted);
	if (substituted)
		return {false, "This companion cannot represent the configuration path in the Windows code page."};
	Handle read, write, process, thread;
	SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
	if (!CreatePipe(&read.value, &write.value, &sa, 0)
		|| !SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0))
		return {false, "Cannot create companion output pipe; nothing applied."};
	std::wstring command = L"\"" + exe.wstring() + L"\" --config \""
		+ ini.wstring() + L"\"" + std::wstring(args.begin(), args.end());
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;
	startup.hStdOutput = startup.hStdError = write.value;
	PROCESS_INFORMATION info{};
	if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr, exe.parent_path().c_str(), &startup, &info))
		return {false, "Could not start companion; Windows error " + std::to_string(GetLastError())};
	process.value = info.hProcess;
	thread.value = info.hThread;
	CloseHandle(write.value);
	write.value = nullptr;
	std::string output;
	const auto start = GetTickCount64();
	for (;;)
	{
		if (GetTickCount64() - start > 10000)
		{
			TerminateProcess(process.value, 1);
			return {false, "Companion timed out. Configuration may be partially written; check its adjacent backup before retrying.\n" + output};
		}
		DWORD available = 0;
		if (PeekNamedPipe(read.value, nullptr, 0, nullptr, &available, nullptr) && available)
		{
			char buffer[2048];
			DWORD count = 0;
			if (ReadFile(read.value, buffer, (std::min)(available, DWORD(sizeof(buffer))), &count, nullptr)
				&& output.size() < 32768) output.append(buffer, count);
			continue;
		}
		if (WaitForSingleObject(process.value, 20) == WAIT_OBJECT_0) break;
	}
	// Drain output written immediately before process exit.
	char buffer[2048];
	DWORD count = 0, available = 0;
	while (PeekNamedPipe(read.value, nullptr, 0, nullptr, &available, nullptr) && available
		&& ReadFile(read.value, buffer, (std::min)(available, DWORD(sizeof(buffer))), &count, nullptr))
		if (output.size() < 32768) output.append(buffer, count);
	DWORD code = 1;
	GetExitCodeProcess(process.value, &code);
	const bool success = code == 0 && output.find("backup=") != std::string::npos
		&& output.find("status=requested-config-written") != std::string::npos;
	return {success, (success
		? "Settings saved and backup created. Exit and restart Flycast to load them. Active consumer settings remain unverified.\n"
		: "Apply failed. Check the companion output and adjacent configuration backup before retrying.\n") + output};
}

}
