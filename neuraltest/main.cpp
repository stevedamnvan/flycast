// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/neural/neural_stage.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

using Args = std::map<std::string, std::string>;

void Usage()
{
	std::cout <<
		"neuraltest render --fixture NAME --renderer dx11|dx11-oit --scale 1|4|8 --frames N --out DIR [--jitter on|off] [--warp]\n"
		"neuraltest determinism --fixture NAME --renderer dx11|dx11-oit [--runs 5] [--warp]\n"
		"neuraltest scaling --fixture NAME --renderer dx11|dx11-oit [--out DIR] [--warp]\n"
		"neuraltest depth-contract --api d3d11|d3d11on12 --out DIR\n"
		"neuraltest motion-contract --out DIR\n"
		"neuraltest color-contract --out DIR\n"
		"neuraltest disocclusion-contract --api d3d11|d3d11on12 --out DIR\n"
		"neuraltest transparency-contract --api d3d11|d3d11on12 --out DIR\n"
		"neuraltest overlay-contract --api d3d11|d3d11on12 --out DIR\n"
		"neuraltest depth|motion --in DIR\n"
		"neuraltest neural --in DIR --out DIR --backend passthrough|dlaa|dlaa-hook|dlss5-hook|sr --api d3d11|d3d12 [--mode quality|balanced|performance|ultra-performance] [--preset auto|j|k] [--depth-polarity inverted|normal] [--previous-in DIR|PNG --motion-x N --motion-y N] [--output-width N --output-height N] [--no-ngx] [--warp]\n"
		"neuraltest compare --a DIR|PNG --b DIR|PNG [--maxabs N] [--psnr N] [--edge-only]\n"
		"neuraltest native-parity --game PATH --enabled-flycast EXE --feature-off-flycast EXE --input-replay FILE --out DIR [--api d3d11|d3d11on12] [--renderer dx11|dx11-oit] [--frames 5] [--skip N] [--render-height N] [--timeout-ms N]\n"
		"neuraltest production-scaling --game PATH --flycast EXE --input-replay FILE --out DIR [--api d3d11|d3d11on12] [--renderer dx11|dx11-oit] [--frames 1] [--skip N] [--base-height 480] [--timeout-ms N]\n"
		"neuraltest capture --game PATH --frames N --skip M --out DIR [--flycast EXE] [--lane native|dlaa|sr-quality|dlss5] [--api d3d11|d3d11on12] [--renderer dx11|dx11-oit] [--preset auto|j|k] [--profile faithful|enhanced|photoreal] [--style auto|realistic|stylized|cel|racing|particles|sprite-2d|mixed-video] [--render-height N] [--feature-path DIR] [--input-replay yes|no] [--late-overlay-proof] [--proof-overlay fps|none] [--evidence-frames 0..480] [--evidence-start-frame N] [--evidence-mask zero|production] [--evidence-presentation marker|restored] [--inject none|create|evaluate|ring-busy|device-removed|runtime-unavailable] [--inject-count N] [--inject-after N] [--timeout-ms N]\n"
		"neuraltest capture-index --root DIR [--out HTML]\n"
		"neuraltest compare-captures --a DIR --b DIR --out JSON [--a-output external|public] [--b-output external|public]\n"
		"neuraltest confirm-external-capture --capture DIR --on-log FILE --on-host-log FILE --off-log FILE --off-host-log FILE --git-sha SHA\n"
		"neuraltest performance --game PATH --frames N --warmup N --out DIR [--flycast EXE] [--lane native|dlaa|sr-quality|dlss5] [--api d3d11|d3d11on12] [--renderer dx11|dx11-oit] [--preset auto|j|k] [--render-height N] [--feature-path DIR] [--input-replay yes|no] [--inject none|create|evaluate|ring-busy|device-removed|runtime-unavailable|seh-exception] [--inject-count N] [--inject-after N] [--transition none|resize-minimize-restore|fullscreen-roundtrip|focus-roundtrip] [--transition-delay-ms N] [--renderer-reinit-after N] [--renderer-switch-after N] [--surface-switch-after N] [--actual-device-removal-after N] [--game-reload-after N] [--savestate-roundtrip-after N] [--savestate-load-delay N] [--pause-roundtrip-after N] [--pause-duration N] [--mode-roundtrip-after N] [--mode-off-duration N] [--timeout-ms N]\n";
	std::cout << "neuraltest selftest\n";
}

bool ParseArgs(int argc, char **argv, int first, Args& args, std::string& error)
{
	for (int i = first; i < argc; ++i)
	{
		std::string key = argv[i];
		if (key.rfind("--", 0) != 0)
		{
			error = "unexpected positional argument: " + key;
			return false;
		}
		if (key == "--warp" || key == "--no-ngx" || key == "--edge-only"
			|| key == "--late-overlay-proof")
		{
			args[key] = "true";
			continue;
		}
		if (++i >= argc)
		{
			error = "missing value for " + key;
			return false;
		}
		args[key] = argv[i];
	}
	return true;
}

std::string Value(const Args& args, const std::string& name, const std::string& fallback = {})
{
	const auto it = args.find(name);
	return it == args.end() ? fallback : it->second;
}

bool Number(const Args& args, const std::string& name, std::uint32_t fallback,
	std::uint32_t& value, std::string& error)
{
	const auto text = Value(args, name);
	if (text.empty())
	{
		value = fallback;
		return true;
	}
	try
	{
		const auto parsed = std::stoul(text);
		if (parsed > std::numeric_limits<std::uint32_t>::max())
			throw std::out_of_range("uint32");
		value = static_cast<std::uint32_t>(parsed);
		return true;
	}
	catch (const std::exception&)
	{
		error = "invalid integer for " + name + ": " + text;
		return false;
	}
}

bool HashFileFnv64(const std::filesystem::path& path, std::uint64_t& hash)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return false;
	hash = 1469598103934665603ull;
	char buffer[64 * 1024];
	while (stream)
	{
		stream.read(buffer, sizeof(buffer));
		for (std::streamsize index = 0; index < stream.gcount(); ++index)
		{
			hash ^= static_cast<unsigned char>(buffer[index]);
			hash *= 1099511628211ull;
		}
	}
	return stream.eof();
}

bool HashFileFnv64Standard(const std::filesystem::path& path, std::uint64_t& hash)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return false;
	hash = 14695981039346656037ull;
	char buffer[64 * 1024];
	while (stream)
	{
		stream.read(buffer, sizeof(buffer));
		for (std::streamsize index = 0; index < stream.gcount(); ++index)
		{
			hash ^= static_cast<unsigned char>(buffer[index]);
			hash *= 1099511628211ull;
		}
	}
	return stream.eof();
}

std::string Hex64(std::uint64_t value)
{
	std::ostringstream stream;
	stream.imbue(std::locale::classic());
	stream << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << value;
	return stream.str();
}

bool Scalar(const Args& args, const std::string& name, float fallback,
	float& value, std::string& error)
{
	const auto text = Value(args, name);
	if (text.empty())
	{
		value = fallback;
		return true;
	}
	try
	{
		value = std::stof(text);
		if (!std::isfinite(value)) throw std::out_of_range("non-finite");
		return true;
	}
	catch (const std::exception&)
	{
		error = "invalid scalar for " + name + ": " + text;
		return false;
	}
}

bool RenderOne(const std::string& name, const neuraltest::RenderOptions& options,
	neuraltest::Fixture& fixture, neuraltest::RenderResult& result, std::string& error)
{
	return neuraltest::MakeFixture(name, options.frame, fixture, error) &&
		neuraltest::RenderFixture(fixture, options, result, error);
}

std::filesystem::path ResolveImage(const std::filesystem::path& input)
{
	if (!std::filesystem::is_directory(input))
		return input;
	for (const char *candidate : {"neural_output.png", "color.png", "prepared-color.png"})
		if (std::filesystem::exists(input / candidate))
			return input / candidate;
	return input / "color.png";
}

int RenderCommand(const Args& args)
{
	const auto name = Value(args, "--fixture");
	const auto out = Value(args, "--out");
	if (name.empty() || out.empty())
	{
		std::cerr << "render requires --fixture and --out\n";
		return 2;
	}
	std::string error;
	std::uint32_t scale = 1;
	std::uint32_t frames = 1;
	if (!Number(args, "--scale", 1, scale, error) || !Number(args, "--frames", 1, frames, error))
	{
		std::cerr << error << '\n';
		return 2;
	}
	for (std::uint32_t frame = 0; frame < frames; ++frame)
	{
		neuraltest::RenderOptions options;
		options.renderer = Value(args, "--renderer", "dx11");
		options.scale = scale;
		options.frame = frame;
		options.jitter = Value(args, "--jitter", "off") == "on";
		options.warp = args.count("--warp") != 0;
		neuraltest::Fixture fixture;
		neuraltest::RenderResult result;
		if (!RenderOne(name, options, fixture, result, error))
		{
			std::cerr << error << '\n';
			return 1;
		}
		std::ostringstream frameName;
		frameName << "frame-" << std::setw(4) << std::setfill('0') << frame;
		const auto destination = std::filesystem::path(out) / frameName.str();
		if (!neuraltest::WriteRenderPackage(destination, fixture, options, result, error))
		{
			std::cerr << error << '\n';
			return 1;
		}
		std::cout << name << " frame=" << frame << " " << result.color.width << 'x'
			<< result.color.height << " hash=0x" << std::hex << result.hash << std::dec
			<< " adapter=\"" << result.adapter << "\" driver=" << result.driver << '\n';
	}
	return 0;
}

int DeterminismCommand(const Args& args)
{
	const auto name = Value(args, "--fixture");
	if (name.empty())
	{
		std::cerr << "determinism requires --fixture\n";
		return 2;
	}
	std::string error;
	std::uint32_t runs = 5;
	if (!Number(args, "--runs", 5, runs, error) || runs < 2)
	{
		std::cerr << (error.empty() ? "--runs must be at least 2" : error) << '\n';
		return 2;
	}
	neuraltest::Image reference;
	std::uint64_t referenceHash = 0;
	for (std::uint32_t run = 0; run < runs; ++run)
	{
		neuraltest::RenderOptions options;
		options.renderer = Value(args, "--renderer", "dx11");
		options.warp = args.count("--warp") != 0;
		neuraltest::Fixture fixture;
		neuraltest::RenderResult result;
		if (!RenderOne(name, options, fixture, result, error))
		{
			std::cerr << error << '\n';
			return 1;
		}
		if (run == 0)
		{
			reference = result.color;
			referenceHash = result.hash;
			continue;
		}
		std::uint32_t pixels = 0;
		std::uint8_t maxDelta = 0;
		const double psnr = neuraltest::ComputePsnr(reference, result.color, pixels, maxDelta);
		std::cout << "pair=0:" << run << " equal=" << (referenceHash == result.hash ? "yes" : "no")
			<< " differing_pixels=" << pixels << " max_delta=" << static_cast<unsigned>(maxDelta)
			<< " psnr=" << (std::isinf(psnr) ? "inf" : std::to_string(psnr)) << '\n';
		if (referenceHash != result.hash)
			return 1;
	}
	std::cout << "deterministic runs=" << runs << " hash=0x" << std::hex << referenceHash
		<< std::dec << " scope=test-only-d3d11\n";
	return 0;
}

neuraltest::Image Nearest(const neuraltest::Image& source, std::uint32_t scale)
{
	neuraltest::Image output;
	output.width = source.width * scale;
	output.height = source.height * scale;
	output.rgba.resize(static_cast<std::size_t>(output.width) * output.height * 4);
	for (std::uint32_t y = 0; y < output.height; ++y)
		for (std::uint32_t x = 0; x < output.width; ++x)
			for (std::uint32_t c = 0; c < 4; ++c)
				output.rgba[(static_cast<std::size_t>(y) * output.width + x) * 4 + c] =
					source.rgba[(static_cast<std::size_t>(y / scale) * source.width + x / scale) * 4 + c];
	return output;
}

int ScalingCommand(const Args& args)
{
	const auto name = Value(args, "--fixture");
	if (name.empty())
	{
		std::cerr << "scaling requires --fixture\n";
		return 2;
	}
	const bool requiresUniqueSamples = name == "textured-checker-edge" || name == "rotate-quad";
	std::string error;
	neuraltest::Image native;
	for (std::uint32_t scale : {1u, 4u, 8u})
	{
		neuraltest::RenderOptions options;
		options.renderer = Value(args, "--renderer", "dx11");
		options.scale = scale;
		options.warp = args.count("--warp") != 0;
		neuraltest::Fixture fixture;
		neuraltest::RenderResult result;
		if (!RenderOne(name, options, fixture, result, error))
		{
			std::cerr << error << '\n';
			return 1;
		}
		if (scale == 1)
			native = result.color;
		else
		{
			const auto nearest = Nearest(native, scale);
			std::uint32_t pixels = 0;
			std::uint8_t maxDelta = 0;
			const double psnr = neuraltest::ComputePsnr(nearest, result.color, pixels, maxDelta);
			std::cout << "scale=" << scale << " genuine_samples=" << pixels << " max_delta="
				<< static_cast<unsigned>(maxDelta) << " psnr=" << psnr
				<< " verdict=" << (pixels != 0 ? "unique-samples" : "informational-no-difference")
				<< " scope=test-only-d3d11\n";
			if (requiresUniqueSamples && pixels == 0)
				return 1;
		}
		const auto out = Value(args, "--out");
		if (!out.empty() && !neuraltest::WritePng(std::filesystem::path(out) /
			("scale-" + std::to_string(scale) + ".png"), result.color, error))
		{
			std::cerr << error << '\n';
			return 1;
		}
	}
	return 0;
}

int NoDataCommand(const std::string& command, const Args& args)
{
	if (Value(args, "--in").empty())
	{
		std::cerr << command << " requires --in\n";
		return 2;
	}
	std::cout << command << ": no data (production renderer instrumentation is not implemented)\n";
	return 0;
}

#ifdef _WIN32
std::wstring QuoteWindowsArg(const std::wstring& value)
{
	std::wstring result = L"\"";
	std::size_t slashes = 0;
	for (const wchar_t c : value)
	{
		if (c == L'\\') { ++slashes; continue; }
		if (c == L'"')
		{
			result.append(slashes * 2 + 1, L'\\');
			result.push_back(c);
			slashes = 0;
			continue;
		}
		result.append(slashes, L'\\');
		slashes = 0;
		result.push_back(c);
	}
	result.append(slashes * 2, L'\\');
	result.push_back(L'"');
	return result;
}

struct CloseWindowsContext { DWORD processId = 0; };

BOOL CALLBACK CloseProcessWindows(HWND window, LPARAM parameter)
{
	auto *context = reinterpret_cast<CloseWindowsContext *>(parameter);
	DWORD processId = 0;
	GetWindowThreadProcessId(window, &processId);
	if (processId == context->processId)
		PostMessageW(window, WM_CLOSE, 0, 0);
	return TRUE;
}

struct FindWindowContext {
	DWORD processId = 0;
	HWND window = nullptr;
};

BOOL CALLBACK FindProcessWindow(HWND window, LPARAM parameter)
{
	auto *context = reinterpret_cast<FindWindowContext *>(parameter);
	DWORD processId = 0;
	GetWindowThreadProcessId(window, &processId);
	if (processId == context->processId && IsWindowVisible(window)
		&& GetWindow(window, GW_OWNER) == nullptr)
	{
		context->window = window;
		return FALSE;
	}
	return TRUE;
}

HWND FindProcessWindow(DWORD processId)
{
	FindWindowContext context{processId};
	EnumWindows(FindProcessWindow, reinterpret_cast<LPARAM>(&context));
	return context.window;
}

bool PostF11(HWND window)
{
	if (!window) return false;
	const LPARAM scan = static_cast<LPARAM>(MapVirtualKeyW(VK_F11, MAPVK_VK_TO_VSC)) << 16;
	return PostMessageW(window, WM_KEYDOWN, VK_F11, scan) != FALSE
		&& PostMessageW(window, WM_KEYUP, VK_F11, scan | (1ll << 30) | (1ll << 31)) != FALSE;
}

bool IsMonitorSizedWindow(HWND window)
{
	if (!window) return false;
	RECT windowRect{};
	MONITORINFO monitorInfo{};
	monitorInfo.cbSize = sizeof(monitorInfo);
	const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
	return GetWindowRect(window, &windowRect) != FALSE && monitor
		&& GetMonitorInfoW(monitor, &monitorInfo) != FALSE
		&& EqualRect(&windowRect, &monitorInfo.rcMonitor) != FALSE;
}

bool RunNativeParityProcess(const std::filesystem::path& flycast,
	const std::filesystem::path& game, const std::filesystem::path& output,
	const std::filesystem::path& inputReplay, int rendererValue,
	std::uint32_t frames, std::uint32_t skip, std::uint32_t renderHeight,
	std::uint32_t timeoutMs, bool neuralEnabled, bool d3d11On12, std::string& error)
{
	std::error_code ec;
	const auto completion = output / "native-parity-capture-complete.json";
	if (std::filesystem::exists(completion))
	{
		error = "parity output already contains a completed capture";
		return false;
	}
	std::filesystem::create_directories(output, ec);
	if (ec)
	{
		error = "cannot create parity output: " + ec.message();
		return false;
	}
	const auto scripts = flycast.parent_path() / "scripts";
	std::filesystem::create_directories(scripts, ec);
	if (ec)
	{
		error = "cannot create flycast replay directory: " + ec.message();
		return false;
	}
	const auto replayDestination = scripts / (game.stem().string() + ".input");
	if (std::filesystem::absolute(inputReplay) != std::filesystem::absolute(replayDestination))
	{
		std::filesystem::copy_file(inputReplay, replayDestination,
			std::filesystem::copy_options::overwrite_existing, ec);
		if (ec)
		{
			error = "cannot stage deterministic input replay: " + ec.message();
			return false;
		}
	}

	std::wstring config = L"config:pvr.rend=" + std::to_wstring(rendererValue)
		+ L",config:rend.Resolution=" + std::to_wstring(renderHeight)
		+ (neuralEnabled ? L",config:rend.NeuralMode=0,config:rend.NeuralCaptureFrames=0"
			L",config:rend.NeuralD3D12Surface=" + std::wstring(d3d11On12 ? L"yes" : L"no") : L"")
		+ L",config:rend.NativeParityCaptureDirectory='" + output.wstring() + L"'"
		+ L",config:rend.NativeParityCaptureFrames=" + std::to_wstring(frames)
		+ L",config:rend.NativeParityCaptureSkip=" + std::to_wstring(skip)
		+ L",record:replay_input=yes,log:LogToFile=yes";
	std::wstring commandLine = QuoteWindowsArg(flycast.wstring()) + L" -config "
		+ QuoteWindowsArg(config) + L" " + QuoteWindowsArg(game.wstring());
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
	mutableCommand.push_back(L'\0');
	if (!CreateProcessW(flycast.wstring().c_str(), mutableCommand.data(), nullptr, nullptr,
		FALSE, CREATE_NO_WINDOW, nullptr, flycast.parent_path().wstring().c_str(),
		&startup, &process))
	{
		error = "failed to launch flycast: win32=" + std::to_string(GetLastError());
		return false;
	}
	CloseHandle(process.hThread);
	const auto deadline = std::chrono::steady_clock::now()
		+ std::chrono::milliseconds(timeoutMs);
	bool complete = false;
	bool exitedEarly = false;
	while (std::chrono::steady_clock::now() < deadline)
	{
		if (std::filesystem::exists(completion)) { complete = true; break; }
		if (WaitForSingleObject(process.hProcess, 50) == WAIT_OBJECT_0)
		{
			exitedEarly = true;
			break;
		}
	}
	CloseWindowsContext closeContext{process.dwProcessId};
	EnumWindows(CloseProcessWindows, reinterpret_cast<LPARAM>(&closeContext));
	const bool cleanClose = WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0;
	if (!cleanClose)
	{
		TerminateProcess(process.hProcess, 4);
		WaitForSingleObject(process.hProcess, 5000);
	}
	CloseHandle(process.hProcess);
	if (!complete)
	{
		error = exitedEarly ? "flycast exited before parity capture completed"
			: "timed out waiting for parity capture";
		return false;
	}
	if (!cleanClose)
	{
		error = "flycast did not close cleanly after parity capture";
		return false;
	}
	return true;
}
#endif

int NativeParityCommand(const Args& args)
{
	const auto gameText = Value(args, "--game");
	const auto enabledText = Value(args, "--enabled-flycast");
	const auto offText = Value(args, "--feature-off-flycast");
	const auto replayText = Value(args, "--input-replay");
	const auto outputText = Value(args, "--out");
	if (gameText.empty() || enabledText.empty() || offText.empty()
		|| replayText.empty() || outputText.empty())
	{
		std::cerr << "native-parity requires --game, both flycast executables, "
			"--input-replay, and --out\n";
		return 2;
	}
	const auto renderer = Value(args, "--renderer", "dx11");
	const auto api = Value(args, "--api", "d3d11");
	if (renderer != "dx11" && renderer != "dx11-oit")
	{
		std::cerr << "--renderer must be dx11 or dx11-oit\n";
		return 2;
	}
	if (api != "d3d11" && api != "d3d11on12")
	{
		std::cerr << "--api must be d3d11 or d3d11on12\n";
		return 2;
	}
	std::string error;
	std::uint32_t frames = 5, skip = 0, renderHeight = 1080, timeoutMs = 120000;
	if (!Number(args, "--frames", 5, frames, error) || frames < 2 || frames > 240
		|| !Number(args, "--skip", 0, skip, error)
		|| !Number(args, "--render-height", 1080, renderHeight, error)
		|| !Number(args, "--timeout-ms", 120000, timeoutMs, error))
	{
		std::cerr << (error.empty() ? "native-parity requires 2..240 frames" : error) << '\n';
		return 2;
	}
	const auto game = std::filesystem::absolute(gameText);
	const auto enabled = std::filesystem::absolute(enabledText);
	const auto featureOff = std::filesystem::absolute(offText);
	const auto replay = std::filesystem::absolute(replayText);
	const auto output = std::filesystem::absolute(outputText);
	for (const auto& required : {game, enabled, featureOff, replay})
		if (!std::filesystem::is_regular_file(required))
		{
			std::cerr << "required native-parity input is unavailable: "
				<< required.string() << '\n';
			return 3;
		}
	if (enabled == featureOff)
	{
		std::cerr << "enabled and feature-off executables must be different files\n";
		return 2;
	}
	if (std::filesystem::exists(output / "native-parity-report.json"))
	{
		std::cerr << "native-parity output already contains a completed report\n";
		return 2;
	}
#ifndef _WIN32
	std::cerr << "production native parity launcher is currently available only on Windows\n";
	return 3;
#else
	const auto enabledOutput = output / "enabled-mode-off";
	const auto featureOffOutput = output / "feature-off";
	const int rendererValue = renderer == "dx11-oit" ? 6 : 2;
	if (!RunNativeParityProcess(enabled, game, enabledOutput, replay, rendererValue,
		frames, skip, renderHeight, timeoutMs, true, api == "d3d11on12", error)
		|| !RunNativeParityProcess(featureOff, game, featureOffOutput, replay, rendererValue,
			frames, skip, renderHeight, timeoutMs, false, false, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	auto readText = [](const std::filesystem::path& path, std::string& text) {
		std::ifstream stream(path);
		if (!stream) return false;
		text.assign(std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>());
		return !stream.bad();
	};
	std::string enabledMarker;
	std::string featureOffMarker;
	if (!readText(enabledOutput / "native-parity-capture-complete.json", enabledMarker)
		|| !readText(featureOffOutput / "native-parity-capture-complete.json", featureOffMarker))
	{
		std::cerr << "cannot read native parity completion diagnostics\n";
		return 1;
	}
	const std::vector<std::string> zeroActivity = {
		"\"neural_mode\": 0",
		"\"neural_instrumentation_enabled\": false",
		"\"neural_draw_records\": 0",
		"\"neural_previous_positions\": 0",
		"\"neural_input_layout_allocated\": false",
		"\"neural_export_resources_allocated\": false",
		"\"neural_guidance_replays\": 0",
		"\"neural_backend_resource_objects\": 0",
	};
	for (const auto& required : zeroActivity)
		if (enabledMarker.find(required) == std::string::npos
			|| featureOffMarker.find(required) == std::string::npos)
		{
			std::cerr << "disabled-build activity invariant failed: " << required << '\n';
			return 1;
		}
	if (enabledMarker.find("\"neural_compiled\": true") == std::string::npos
		|| featureOffMarker.find("\"neural_compiled\": false") == std::string::npos)
	{
		std::cerr << "native-parity executables do not represent enabled and feature-off builds\n";
		return 1;
	}
	const std::string expectedEnabledSurface = std::string("\"d3d11on12_surface\": ")
		+ (api == "d3d11on12" ? "true" : "false");
	if (enabledMarker.find(expectedEnabledSurface) == std::string::npos
		|| featureOffMarker.find("\"d3d11on12_surface\": false") == std::string::npos)
	{
		std::cerr << "native-parity did not exercise the requested enabled-build surface\n";
		return 1;
	}
	std::uint64_t replayHash = 0;
	if (!HashFileFnv64(replay, replayHash))
	{
		std::cerr << "cannot hash retained input replay\n";
		return 1;
	}
	std::error_code ec;
	std::filesystem::copy_file(replay, output / "input-replay.input",
		std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
	{
		std::cerr << "cannot retain input replay: " << ec.message() << '\n';
		return 1;
	}

	std::vector<std::string> hashes;
	hashes.reserve(frames);
	for (std::uint32_t frame = 0; frame < frames; ++frame)
	{
		std::ostringstream name;
		name << "frame-" << std::setw(6) << std::setfill('0') << frame << ".bgra8";
		const auto a = enabledOutput / name.str();
		const auto b = featureOffOutput / name.str();
		std::uint64_t aHash = 0, bHash = 0;
		const auto aSize = std::filesystem::file_size(a, ec);
		if (ec) { std::cerr << "cannot size enabled parity frame\n"; return 1; }
		const auto bSize = std::filesystem::file_size(b, ec);
		if (ec || aSize != bSize || !HashFileFnv64Standard(a, aHash)
			|| !HashFileFnv64Standard(b, bHash) || aHash != bHash)
		{
			std::cerr << "native parity mismatch at capture frame " << frame << '\n';
			return 1;
		}
		hashes.push_back(Hex64(aHash));
	}
	std::uint64_t wrongA = 0, wrongB = 0;
	const auto wrongPathA = enabledOutput / "frame-000000.bgra8";
	if (!HashFileFnv64Standard(wrongPathA, wrongA))
		return 1;
	std::ostringstream lastName;
	lastName << "frame-" << std::setw(6) << std::setfill('0') << (frames - 1) << ".bgra8";
	const auto wrongPathB = featureOffOutput / lastName.str();
	if (!HashFileFnv64Standard(wrongPathB, wrongB))
		return 1;
	if (wrongA == wrongB)
	{
		std::cerr << "wrong-frame negative control did not differ; choose a dynamic capture window\n";
		return 1;
	}
	std::ifstream wrongStreamA(wrongPathA, std::ios::binary);
	std::ifstream wrongStreamB(wrongPathB, std::ios::binary);
	std::uint64_t wrongDifferingPixels = 0;
	std::uint64_t wrongComparedPixels = 0;
	std::uint64_t wrongAbsoluteDelta = 0;
	std::uint32_t wrongMaxDelta = 0;
	std::array<unsigned char, 64 * 1024> wrongBytesA{};
	std::array<unsigned char, 64 * 1024> wrongBytesB{};
	while (wrongStreamA && wrongStreamB)
	{
		wrongStreamA.read(reinterpret_cast<char *>(wrongBytesA.data()), wrongBytesA.size());
		wrongStreamB.read(reinterpret_cast<char *>(wrongBytesB.data()), wrongBytesB.size());
		const auto countA = wrongStreamA.gcount();
		const auto countB = wrongStreamB.gcount();
		if (countA != countB || countA % 4 != 0)
		{
			std::cerr << "wrong-frame control byte contract differs\n";
			return 1;
		}
		for (std::streamsize offset = 0; offset < countA; offset += 4)
		{
			bool pixelDiffers = false;
			for (std::streamsize channel = 0; channel < 4; ++channel)
			{
				const auto delta = static_cast<std::uint32_t>(std::abs(
					static_cast<int>(wrongBytesA[static_cast<std::size_t>(offset + channel)])
					- static_cast<int>(wrongBytesB[static_cast<std::size_t>(offset + channel)])));
				wrongAbsoluteDelta += delta;
				wrongMaxDelta = std::max(wrongMaxDelta, delta);
				pixelDiffers = pixelDiffers || delta != 0;
			}
			wrongDifferingPixels += pixelDiffers ? 1 : 0;
			++wrongComparedPixels;
		}
	}
	const std::uint64_t minimumWrongPixels = std::max<std::uint64_t>(1000,
		wrongComparedPixels / 100);
	if (!wrongStreamA.eof() || !wrongStreamB.eof()
		|| wrongDifferingPixels < minimumWrongPixels)
	{
		std::cerr << "wrong-frame control is not materially different: pixels="
			<< wrongDifferingPixels << "/" << wrongComparedPixels << '\n';
		return 1;
	}
	const double wrongMeanAbsoluteDelta = wrongComparedPixels == 0 ? 0.0
		: static_cast<double>(wrongAbsoluteDelta)
			/ static_cast<double>(wrongComparedPixels * 4);

	std::ofstream report(output / "native-parity-report.json");
	report.imbue(std::locale::classic());
	report << "{\n  \"schema\": 1,\n"
		<< "  \"status\": \"pass\",\n"
		<< "  \"scope\": \"production-pvr-scene-color-before-neural-and-overlays\",\n"
		<< "  \"enabled_api\": \"" << api << "\",\n"
		<< "  \"feature_off_api\": \"d3d11\",\n"
		<< "  \"renderer\": \"" << renderer << "\",\n"
		<< "  \"frames\": " << frames << ",\n"
		<< "  \"skip\": " << skip << ",\n"
		<< "  \"render_height\": " << renderHeight << ",\n"
		<< "  \"input_replay_fnv64\": \"" << Hex64(replayHash) << "\",\n"
		<< "  \"exact_pairs\": [\n";
	for (std::uint32_t frame = 0; frame < frames; ++frame)
		report << "    {\"frame\": " << frame << ", \"fnv64\": \""
			<< hashes[frame] << "\"}" << (frame + 1 == frames ? "\n" : ",\n");
	report << "  ],\n"
		<< "  \"wrong_frame_control\": {\"equal\": false, \"enabled_frame\": 0, "
			"\"feature_off_frame\": " << (frames - 1)
		<< ", \"differing_pixels\": " << wrongDifferingPixels
		<< ", \"compared_pixels\": " << wrongComparedPixels
		<< ", \"max_delta\": " << wrongMaxDelta
		<< ", \"mean_absolute_delta\": " << wrongMeanAbsoluteDelta << "},\n"
		<< "  \"synchronous_developer_capture\": true,\n"
		<< "  \"clean_close\": true,\n"
		<< "  \"performance_eligible\": false\n}\n";
	if (!report)
	{
		std::cerr << "cannot write native parity report\n";
		return 1;
	}
	std::cout << "native-parity pass api=" << api << " renderer=" << renderer
		<< " exact_pairs=" << frames
		<< " wrong_frame_pixels=" << wrongDifferingPixels << '/' << wrongComparedPixels
		<< " replay_fnv64=" << Hex64(replayHash) << '\n';
	return 0;
#endif
}

int CaptureCommand(const Args& args)
{
	const auto gameText = Value(args, "--game");
	const auto outputText = Value(args, "--out");
	if (gameText.empty() || outputText.empty())
	{
		std::cerr << "capture requires --game and --out\n";
		return 2;
	}
	std::string error;
	std::uint32_t frames = 0, skip = 0, timeoutMs = 120000, renderHeight = 480,
		evidenceFrames = 0, evidenceStartFrame = 0;
	if (!Number(args, "--frames", 0, frames, error) || frames == 0 || frames > 240
		|| !Number(args, "--skip", 0, skip, error)
		|| !Number(args, "--render-height", 480, renderHeight, error)
		|| renderHeight < 120 || renderHeight > 8640
		|| !Number(args, "--evidence-frames", 0, evidenceFrames, error) || evidenceFrames > 480
		|| !Number(args, "--evidence-start-frame", 0, evidenceStartFrame, error)
		|| !Number(args, "--timeout-ms", 120000, timeoutMs, error) || timeoutMs < 1000)
	{
		std::cerr << (error.empty() ? "--frames must be 1..240, --evidence-frames 0..480, --render-height 120..8640, and --timeout-ms at least 1000" : error) << '\n';
		return 2;
	}
	const auto lane = Value(args, "--lane", "dlaa");
	const auto api = Value(args, "--api", "d3d11");
	const auto renderer = Value(args, "--renderer", "dx11");
	const auto preset = Value(args, "--preset", "auto");
	const auto injection = Value(args, "--inject", "none");
	const auto profile = Value(args, "--profile", "faithful");
	const auto style = Value(args, "--style", "auto");
	const auto evidenceMask = Value(args, "--evidence-mask", "zero");
	const auto evidencePresentation = Value(args, "--evidence-presentation", "marker");
	const auto inputReplay = Value(args, "--input-replay", "no");
	const bool lateOverlayProof = args.count("--late-overlay-proof") != 0;
	const auto proofOverlay = Value(args, "--proof-overlay", "fps");
	if (lane != "native" && lane != "dlaa" && lane != "sr-quality" && lane != "dlss5")
	{
		std::cerr << "--lane must be native, dlaa, sr-quality, or dlss5\n";
		return 2;
	}
	if (api != "d3d11" && api != "d3d11on12")
	{
		std::cerr << "--api must be d3d11 or d3d11on12\n";
		return 2;
	}
	if (evidenceFrames != 0 && (lane != "dlss5" || api != "d3d11on12"))
	{
		std::cerr << "evidence capture requires the dlss5 D3D11On12 lane\n";
		return 2;
	}
	if (evidenceMask != "zero" && evidenceMask != "production")
	{
		std::cerr << "--evidence-mask must be zero or production\n";
		return 2;
	}
	if (evidencePresentation != "marker" && evidencePresentation != "restored")
	{
		std::cerr << "--evidence-presentation must be marker or restored\n";
		return 2;
	}
	if (evidenceFrames == 0 && evidencePresentation != "marker")
	{
		std::cerr << "restored evidence presentation requires --evidence-frames\n";
		return 2;
	}
	if (inputReplay != "yes" && inputReplay != "no")
	{
		std::cerr << "--input-replay must be yes or no\n";
		return 2;
	}
	if (proofOverlay != "fps" && proofOverlay != "none")
	{
		std::cerr << "--proof-overlay must be fps or none\n";
		return 2;
	}
	if (!lateOverlayProof && args.count("--proof-overlay") != 0)
	{
		std::cerr << "--proof-overlay requires --late-overlay-proof\n";
		return 2;
	}
	if (renderer != "dx11" && renderer != "dx11-oit")
	{
		std::cerr << "--renderer must be dx11 or dx11-oit\n";
		return 2;
	}
	if (preset != "auto" && preset != "j" && preset != "k")
	{
		std::cerr << "--preset must be auto, j, or k\n";
		return 2;
	}
	if (injection != "none" && injection != "create" && injection != "evaluate"
		&& injection != "ring-busy" && injection != "device-removed"
		&& injection != "runtime-unavailable")
	{
		std::cerr << "invalid capture injection\n";
		return 2;
	}
	std::uint32_t injectionCount = 0;
	if (!Number(args, "--inject-count", injection == "none" ? 0
		: injection == "runtime-unavailable" ? 1 : 3,
		injectionCount, error) || injectionCount > 10000)
	{
		std::cerr << (error.empty() ? "invalid capture injection count" : error) << '\n';
		return 2;
	}
	if (injection == "none") injectionCount = 0;
	if (injection == "runtime-unavailable" && injectionCount != 1)
	{
		std::cerr << "runtime-unavailable injection count must be exactly 1\n";
		return 2;
	}
	std::uint32_t injectionAfter = 0;
	if (!Number(args, "--inject-after", 0, injectionAfter, error) || injectionAfter > 10000)
	{
		std::cerr << (error.empty() ? "invalid capture injection delay" : error) << '\n';
		return 2;
	}
	if (injection == "none") injectionAfter = 0;
	if (profile != "faithful" && profile != "enhanced" && profile != "photoreal")
	{
		std::cerr << "--profile must be faithful, enhanced, or photoreal\n";
		return 2;
	}
	static const std::vector<std::string> styles = {
		"auto", "realistic", "stylized", "cel", "racing", "particles", "sprite-2d", "mixed-video"
	};
	const auto styleIt = std::find(styles.begin(), styles.end(), style);
	if (styleIt == styles.end())
	{
		std::cerr << "invalid --style\n";
		return 2;
	}
	const auto game = std::filesystem::absolute(gameText);
	const auto output = std::filesystem::absolute(outputText);
	if (!std::filesystem::is_regular_file(game))
	{
		std::cerr << "game media is unavailable\n";
		return 3;
	}
	if (std::filesystem::exists(output / "capture-complete.json"))
	{
		std::cerr << "capture output already contains a completed run\n";
		return 2;
	}
#ifndef _WIN32
	std::cerr << "production capture launcher is currently available only on Windows\n";
	return 3;
#else
	wchar_t modulePath[32768]{};
	GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
	const std::filesystem::path defaultFlycast = std::filesystem::path(modulePath).parent_path()
		.parent_path() / "flycast.exe";
	const auto flycast = std::filesystem::absolute(Value(args, "--flycast",
		defaultFlycast.string()));
	if (!std::filesystem::is_regular_file(flycast))
	{
		std::cerr << "flycast executable is unavailable: " << flycast.string() << '\n';
		return 3;
	}
	std::uint64_t inputReplayHash = 0;
	std::uintmax_t inputReplayBytes = 0;
	if (inputReplay == "yes")
	{
		const auto source = flycast.parent_path() / "scripts" / (game.stem().string() + ".input");
		if (!std::filesystem::is_regular_file(source) || !HashFileFnv64(source, inputReplayHash))
		{
			std::cerr << "input replay file is unavailable: " << source.string() << '\n';
			return 3;
		}
		std::error_code ec;
		inputReplayBytes = std::filesystem::file_size(source, ec);
		if (ec)
		{
			std::cerr << "cannot size input replay: " << ec.message() << '\n';
			return 1;
		}
		std::filesystem::create_directories(output, ec);
		if (ec)
		{
			std::cerr << "cannot create capture output for input replay: " << ec.message() << '\n';
			return 1;
		}
		const auto retained = output / "input-replay.input";
		if (std::filesystem::exists(retained))
		{
			std::uint64_t retainedHash = 0;
			if (!HashFileFnv64(retained, retainedHash) || retainedHash != inputReplayHash)
			{
				std::cerr << "capture output contains a different retained input replay\n";
				return 2;
			}
		}
		else if (!std::filesystem::copy_file(source, retained, std::filesystem::copy_options::none, ec))
		{
			std::cerr << "cannot retain input replay: " << ec.message() << '\n';
			return 1;
		}
	}
	const int mode = lane == "native" ? 1 : lane == "dlaa" ? 2
		: lane == "sr-quality" ? 4 : 8;
	const int rendererValue = renderer == "dx11-oit" ? 6 : 2;
	const int presetValue = preset == "j" ? 10 : preset == "k" ? 11 : 0;
	const int injectionValue = injection == "create" ? 1 : injection == "evaluate" ? 2
		: injection == "ring-busy" ? 3 : injection == "device-removed" ? 4
		: injection == "runtime-unavailable" ? 5 : 0;
	const int profileValue = profile == "enhanced" ? 1 : profile == "photoreal" ? 2 : 0;
	const int styleValue = static_cast<int>(std::distance(styles.begin(), styleIt));
	std::wstring config = L"config:pvr.rend=" + std::to_wstring(rendererValue)
		+ L",config:rend.Resolution=" + std::to_wstring(renderHeight)
		+ L",config:rend.NeuralMode=" + std::to_wstring(mode)
		+ L",config:rend.NeuralD3D12Surface=" + (api == "d3d11on12" ? L"yes" : L"no")
		+ L",config:rend.NeuralMatchOutputResolution=yes"
		+ L",config:rend.NeuralCaptureDirectory='" + output.wstring() + L"'"
		+ L",config:rend.NeuralCaptureFrames=" + std::to_wstring(frames)
		+ L",config:rend.NeuralCaptureSkip=" + std::to_wstring(skip)
		+ L",config:rend.NeuralLateOverlayProof=" + (lateOverlayProof ? L"yes" : L"no")
		+ L",config:rend.ShowFPS="
		+ (lateOverlayProof && proofOverlay == "fps" ? L"yes" : L"no")
		+ L",config:rend.NeuralDlss5EvidenceCapture=" + (evidenceFrames != 0 ? L"yes" : L"no")
		+ L",config:rend.NeuralDlss5EvidenceCaptureFrames="
		+ std::to_wstring(evidenceFrames == 0 ? 1 : evidenceFrames)
		+ L",config:rend.NeuralDlss5EvidenceStartFrame=" + std::to_wstring(evidenceStartFrame)
		+ L",config:rend.NeuralDlss5EvidencePreserveMask="
		+ (evidenceFrames != 0 && evidenceMask == "production" ? L"yes" : L"no")
		+ L",config:rend.NeuralDlss5EvidencePresentMarker="
		+ (evidencePresentation == "marker" ? L"yes" : L"no")
		+ L",config:rend.NeuralFailureInjection=" + std::to_wstring(injectionValue)
		+ L",config:rend.NeuralFailureInjectionCount=" + std::to_wstring(injectionCount)
		+ L",config:rend.NeuralFailureInjectionAfter=" + std::to_wstring(injectionAfter)
		+ L",config:rend.NeuralDlssPreset=" + std::to_wstring(presetValue)
		+ L",config:rend.NeuralQualityProfile=" + std::to_wstring(profileValue)
		+ L",config:rend.NeuralStyleFamily=" + std::to_wstring(styleValue)
		+ L",record:replay_input=" + (inputReplay == "yes" ? L"yes" : L"no")
		+ L",log:LogToFile=yes";
	std::wstring commandLine = QuoteWindowsArg(flycast.wstring()) + L" -config "
		+ QuoteWindowsArg(config) + L" " + QuoteWindowsArg(game.wstring());

	const auto featurePath = Value(args, "--feature-path");
	std::wstring oldFeaturePath;
	bool restoreFeaturePath = false;
	if (!featurePath.empty())
	{
		const auto path = std::filesystem::absolute(featurePath);
		if (!std::filesystem::is_directory(path))
		{
			std::cerr << "public NGX feature path is unavailable\n";
			return 3;
		}
		wchar_t oldValue[32768]{};
		const DWORD oldLength = GetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH",
			oldValue, static_cast<DWORD>(std::size(oldValue)));
		if (oldLength > 0 && oldLength < std::size(oldValue)) oldFeaturePath = oldValue;
		restoreFeaturePath = true;
		SetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH", path.wstring().c_str());
	}

	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
	mutableCommand.push_back(L'\0');
	const BOOL launched = CreateProcessW(flycast.wstring().c_str(), mutableCommand.data(),
		nullptr, nullptr, FALSE, 0, nullptr, flycast.parent_path().wstring().c_str(),
		&startup, &process);
	if (restoreFeaturePath)
		SetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH",
			oldFeaturePath.empty() ? nullptr : oldFeaturePath.c_str());
	if (!launched)
	{
		std::cerr << "failed to launch flycast: win32=" << GetLastError() << '\n';
		return 1;
	}
	CloseHandle(process.hThread);
	const auto completion = output / "capture-complete.json";
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	bool complete = false;
	bool exitedEarly = false;
	while (std::chrono::steady_clock::now() < deadline)
	{
		if (std::filesystem::exists(completion)) { complete = true; break; }
		if (WaitForSingleObject(process.hProcess, 50) == WAIT_OBJECT_0)
		{
			exitedEarly = true;
			break;
		}
	}
	CloseWindowsContext closeContext{process.dwProcessId};
	EnumWindows(CloseProcessWindows, reinterpret_cast<LPARAM>(&closeContext));
	bool forcedTermination = false;
	if (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0)
	{
		TerminateProcess(process.hProcess, 4);
		WaitForSingleObject(process.hProcess, 5000);
		forcedTermination = true;
	}
	CloseHandle(process.hProcess);
	if (!complete)
	{
		std::cerr << (exitedEarly ? "flycast exited before capture completed"
			: "capture timed out before completion") << '\n';
		return 1;
	}
	std::uint32_t lateOverlayProofFiles = 0;
	if (lateOverlayProof)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(output))
		{
			if (!entry.is_regular_file() || entry.path().filename() != "late-overlay-proof.json")
				continue;
			std::ifstream proof(entry.path());
			std::ostringstream text;
			text << proof.rdbuf();
			if (!proof || text.str().find("\"passed\": true") == std::string::npos)
			{
				std::cerr << "late Flycast overlay pixel proof failed: "
					<< entry.path().string() << '\n';
				return 1;
			}
			++lateOverlayProofFiles;
		}
		if (lateOverlayProofFiles != frames)
		{
			std::cerr << "late Flycast overlay proof count differs from requested frames\n";
			return 1;
		}
	}
	std::ofstream launchReport(output / "capture-launch.json");
	launchReport << "{\n  \"schema\": 1,\n  \"lane\": \"" << lane
		<< "\",\n  \"api\": \"" << api << "\",\n  \"renderer\": \"" << renderer
		<< "\",\n  \"preset\": \"" << preset
		<< "\",\n  \"profile\": \"" << profile
		<< "\",\n  \"style\": \"" << style
		<< "\",\n  \"render_height\": " << renderHeight
		<< ",\n  \"evidence_frames\": " << evidenceFrames
		<< ",\n  \"evidence_start_frame\": " << evidenceStartFrame
		<< ",\n  \"evidence_mask\": \"" << evidenceMask << "\""
		<< ",\n  \"evidence_presentation\": \"" << evidencePresentation << "\""
		<< ",\n  \"input_replay_requested\": " << (inputReplay == "yes" ? "true" : "false")
		<< ",\n  \"input_replay_retained\": " << (inputReplay == "yes" ? "true" : "false")
		<< ",\n  \"input_replay_fnv64\": \"" << (inputReplay == "yes" ? Hex64(inputReplayHash) : "") << "\""
		<< ",\n  \"input_replay_bytes\": " << inputReplayBytes
		<< ",\n  \"late_overlay_proof_requested\": " << (lateOverlayProof ? "true" : "false")
		<< ",\n  \"late_overlay_source\": \"" << (lateOverlayProof ? proofOverlay : "none") << "\""
		<< ",\n  \"late_overlay_proof_frames\": " << lateOverlayProofFiles
		<< ",\n  \"failure_injection\": \"" << injection
		<< "\",\n  \"failure_injection_count\": " << injectionCount
		<< ",\n  \"failure_injection_after_accepted\": " << injectionAfter
		<< ",\n  \"requested_frames\": " << frames
		<< ",\n  \"skip\": " << skip
		<< ",\n  \"clean_window_close\": " << (forcedTermination ? "false" : "true")
		<< ",\n  \"media_path_recorded\": false\n}\n";
	std::cout << "capture complete frames=" << frames << " lane=" << lane
		<< " api=" << api << " renderer=" << renderer
		<< " injection=" << injection << ':' << injectionCount
		<< " clean_close=" << (forcedTermination ? "no" : "yes") << '\n';
	return launchReport ? 0 : 1;
#endif
}

int PerformanceCommand(const Args& args)
{
	const auto gameText = Value(args, "--game");
	const auto outputText = Value(args, "--out");
	if (gameText.empty() || outputText.empty())
	{
		std::cerr << "performance requires --game and --out\n";
		return 2;
	}
	std::string error;
	std::uint32_t frames = 0, warmup = 120, timeoutMs = 120000, renderHeight = 480;
	std::uint32_t rendererReinitAfter = 0, rendererSwitchAfter = 0;
	std::uint32_t surfaceSwitchAfter = 0, gameReloadAfter = 0;
	std::uint32_t saveStateAfter = 0, saveStateLoadDelay = 30;
	std::uint32_t pauseAfter = 0, pauseDuration = 30;
	std::uint32_t modeRoundtripAfter = 0, modeOffDuration = 30;
	std::uint32_t actualDeviceRemovalAfter = 0;
	if (!Number(args, "--frames", 0, frames, error) || frames == 0 || frames > 10000
		|| !Number(args, "--warmup", 120, warmup, error) || warmup > 10000
		|| !Number(args, "--render-height", 480, renderHeight, error)
		|| renderHeight < 120 || renderHeight > 8640
		|| !Number(args, "--renderer-reinit-after", 0, rendererReinitAfter, error)
		|| rendererReinitAfter > 10000
		|| !Number(args, "--renderer-switch-after", 0, rendererSwitchAfter, error)
		|| rendererSwitchAfter > 10000
		|| !Number(args, "--surface-switch-after", 0, surfaceSwitchAfter, error)
		|| surfaceSwitchAfter > 10000
		|| !Number(args, "--actual-device-removal-after", 0,
			actualDeviceRemovalAfter, error) || actualDeviceRemovalAfter > 10000
		|| !Number(args, "--game-reload-after", 0, gameReloadAfter, error)
		|| gameReloadAfter > 10000
		|| !Number(args, "--savestate-roundtrip-after", 0, saveStateAfter, error)
		|| saveStateAfter > 10000
		|| !Number(args, "--savestate-load-delay", 30, saveStateLoadDelay, error)
		|| saveStateLoadDelay == 0 || saveStateLoadDelay > 10000
		|| !Number(args, "--pause-roundtrip-after", 0, pauseAfter, error)
		|| pauseAfter > 10000
		|| !Number(args, "--pause-duration", 30, pauseDuration, error)
		|| pauseDuration == 0 || pauseDuration > 10000
		|| !Number(args, "--mode-roundtrip-after", 0, modeRoundtripAfter, error)
		|| modeRoundtripAfter > 10000
		|| !Number(args, "--mode-off-duration", 30, modeOffDuration, error)
		|| modeOffDuration == 0 || modeOffDuration > 10000
		|| !Number(args, "--timeout-ms", 120000, timeoutMs, error) || timeoutMs < 1000)
	{
		std::cerr << (error.empty() ? "invalid performance bounds" : error) << '\n';
		return 2;
	}
	const unsigned developerTransitionCount = (rendererReinitAfter != 0 ? 1u : 0u)
		+ (rendererSwitchAfter != 0 ? 1u : 0u) + (surfaceSwitchAfter != 0 ? 1u : 0u)
		+ (gameReloadAfter != 0 ? 1u : 0u) + (saveStateAfter != 0 ? 1u : 0u)
		+ (pauseAfter != 0 ? 1u : 0u) + (modeRoundtripAfter != 0 ? 1u : 0u)
		+ (actualDeviceRemovalAfter != 0 ? 1u : 0u);
	if (developerTransitionCount > 1)
	{
		std::cerr << "developer renderer transitions are mutually exclusive\n";
		return 2;
	}
	const auto lane = Value(args, "--lane", "dlaa");
	const auto api = Value(args, "--api", "d3d11");
	const auto renderer = Value(args, "--renderer", "dx11");
	const auto preset = Value(args, "--preset", "auto");
	const auto injection = Value(args, "--inject", "none");
	const auto transition = Value(args, "--transition", "none");
	const auto inputReplay = Value(args, "--input-replay", "no");
	if (lane != "native" && lane != "dlaa" && lane != "sr-quality" && lane != "dlss5")
	{
		std::cerr << "invalid performance lane\n";
		return 2;
	}
	if (modeRoundtripAfter != 0 && lane == "native")
	{
		std::cerr << "neural mode round trip requires a neural lane\n";
		return 2;
	}
	if (modeRoundtripAfter == 0 && args.find("--mode-off-duration") != args.end())
	{
		std::cerr << "mode-off duration requires a mode round trip\n";
		return 2;
	}
	if (modeRoundtripAfter != 0
		&& (modeRoundtripAfter <= warmup + 12
			|| static_cast<std::uint64_t>(modeRoundtripAfter) + modeOffDuration + 24
				>= static_cast<std::uint64_t>(warmup) + frames))
	{
		std::cerr << "mode round trip must leave measured neural samples before and after the off interval\n";
		return 2;
	}
	if (api != "d3d11" && api != "d3d11on12")
	{
		std::cerr << "invalid performance API\n";
		return 2;
	}
	if (actualDeviceRemovalAfter != 0 && api != "d3d11on12")
	{
		std::cerr << "actual device removal requires d3d11on12\n";
		return 2;
	}
	if (actualDeviceRemovalAfter != 0 && lane == "native")
	{
		std::cerr << "actual device removal requires a neural lane\n";
		return 2;
	}
	if (renderer != "dx11" && renderer != "dx11-oit")
	{
		std::cerr << "invalid performance renderer\n";
		return 2;
	}
	if (preset != "auto" && preset != "j" && preset != "k")
	{
		std::cerr << "invalid performance preset\n";
		return 2;
	}
	if (inputReplay != "yes" && inputReplay != "no")
	{
		std::cerr << "--input-replay must be yes or no\n";
		return 2;
	}
	if (injection != "none" && injection != "create" && injection != "evaluate"
		&& injection != "ring-busy" && injection != "device-removed"
		&& injection != "runtime-unavailable" && injection != "seh-exception")
	{
		std::cerr << "invalid performance injection\n";
		return 2;
	}
	if (transition != "none" && transition != "resize-minimize-restore"
		&& transition != "fullscreen-roundtrip" && transition != "focus-roundtrip")
	{
		std::cerr << "invalid performance transition\n";
		return 2;
	}
	if (actualDeviceRemovalAfter != 0
		&& (injection != "none" || transition != "none"))
	{
		std::cerr << "actual device removal cannot overlap another failure or window transition\n";
		return 2;
	}
	std::uint32_t injectionCount = 0;
	if (!Number(args, "--inject-count", injection == "none" ? 0
		: injection == "runtime-unavailable" ? 1 : 3,
		injectionCount, error) || injectionCount > 10000)
	{
		std::cerr << (error.empty() ? "invalid performance injection count" : error) << '\n';
		return 2;
	}
	if (injection == "none") injectionCount = 0;
	if (injection == "runtime-unavailable" && injectionCount != 1)
	{
		std::cerr << "runtime-unavailable injection count must be exactly 1\n";
		return 2;
	}
	std::uint32_t injectionAfter = 0;
	if (!Number(args, "--inject-after", 0, injectionAfter, error) || injectionAfter > 10000)
	{
		std::cerr << (error.empty() ? "invalid performance injection delay" : error) << '\n';
		return 2;
	}
	if (injection == "none") injectionAfter = 0;
	std::uint32_t transitionDelayMs = 500;
	if (!Number(args, "--transition-delay-ms", 500, transitionDelayMs, error)
		|| transitionDelayMs > 60000)
	{
		std::cerr << (error.empty() ? "invalid performance transition delay" : error) << '\n';
		return 2;
	}
	if (transition == "none") transitionDelayMs = 0;
	const auto game = std::filesystem::absolute(gameText);
	const auto output = std::filesystem::absolute(outputText);
	if (!std::filesystem::is_regular_file(game))
	{
		std::cerr << "game media is unavailable\n";
		return 3;
	}
	if (std::filesystem::exists(output / "performance-complete.json"))
	{
		std::cerr << "performance output already contains a completed run\n";
		return 2;
	}
	if (rendererReinitAfter != 0
		&& std::filesystem::exists(output / "renderer-reinit-complete.json"))
	{
		std::cerr << "performance output already contains a renderer-reinit marker\n";
		return 2;
	}
	if (rendererSwitchAfter != 0
		&& std::filesystem::exists(output / "renderer-switch-complete.json"))
	{
		std::cerr << "performance output already contains a renderer-switch marker\n";
		return 2;
	}
	if (surfaceSwitchAfter != 0
		&& std::filesystem::exists(output / "surface-switch-complete.json"))
	{
		std::cerr << "performance output already contains a surface-switch marker\n";
		return 2;
	}
	if (actualDeviceRemovalAfter != 0
		&& std::filesystem::exists(output / "actual-device-removal-complete.json"))
	{
		std::cerr << "performance output already contains an actual-device-removal marker\n";
		return 2;
	}
	if (gameReloadAfter != 0
		&& std::filesystem::exists(output / "game-reload-complete.json"))
	{
		std::cerr << "performance output already contains a game-reload marker\n";
		return 2;
	}
	if (saveStateAfter != 0
		&& std::filesystem::exists(output / "savestate-roundtrip-complete.json"))
	{
		std::cerr << "performance output already contains a savestate marker\n";
		return 2;
	}
	if (pauseAfter != 0
		&& std::filesystem::exists(output / "pause-roundtrip-complete.json"))
	{
		std::cerr << "performance output already contains a pause marker\n";
		return 2;
	}
	if (modeRoundtripAfter != 0
		&& std::filesystem::exists(output / "neural-mode-roundtrip-complete.json"))
	{
		std::cerr << "performance output already contains a neural-mode marker\n";
		return 2;
	}
#ifndef _WIN32
	std::cerr << "production performance launcher is currently available only on Windows\n";
	return 3;
#else
	wchar_t modulePath[32768]{};
	GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(std::size(modulePath)));
	const std::filesystem::path defaultFlycast = std::filesystem::path(modulePath).parent_path()
		.parent_path() / "flycast.exe";
	const auto flycast = std::filesystem::absolute(Value(args, "--flycast",
		defaultFlycast.string()));
	if (!std::filesystem::is_regular_file(flycast))
	{
		std::cerr << "flycast executable is unavailable: " << flycast.string() << '\n';
		return 3;
	}
	std::uint64_t inputReplayHash = 0;
	std::uintmax_t inputReplayBytes = 0;
	if (inputReplay == "yes")
	{
		const auto source = flycast.parent_path() / "scripts" / (game.stem().string() + ".input");
		if (!std::filesystem::is_regular_file(source) || !HashFileFnv64(source, inputReplayHash))
		{
			std::cerr << "input replay file is unavailable: " << source.string() << '\n';
			return 3;
		}
		std::error_code ec;
		inputReplayBytes = std::filesystem::file_size(source, ec);
		if (ec)
		{
			std::cerr << "cannot size input replay: " << ec.message() << '\n';
			return 1;
		}
		std::filesystem::create_directories(output, ec);
		if (ec)
		{
			std::cerr << "cannot create performance output for input replay: " << ec.message() << '\n';
			return 1;
		}
		const auto retained = output / "input-replay.input";
		if (std::filesystem::exists(retained))
		{
			std::uint64_t retainedHash = 0;
			if (!HashFileFnv64(retained, retainedHash) || retainedHash != inputReplayHash)
			{
				std::cerr << "performance output contains a different retained input replay\n";
				return 2;
			}
		}
		else if (!std::filesystem::copy_file(source, retained, std::filesystem::copy_options::none, ec))
		{
			std::cerr << "cannot retain input replay: " << ec.message() << '\n';
			return 1;
		}
	}
	const int mode = lane == "native" ? 0 : lane == "dlaa" ? 2
		: lane == "sr-quality" ? 4 : 8;
	const int rendererValue = renderer == "dx11-oit" ? 6 : 2;
	const int presetValue = preset == "j" ? 10 : preset == "k" ? 11 : 0;
	const int injectionValue = injection == "create" ? 1 : injection == "evaluate" ? 2
		: injection == "ring-busy" ? 3 : injection == "device-removed" ? 4
		: injection == "runtime-unavailable" ? 5 : injection == "seh-exception" ? 6 : 0;
	std::wstring config = L"config:pvr.rend=" + std::to_wstring(rendererValue)
		+ L",config:rend.Resolution=" + std::to_wstring(renderHeight)
		+ L",config:rend.NeuralMode=" + std::to_wstring(mode)
		+ L",config:rend.NeuralD3D12Surface=" + (api == "d3d11on12" ? L"yes" : L"no")
		+ L",config:rend.NeuralMatchOutputResolution=yes"
		+ L",config:rend.NeuralCaptureFrames=0"
		+ L",config:rend.NeuralDlss5EvidenceCapture=no"
		+ L",config:rend.NeuralPerformanceDirectory='" + output.wstring() + L"'"
		+ L",config:rend.NeuralPerformanceFrames=" + std::to_wstring(frames)
		+ L",config:rend.NeuralPerformanceWarmup=" + std::to_wstring(warmup)
		+ L",config:rend.NeuralFailureInjection=" + std::to_wstring(injectionValue)
		+ L",config:rend.NeuralFailureInjectionCount=" + std::to_wstring(injectionCount)
		+ L",config:rend.NeuralFailureInjectionAfter=" + std::to_wstring(injectionAfter)
		+ L",config:rend.NeuralRendererReinitAfter=" + std::to_wstring(rendererReinitAfter)
		+ L",config:rend.NeuralRendererSwitchAfter=" + std::to_wstring(rendererSwitchAfter)
		+ L",config:rend.NeuralSurfaceSwitchAfter=" + std::to_wstring(surfaceSwitchAfter)
		+ L",config:rend.NeuralActualDeviceRemovalAfter="
		+ std::to_wstring(actualDeviceRemovalAfter)
		+ L",config:rend.NeuralGameReloadAfter=" + std::to_wstring(gameReloadAfter)
		+ L",config:rend.NeuralSaveStateAfter=" + std::to_wstring(saveStateAfter)
		+ L",config:rend.NeuralSaveStateLoadDelay=" + std::to_wstring(saveStateLoadDelay)
		+ L",config:rend.NeuralPauseAfter=" + std::to_wstring(pauseAfter)
		+ L",config:rend.NeuralPauseDuration=" + std::to_wstring(pauseDuration)
		+ L",config:rend.NeuralModeRoundtripAfter=" + std::to_wstring(modeRoundtripAfter)
		+ L",config:rend.NeuralModeOffDuration=" + std::to_wstring(modeOffDuration)
		+ L",config:rend.NeuralDlssPreset=" + std::to_wstring(presetValue)
		+ L",record:replay_input=" + (inputReplay == "yes" ? L"yes" : L"no")
		+ L",log:LogToFile=yes";
	std::wstring commandLine = QuoteWindowsArg(flycast.wstring()) + L" -config "
		+ QuoteWindowsArg(config) + L" " + QuoteWindowsArg(game.wstring());

	const auto featurePath = Value(args, "--feature-path");
	std::wstring oldFeaturePath;
	bool restoreFeaturePath = false;
	if (!featurePath.empty())
	{
		const auto path = std::filesystem::absolute(featurePath);
		if (!std::filesystem::is_directory(path))
		{
			std::cerr << "public NGX feature path is unavailable\n";
			return 3;
		}
		wchar_t oldValue[32768]{};
		const DWORD oldLength = GetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH",
			oldValue, static_cast<DWORD>(std::size(oldValue)));
		if (oldLength > 0 && oldLength < std::size(oldValue)) oldFeaturePath = oldValue;
		restoreFeaturePath = true;
		SetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH", path.wstring().c_str());
	}
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
	mutableCommand.push_back(L'\0');
	const BOOL launched = CreateProcessW(flycast.wstring().c_str(), mutableCommand.data(),
		nullptr, nullptr, FALSE, 0, nullptr, flycast.parent_path().wstring().c_str(),
		&startup, &process);
	if (restoreFeaturePath)
		SetEnvironmentVariableW(L"FLYCAST_NGX_FEATURE_PATH",
			oldFeaturePath.empty() ? nullptr : oldFeaturePath.c_str());
	if (!launched)
	{
		std::cerr << "failed to launch flycast: win32=" << GetLastError() << '\n';
		return 1;
	}
	CloseHandle(process.hThread);
	const auto completion = output / "performance-complete.json";
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	bool complete = false, exitedEarly = false;
	HWND transitionWindow = nullptr;
	RECT originalWindowRect{};
	bool originalWindowRectValid = false;
	int transitionStep = transition == "none" ? 5 : 0;
	bool resizeOutPassed = transition != "resize-minimize-restore";
	bool minimizePassed = transition != "resize-minimize-restore";
	bool restorePassed = transition != "resize-minimize-restore";
	bool resizeBackPassed = transition != "resize-minimize-restore";
	bool fullscreenEnterRequested = transition != "fullscreen-roundtrip";
	bool fullscreenEnterObserved = transition != "fullscreen-roundtrip";
	bool fullscreenExitRequested = transition != "fullscreen-roundtrip";
	bool fullscreenExitObserved = transition != "fullscreen-roundtrip";
	bool fullscreenRectRestored = transition != "fullscreen-roundtrip";
	bool focusTargetRequested = transition != "focus-roundtrip";
	bool focusTargetObserved = transition != "focus-roundtrip";
	bool focusLossRequested = transition != "focus-roundtrip";
	bool focusLossObserved = transition != "focus-roundtrip";
	bool focusRestoreRequested = transition != "focus-roundtrip";
	bool focusRestoreObserved = transition != "focus-roundtrip";
	HWND focusControlWindow = nullptr;
	auto nextTransition = std::chrono::steady_clock::time_point{};
	while (std::chrono::steady_clock::now() < deadline)
	{
		const auto now = std::chrono::steady_clock::now();
		if (transitionStep < 5)
		{
			if (!transitionWindow)
			{
				transitionWindow = FindProcessWindow(process.dwProcessId);
				if (transitionWindow)
				{
					originalWindowRectValid = GetWindowRect(transitionWindow,
						&originalWindowRect) != FALSE;
					nextTransition = now + std::chrono::milliseconds(transitionDelayMs);
				}
			}
			else if (now >= nextTransition)
			{
				if (!IsWindow(transitionWindow))
					transitionWindow = FindProcessWindow(process.dwProcessId);
				const int originalWidth = originalWindowRect.right - originalWindowRect.left;
				const int originalHeight = originalWindowRect.bottom - originalWindowRect.top;
				if (transition == "focus-roundtrip")
				{
					switch (transitionStep)
					{
					case 0:
						focusTargetRequested = transitionWindow
							&& SetForegroundWindow(transitionWindow) != FALSE;
						break;
					case 1:
						focusTargetObserved = focusTargetRequested
							&& GetForegroundWindow() == transitionWindow;
						focusControlWindow = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC",
							L"Flycast focus lifecycle control", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
							0, 0, 160, 90, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
						focusLossRequested = focusControlWindow
							&& SetForegroundWindow(focusControlWindow) != FALSE;
						break;
					case 2:
						focusLossObserved = focusLossRequested && focusControlWindow
							&& GetForegroundWindow() == focusControlWindow
							&& GetForegroundWindow() != transitionWindow;
						focusRestoreRequested = focusLossObserved && transitionWindow
							&& SetForegroundWindow(transitionWindow) != FALSE;
						break;
					case 3:
						focusRestoreObserved = focusRestoreRequested
							&& GetForegroundWindow() == transitionWindow;
						break;
					case 4:
						focusRestoreObserved = focusRestoreObserved && transitionWindow
							&& IsWindowVisible(transitionWindow) != FALSE
							&& IsIconic(transitionWindow) == FALSE;
						if (focusControlWindow)
						{
							DestroyWindow(focusControlWindow);
							focusControlWindow = nullptr;
						}
						break;
					}
				}
				else if (transition == "fullscreen-roundtrip")
				{
					switch (transitionStep)
					{
					case 0:
						fullscreenEnterRequested = originalWindowRectValid
							&& PostF11(transitionWindow);
						break;
					case 1:
						fullscreenEnterObserved = fullscreenEnterRequested
							&& IsMonitorSizedWindow(transitionWindow);
						break;
					case 2:
						fullscreenExitRequested = PostF11(transitionWindow);
						break;
					case 3:
						fullscreenExitObserved = fullscreenExitRequested && transitionWindow
							&& IsWindowVisible(transitionWindow) != FALSE
							&& !IsMonitorSizedWindow(transitionWindow);
						break;
					case 4:
					{
						RECT finalRect{};
						fullscreenRectRestored = fullscreenExitObserved && transitionWindow
							&& GetWindowRect(transitionWindow, &finalRect) != FALSE
							&& EqualRect(&finalRect, &originalWindowRect) != FALSE;
						break;
					}
					}
				}
				else switch (transitionStep)
				{
				case 0:
					resizeOutPassed = originalWindowRectValid && SetWindowPos(transitionWindow,
						nullptr, 0, 0, originalWidth + 160, originalHeight + 90,
						SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
					break;
				case 1:
				{
					RECT expandedRect{};
					resizeOutPassed = resizeOutPassed && transitionWindow
						&& GetWindowRect(transitionWindow, &expandedRect) != FALSE
						&& expandedRect.right - expandedRect.left == originalWidth + 160
						&& expandedRect.bottom - expandedRect.top == originalHeight + 90;
					if (transitionWindow)
						ShowWindow(transitionWindow, SW_MINIMIZE);
					minimizePassed = transitionWindow
						&& IsIconic(transitionWindow) != FALSE;
					break;
				}
				case 2:
					if (transitionWindow)
						ShowWindow(transitionWindow, SW_RESTORE);
					restorePassed = transitionWindow != nullptr;
					break;
				case 3:
					restorePassed = restorePassed && transitionWindow
						&& IsIconic(transitionWindow) == FALSE
						&& IsWindowVisible(transitionWindow) != FALSE;
					resizeBackPassed = originalWindowRectValid && SetWindowPos(transitionWindow,
						nullptr, originalWindowRect.left, originalWindowRect.top,
						originalWidth, originalHeight,
						SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
					break;
				case 4:
				{
					RECT finalRect{};
					resizeBackPassed = resizeBackPassed && transitionWindow
						&& GetWindowRect(transitionWindow, &finalRect) != FALSE
						&& finalRect.left == originalWindowRect.left
						&& finalRect.top == originalWindowRect.top
						&& finalRect.right == originalWindowRect.right
						&& finalRect.bottom == originalWindowRect.bottom;
					break;
				}
				}
				++transitionStep;
				nextTransition = now + std::chrono::milliseconds(
					transition == "fullscreen-roundtrip" ? 1000 : 350);
			}
		}
		if (std::filesystem::exists(completion)) { complete = true; break; }
		if (WaitForSingleObject(process.hProcess, 50) == WAIT_OBJECT_0)
		{
			exitedEarly = true;
			break;
		}
	}
	if (focusControlWindow)
	{
		DestroyWindow(focusControlWindow);
		focusControlWindow = nullptr;
	}
	CloseWindowsContext closeContext{process.dwProcessId};
	EnumWindows(CloseProcessWindows, reinterpret_cast<LPARAM>(&closeContext));
	bool forcedTermination = false;
	if (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0)
	{
		TerminateProcess(process.hProcess, 4);
		WaitForSingleObject(process.hProcess, 5000);
		forcedTermination = true;
	}
	CloseHandle(process.hProcess);
	const bool transitionComplete = transitionStep == 5 && resizeOutPassed
		&& minimizePassed && restorePassed && resizeBackPassed
		&& fullscreenEnterRequested && fullscreenEnterObserved
		&& fullscreenExitRequested && fullscreenExitObserved && fullscreenRectRestored
		&& focusTargetRequested && focusTargetObserved
		&& focusLossRequested && focusLossObserved
		&& focusRestoreRequested && focusRestoreObserved;
	bool rendererReinitComplete = rendererReinitAfter == 0;
	if (rendererReinitAfter != 0)
	{
		std::ifstream markerStream(output / "renderer-reinit-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		rendererReinitComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"main_frame\": " + std::to_string(rendererReinitAfter))
				!= std::string::npos
			&& marker.find("\"renderer\": " + std::to_string(rendererValue))
				!= std::string::npos
			&& marker.find("\"performance_sampling_restarted\": true")
				!= std::string::npos;
	}
	const int switchedRendererValue = rendererValue == 2 ? 6 : 2;
	bool rendererSwitchComplete = rendererSwitchAfter == 0;
	if (rendererSwitchAfter != 0)
	{
		std::ifstream markerStream(output / "renderer-switch-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		rendererSwitchComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"main_frame\": " + std::to_string(rendererSwitchAfter))
				!= std::string::npos
			&& marker.find("\"renderer_from\": " + std::to_string(rendererValue))
				!= std::string::npos
			&& marker.find("\"renderer_to\": " + std::to_string(switchedRendererValue))
				!= std::string::npos
			&& marker.find("\"performance_sampling_restarted\": true")
				!= std::string::npos;
	}
	const int surfaceFrom = api == "d3d11on12" ? 1 : 0;
	const int surfaceTo = surfaceFrom == 0 ? 1 : 0;
	bool surfaceSwitchComplete = surfaceSwitchAfter == 0;
	if (surfaceSwitchAfter != 0)
	{
		std::ifstream markerStream(output / "surface-switch-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		surfaceSwitchComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"main_frame\": " + std::to_string(surfaceSwitchAfter))
				!= std::string::npos
			&& marker.find("\"surface_from\": " + std::to_string(surfaceFrom))
				!= std::string::npos
			&& marker.find("\"surface_to\": " + std::to_string(surfaceTo))
				!= std::string::npos
			&& marker.find("\"performance_sampling_restarted\": true")
				!= std::string::npos;
	}
	bool actualDeviceRemovalComplete = actualDeviceRemovalAfter == 0;
	if (actualDeviceRemovalAfter != 0)
	{
		std::ifstream markerStream(output / "actual-device-removal-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		actualDeviceRemovalComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"main_frame\": "
				+ std::to_string(actualDeviceRemovalAfter)) != std::string::npos
			&& marker.find("\"method\": \"ID3D12Device5::RemoveDevice\"")
				!= std::string::npos
			&& marker.find("\"removal_requested\": true") != std::string::npos
			&& marker.find("\"removal_observed\": true") != std::string::npos
			&& marker.find("\"removed_reason\": \"0x887A0005\"")
				!= std::string::npos
			&& marker.find("\"recovery_initialized\": true") != std::string::npos
			&& marker.find("\"d3d11on12_restored\": true") != std::string::npos
			&& marker.find("\"performance_sampling_restarted\": true")
				!= std::string::npos;
	}
	std::ifstream performanceStream(output / "performance.json");
	const bool performanceOpened = performanceStream.is_open();
	const std::string completedPerformance(
		(std::istreambuf_iterator<char>(performanceStream)),
		std::istreambuf_iterator<char>());
	const bool resourceAccountingComplete = performanceOpened
		&& completedPerformance.find("\"resource_objects\":") != std::string::npos
		&& completedPerformance.find("\"scope\": \"flycast-owned-neural-gpu-objects\"")
			!= std::string::npos
		&& completedPerformance.find("\"initial\":") != std::string::npos
		&& completedPerformance.find("\"minimum\":") != std::string::npos
		&& completedPerformance.find("\"maximum\":") != std::string::npos
		&& completedPerformance.find("\"final\":") != std::string::npos
		&& completedPerformance.find("\"growth\":") != std::string::npos
		&& completedPerformance.find("\"renderer_final\":") != std::string::npos
		&& completedPerformance.find("\"backend_final\":") != std::string::npos;
	auto jsonUnsigned = [&completedPerformance](const char *field,
		std::uint64_t& value) {
		const std::regex expression(std::string("\\\"") + field
			+ "\\\"\\s*:\\s*([0-9]+)");
		std::smatch match;
		if (!std::regex_search(completedPerformance, match, expression)) return false;
		try { value = std::stoull(match[1].str()); }
		catch (...) { return false; }
		return true;
	};
	if (rendererSwitchAfter != 0)
		rendererSwitchComplete = rendererSwitchComplete
			&& completedPerformance.find("\"renderer\": \""
				+ std::string(switchedRendererValue == 6 ? "dx11-oit" : "dx11")
				+ "\"") != std::string::npos;
	bool gameReloadComplete = gameReloadAfter == 0;
	if (gameReloadAfter != 0)
	{
		std::ifstream markerStream(output / "game-reload-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		gameReloadComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"main_frame\": " + std::to_string(gameReloadAfter))
				!= std::string::npos
			&& marker.find("\"unload_observed\": true") != std::string::npos
			&& marker.find("\"same_game_id\": true") != std::string::npos
			&& marker.find("\"same_media_path\": true") != std::string::npos;
	}
	bool saveStateComplete = saveStateAfter == 0;
	if (saveStateAfter != 0)
	{
		std::ifstream markerStream(output / "savestate-roundtrip-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		saveStateComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"in_memory\": true") != std::string::npos
			&& marker.find("\"save_allowed\": true") != std::string::npos
			&& marker.find("\"saved\": true") != std::string::npos
			&& marker.find("\"loaded\": true") != std::string::npos
			&& marker.find("\"save_main_frame\": " + std::to_string(saveStateAfter))
				!= std::string::npos
			&& marker.find("\"load_main_frame\": "
				+ std::to_string(saveStateAfter + saveStateLoadDelay)) != std::string::npos
			&& marker.find("\"state_bytes\": 0") == std::string::npos;
	}
	bool pauseComplete = pauseAfter == 0;
	if (pauseAfter != 0)
	{
		std::ifstream markerStream(output / "pause-roundtrip-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		pauseComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"pause_observed\": true") != std::string::npos
			&& marker.find("\"resume_observed\": true") != std::string::npos
			&& marker.find("\"pause_main_frame\": " + std::to_string(pauseAfter))
				!= std::string::npos
			&& marker.find("\"resume_main_frame\": "
				+ std::to_string(pauseAfter + pauseDuration)) != std::string::npos;
	}
	bool modeRoundtripComplete = modeRoundtripAfter == 0;
	if (modeRoundtripAfter != 0)
	{
		std::ifstream markerStream(output / "neural-mode-roundtrip-complete.json");
		const bool markerOpened = markerStream.is_open();
		const std::string marker((std::istreambuf_iterator<char>(markerStream)),
			std::istreambuf_iterator<char>());
		std::uint64_t initialMode = 0, finalMode = 0, modeTransitions = 0;
		std::uint64_t offModeSamples = 0, offNativePresents = 0;
		std::uint64_t offAcceptedEvaluations = 0, requestedModeSamples = 0;
		std::uint64_t requestedModeNeuralPresents = 0, requestedModeNativePresents = 0;
		std::uint64_t reentryResetAccepts = 0;
		std::uint64_t acceptedNotPresented = 0, identityMismatches = 0;
		std::uint64_t outputRepeats = 0, nativeNeuralAlternations = 0;
		modeRoundtripComplete = markerOpened
			&& marker.find("\"completed\": true") != std::string::npos
			&& marker.find("\"original_mode\": " + std::to_string(mode)) != std::string::npos
			&& marker.find("\"off_mode\": 0") != std::string::npos
			&& marker.find("\"restored_mode\": " + std::to_string(mode)) != std::string::npos
			&& marker.find("\"off_main_frame\": "
				+ std::to_string(modeRoundtripAfter)) != std::string::npos
			&& marker.find("\"on_main_frame\": "
				+ std::to_string(modeRoundtripAfter + modeOffDuration)) != std::string::npos
			&& marker.find("\"renderer_restarted\": false") != std::string::npos
			&& marker.find("\"performance_sampling_restarted\": false") != std::string::npos
			&& jsonUnsigned("initial_mode", initialMode) && initialMode == static_cast<unsigned>(mode)
			&& jsonUnsigned("final_mode", finalMode) && finalMode == static_cast<unsigned>(mode)
			&& jsonUnsigned("mode_transitions", modeTransitions) && modeTransitions == 2
			&& jsonUnsigned("off_mode_samples", offModeSamples) && offModeSamples != 0
			&& jsonUnsigned("off_native_presents", offNativePresents)
			&& offNativePresents == offModeSamples
			&& jsonUnsigned("off_accepted_evaluations", offAcceptedEvaluations)
			&& offAcceptedEvaluations == 0
			&& jsonUnsigned("requested_mode_samples", requestedModeSamples)
			&& requestedModeSamples > offModeSamples
			&& jsonUnsigned("requested_mode_neural_presents", requestedModeNeuralPresents)
			&& requestedModeNeuralPresents != 0
			&& jsonUnsigned("requested_mode_native_presents", requestedModeNativePresents)
			&& requestedModeNativePresents <= 1
			&& jsonUnsigned("reentry_reset_accepts", reentryResetAccepts)
			&& reentryResetAccepts != 0
			&& completedPerformance.find("\"first_reentry_accepted_reset\": true")
				!= std::string::npos
			&& jsonUnsigned("accepted_not_presented", acceptedNotPresented)
			&& acceptedNotPresented == 0
			&& jsonUnsigned("frame_identity_mismatches", identityMismatches)
			&& identityMismatches == 0
			&& jsonUnsigned("output_frame_repeats", outputRepeats) && outputRepeats == 0
			&& jsonUnsigned("native_neural_alternations", nativeNeuralAlternations)
			&& nativeNeuralAlternations >= 2 && nativeNeuralAlternations <= 4;
	}
	if (surfaceSwitchAfter != 0)
		surfaceSwitchComplete = surfaceSwitchComplete
			&& completedPerformance.find("\"api\": \""
				+ std::string(surfaceTo == 1 ? "d3d11on12" : "d3d11")
				+ "\"") != std::string::npos;
	if (actualDeviceRemovalAfter != 0)
	{
		std::uint64_t acceptedEvaluations = 0, neuralPresents = 0;
		std::uint64_t nativePresents = 0, missingPresents = 0;
		std::uint64_t acceptedNotPresented = 0, identityMismatches = 0;
		std::uint64_t sourceRepeats = 0, outputRepeats = 0, latencyMax = 0;
		actualDeviceRemovalComplete = actualDeviceRemovalComplete
			&& completedPerformance.find("\"api\": \"d3d11on12\"")
				!= std::string::npos
			&& jsonUnsigned("accepted_evaluations", acceptedEvaluations)
			&& acceptedEvaluations == frames
			&& jsonUnsigned("neural_presents", neuralPresents) && neuralPresents == frames
			&& jsonUnsigned("native_presents", nativePresents) && nativePresents == 0
			&& jsonUnsigned("missing_presents", missingPresents) && missingPresents == 0
			&& jsonUnsigned("accepted_not_presented", acceptedNotPresented)
			&& acceptedNotPresented == 0
			&& jsonUnsigned("frame_identity_mismatches", identityMismatches)
			&& identityMismatches == 0
			&& jsonUnsigned("source_frame_repeats", sourceRepeats) && sourceRepeats == 0
			&& jsonUnsigned("output_frame_repeats", outputRepeats) && outputRepeats == 0
			&& jsonUnsigned("latency_frames_max", latencyMax) && latencyMax == 0;
	}
	bool sehExceptionComplete = injection != "seh-exception";
	if (injection == "seh-exception")
	{
		std::uint64_t exceptionCode = 0, evaluateFailures = 0;
		std::uint64_t acceptedEvaluations = 0, neuralPresents = 0, nativePresents = 0;
		std::uint64_t missingPresents = 0, acceptedNotPresented = 0;
		std::uint64_t identityMismatches = 0, outputRepeats = 0, latencyMax = 0;
		sehExceptionComplete = jsonUnsigned("last_exception_code", exceptionCode)
			&& exceptionCode == 0xE0424E47ull
			&& jsonUnsigned("evaluate_failures", evaluateFailures)
			&& evaluateFailures >= injectionCount
			&& jsonUnsigned("accepted_evaluations", acceptedEvaluations)
			&& acceptedEvaluations != 0
			&& jsonUnsigned("neural_presents", neuralPresents) && neuralPresents != 0
			&& jsonUnsigned("native_presents", nativePresents) && nativePresents != 0
			&& jsonUnsigned("missing_presents", missingPresents) && missingPresents == 0
			&& jsonUnsigned("accepted_not_presented", acceptedNotPresented)
			&& acceptedNotPresented == 0
			&& jsonUnsigned("frame_identity_mismatches", identityMismatches)
			&& identityMismatches == 0
			&& jsonUnsigned("output_frame_repeats", outputRepeats) && outputRepeats == 0
			&& jsonUnsigned("latency_frames_max", latencyMax) && latencyMax == 0;
	}
	if (!complete)
	{
		std::cerr << (exitedEarly ? "flycast exited before performance run completed"
			: "performance run timed out") << '\n';
		return 1;
	}
	std::ofstream launchReport(output / "performance-launch.json");
	launchReport << "{\n  \"schema\": 1,\n  \"lane\": \"" << lane
		<< "\",\n  \"api\": \"" << api << "\",\n  \"renderer\": \"" << renderer
		<< "\",\n  \"preset\": \"" << preset << "\",\n  \"render_height\": " << renderHeight
		<< ",\n  \"input_replay_requested\": " << (inputReplay == "yes" ? "true" : "false")
		<< ",\n  \"input_replay_retained\": " << (inputReplay == "yes" ? "true" : "false")
		<< ",\n  \"input_replay_fnv64\": \"" << (inputReplay == "yes" ? Hex64(inputReplayHash) : "") << "\""
		<< ",\n  \"input_replay_bytes\": " << inputReplayBytes
		<< ",\n  \"failure_injection\": \"" << injection
		<< "\",\n  \"failure_injection_count\": " << injectionCount
		<< ",\n  \"failure_injection_after_accepted\": " << injectionAfter
		<< ",\n  \"window_transition\": \"" << transition
		<< "\",\n  \"window_transition_delay_ms\": " << transitionDelayMs
		<< ",\n  \"window_transition_completed\": "
		<< (transitionComplete ? "true" : "false")
		<< ",\n  \"window_transition_actions\": {\"resize_out\": "
		<< (resizeOutPassed ? "true" : "false")
		<< ", \"minimize\": " << (minimizePassed ? "true" : "false")
		<< ", \"restore\": " << (restorePassed ? "true" : "false")
		<< ", \"resize_back\": " << (resizeBackPassed ? "true" : "false")
		<< ", \"fullscreen_enter_requested\": " << (fullscreenEnterRequested ? "true" : "false")
		<< ", \"fullscreen_enter_observed\": " << (fullscreenEnterObserved ? "true" : "false")
		<< ", \"fullscreen_exit_requested\": " << (fullscreenExitRequested ? "true" : "false")
		<< ", \"fullscreen_exit_observed\": " << (fullscreenExitObserved ? "true" : "false")
		<< ", \"fullscreen_rect_restored\": " << (fullscreenRectRestored ? "true" : "false")
		<< ", \"focus_target_requested\": " << (focusTargetRequested ? "true" : "false")
		<< ", \"focus_target_observed\": " << (focusTargetObserved ? "true" : "false")
		<< ", \"focus_loss_requested\": " << (focusLossRequested ? "true" : "false")
		<< ", \"focus_loss_observed\": " << (focusLossObserved ? "true" : "false")
		<< ", \"focus_restore_requested\": " << (focusRestoreRequested ? "true" : "false")
		<< ", \"focus_restore_observed\": " << (focusRestoreObserved ? "true" : "false") << "}"
		<< ",\n  \"renderer_reinit_after_main_frames\": " << rendererReinitAfter
		<< ",\n  \"renderer_reinit_completed\": "
		<< (rendererReinitComplete ? "true" : "false")
		<< ",\n  \"renderer_switch_after_main_frames\": " << rendererSwitchAfter
		<< ",\n  \"renderer_switch_to\": \""
		<< (rendererSwitchAfter == 0 ? "none"
			: switchedRendererValue == 6 ? "dx11-oit" : "dx11") << "\""
		<< ",\n  \"renderer_switch_completed\": "
		<< (rendererSwitchComplete ? "true" : "false")
		<< ",\n  \"surface_switch_after_main_frames\": " << surfaceSwitchAfter
		<< ",\n  \"surface_switch_to\": \""
		<< (surfaceSwitchAfter == 0 ? "none"
			: surfaceTo == 1 ? "d3d11on12" : "d3d11") << "\""
		<< ",\n  \"surface_switch_completed\": "
		<< (surfaceSwitchComplete ? "true" : "false")
		<< ",\n  \"actual_device_removal_after_main_frames\": "
		<< actualDeviceRemovalAfter
		<< ",\n  \"actual_device_removal_completed\": "
		<< (actualDeviceRemovalComplete ? "true" : "false")
		<< ",\n  \"game_reload_after_main_frames\": " << gameReloadAfter
		<< ",\n  \"game_reload_completed\": "
		<< (gameReloadComplete ? "true" : "false")
		<< ",\n  \"savestate_roundtrip_after_main_frames\": " << saveStateAfter
		<< ",\n  \"savestate_load_delay_main_frames\": " << saveStateLoadDelay
		<< ",\n  \"savestate_roundtrip_completed\": "
		<< (saveStateComplete ? "true" : "false")
		<< ",\n  \"pause_roundtrip_after_main_frames\": " << pauseAfter
		<< ",\n  \"pause_duration_main_frames\": " << pauseDuration
		<< ",\n  \"pause_roundtrip_completed\": "
		<< (pauseComplete ? "true" : "false")
		<< ",\n  \"mode_roundtrip_after_main_frames\": " << modeRoundtripAfter
		<< ",\n  \"mode_off_duration_main_frames\": " << modeOffDuration
		<< ",\n  \"mode_roundtrip_completed\": "
		<< (modeRoundtripComplete ? "true" : "false")
		<< ",\n  \"seh_exception_completed\": "
		<< (sehExceptionComplete ? "true" : "false")
		<< ",\n  \"resource_accounting_completed\": "
		<< (resourceAccountingComplete ? "true" : "false")
		<< ",\n  \"requested_samples\": " << frames << ",\n  \"warmup_frames\": " << warmup
		<< ",\n  \"clean_window_close\": " << (forcedTermination ? "false" : "true")
		<< ",\n  \"media_path_recorded\": false\n}\n";
	std::cout << "performance complete samples=" << frames << " lane=" << lane
		<< " api=" << api << " renderer=" << renderer
		<< " injection=" << injection << ':' << injectionCount
		<< " transition=" << transition << ':' << (transitionComplete ? "pass" : "fail")
		<< " renderer_reinit=" << (rendererReinitComplete ? "pass" : "fail")
		<< " renderer_switch=" << (rendererSwitchComplete ? "pass" : "fail")
		<< " surface_switch=" << (surfaceSwitchComplete ? "pass" : "fail")
		<< " actual_device_removal="
		<< (actualDeviceRemovalComplete ? "pass" : "fail")
		<< " game_reload=" << (gameReloadComplete ? "pass" : "fail")
		<< " savestate=" << (saveStateComplete ? "pass" : "fail")
		<< " pause=" << (pauseComplete ? "pass" : "fail")
		<< " mode_roundtrip=" << (modeRoundtripComplete ? "pass" : "fail")
		<< " seh_exception=" << (sehExceptionComplete ? "pass" : "fail")
		<< " resources=" << (resourceAccountingComplete ? "pass" : "fail")
		<< " clean_close=" << (forcedTermination ? "no" : "yes") << '\n';
	return launchReport && transitionComplete && rendererReinitComplete
		&& rendererSwitchComplete && surfaceSwitchComplete
		&& actualDeviceRemovalComplete && gameReloadComplete
		&& saveStateComplete && pauseComplete && modeRoundtripComplete
		&& sehExceptionComplete && resourceAccountingComplete ? 0 : 1;
#endif
}

std::string HtmlEscape(const std::string& value)
{
	std::string result;
	for (const char c : value)
	{
		switch (c)
		{
		case '&': result += "&amp;"; break;
		case '<': result += "&lt;"; break;
		case '>': result += "&gt;"; break;
		case '"': result += "&quot;"; break;
		default: result.push_back(c); break;
		}
	}
	return result;
}

std::string JsonStringField(const std::string& json, const std::string& field)
{
	const auto marker = "\"" + field + "\":";
	auto position = json.find(marker);
	if (position == std::string::npos) return {};
	position = json.find('"', position + marker.size());
	if (position == std::string::npos) return {};
	std::string result;
	bool escaped = false;
	for (++position; position < json.size(); ++position)
	{
		const char c = json[position];
		if (escaped) { result.push_back(c); escaped = false; continue; }
		if (c == '\\') { escaped = true; continue; }
		if (c == '"') break;
		result.push_back(c);
	}
	return result;
}

std::string JsonScalarField(const std::string& json, const std::string& field)
{
	const auto marker = "\"" + field + "\":";
	auto begin = json.find(marker);
	if (begin == std::string::npos) return {};
	begin += marker.size();
	while (begin < json.size() && std::isspace(static_cast<unsigned char>(json[begin]))) ++begin;
	auto end = begin;
	while (end < json.size() && json[end] != ',' && json[end] != '\n' && json[end] != '}') ++end;
	while (end > begin && std::isspace(static_cast<unsigned char>(json[end - 1]))) --end;
	return json.substr(begin, end - begin);
}

bool JsonUnsignedArrayField(const std::string& json, const std::string& field,
	std::size_t expectedCount)
{
	const auto marker = "\"" + field + "\":";
	auto position = json.find(marker);
	if (position == std::string::npos) return false;
	position += marker.size();
	auto skipSpace = [&]() {
		while (position < json.size()
			&& std::isspace(static_cast<unsigned char>(json[position]))) ++position;
	};
	skipSpace();
	if (position >= json.size() || json[position++] != '[') return false;
	for (std::size_t item = 0; item < expectedCount; ++item)
	{
		skipSpace();
		if (position >= json.size()
			|| !std::isdigit(static_cast<unsigned char>(json[position]))) return false;
		std::uint64_t value = 0;
		while (position < json.size()
			&& std::isdigit(static_cast<unsigned char>(json[position])))
		{
			value = value * 10 + static_cast<unsigned>(json[position++] - '0');
			if (value > std::numeric_limits<std::uint32_t>::max()) return false;
		}
		skipSpace();
		if (item + 1 < expectedCount)
		{
			if (position >= json.size() || json[position++] != ',') return false;
		}
	}
	skipSpace();
	return position < json.size() && json[position] == ']';
}

struct ExternalEvidenceRecord {
	std::string gitSha;
	std::uint64_t frameId = 0;
	std::string color;
	std::string depth;
	std::string motion;
	std::string mask;
	std::string returned;
	std::string marked;
};

struct ExternalEvidenceLog {
	std::vector<ExternalEvidenceRecord> records;
	std::set<std::uint64_t> markerPresentedFrames;
	std::set<std::uint64_t> completedPresentFrames;
	bool contractEvaluated = false;
};

bool ReadTextFile(const std::filesystem::path& path, std::string& text)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream) return false;
	text.assign(std::istreambuf_iterator<char>(stream), {});
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	return static_cast<bool>(stream) || stream.eof();
}

bool JsonUint32Field(const std::string& json, const std::string& field,
	std::uint32_t& value)
{
	const auto scalar = JsonScalarField(json, field);
	if (scalar.empty()) return false;
	try
	{
		const auto parsed = std::stoull(scalar);
		if (parsed > std::numeric_limits<std::uint32_t>::max()) return false;
		value = static_cast<std::uint32_t>(parsed);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

struct ProductionScaleStats {
	std::uint64_t comparedPixels = 0;
	std::uint64_t differingPixels = 0;
	std::uint64_t edgeSourcePixels = 0;
	std::uint64_t edgeComparedPixels = 0;
	std::uint64_t edgeDifferingPixels = 0;
	std::uint64_t subpixelDiverseEdgeBlocks = 0;
	std::uint64_t absoluteDelta = 0;
	std::uint32_t maxDelta = 0;
};

bool ReadExactBytes(const std::filesystem::path& path, std::size_t expected,
	std::vector<std::uint8_t>& bytes)
{
	std::error_code ec;
	if (std::filesystem::file_size(path, ec) != expected || ec) return false;
	bytes.resize(expected);
	std::ifstream stream(path, std::ios::binary);
	stream.read(reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	return stream.gcount() == static_cast<std::streamsize>(bytes.size())
		&& stream.peek() == std::char_traits<char>::eof();
}

bool IsSourceEdge(const std::vector<std::uint8_t>& source, std::uint32_t width,
	std::uint32_t height, std::uint32_t x, std::uint32_t y)
{
	constexpr int edgeThreshold = 8;
	const auto differs = [&](std::uint32_t nx, std::uint32_t ny) {
		const std::size_t a = (static_cast<std::size_t>(y) * width + x) * 4;
		const std::size_t b = (static_cast<std::size_t>(ny) * width + nx) * 4;
		for (std::size_t channel = 0; channel < 3; ++channel)
			if (std::abs(static_cast<int>(source[a + channel])
				- static_cast<int>(source[b + channel])) >= edgeThreshold)
				return true;
		return false;
	};
	return (x > 0 && differs(x - 1, y))
		|| (x + 1 < width && differs(x + 1, y))
		|| (y > 0 && differs(x, y - 1))
		|| (y + 1 < height && differs(x, y + 1));
}

ProductionScaleStats CompareProductionScale(const std::vector<std::uint8_t>& source,
	std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& scaled,
	std::uint32_t scale)
{
	ProductionScaleStats stats;
	const std::uint32_t scaledWidth = width * scale;
	for (std::uint32_t y = 0; y < height; ++y)
	for (std::uint32_t x = 0; x < width; ++x)
	{
		const std::size_t sourceOffset = (static_cast<std::size_t>(y) * width + x) * 4;
		const bool edge = IsSourceEdge(source, width, height, x, y);
		stats.edgeSourcePixels += edge ? 1 : 0;
		bool diverse = false;
		std::array<std::uint8_t, 4> first{};
		for (std::uint32_t sy = 0; sy < scale; ++sy)
		for (std::uint32_t sx = 0; sx < scale; ++sx)
		{
			const std::size_t scaledOffset = ((static_cast<std::size_t>(y) * scale + sy)
				* scaledWidth + static_cast<std::size_t>(x) * scale + sx) * 4;
			bool pixelDiffers = false;
			for (std::size_t channel = 0; channel < 4; ++channel)
			{
				const auto scaledValue = scaled[scaledOffset + channel];
				if (sx == 0 && sy == 0) first[channel] = scaledValue;
				else diverse = diverse || scaledValue != first[channel];
				const auto delta = static_cast<std::uint32_t>(std::abs(
					static_cast<int>(scaledValue) - static_cast<int>(source[sourceOffset + channel])));
				stats.absoluteDelta += delta;
				stats.maxDelta = std::max(stats.maxDelta, delta);
				pixelDiffers = pixelDiffers || delta != 0;
			}
			++stats.comparedPixels;
			stats.differingPixels += pixelDiffers ? 1 : 0;
			if (edge)
			{
				++stats.edgeComparedPixels;
				stats.edgeDifferingPixels += pixelDiffers ? 1 : 0;
			}
		}
		stats.subpixelDiverseEdgeBlocks += edge && diverse ? 1 : 0;
	}
	return stats;
}

bool AcceptProductionScaleStats(const ProductionScaleStats& stats)
{
	const auto minimumDifferent = std::max<std::uint64_t>(1000, stats.comparedPixels / 1000);
	const auto minimumEdgeDifferent = std::max<std::uint64_t>(1000,
		stats.edgeComparedPixels / 100);
	const auto minimumDiverse = std::max<std::uint64_t>(100,
		stats.edgeSourcePixels / 100);
	return stats.differingPixels >= minimumDifferent
		&& stats.edgeDifferingPixels >= minimumEdgeDifferent
		&& stats.subpixelDiverseEdgeBlocks >= minimumDiverse;
}

int ProductionScalingCommand(const Args& args)
{
	const auto gameText = Value(args, "--game");
	const auto flycastText = Value(args, "--flycast");
	const auto replayText = Value(args, "--input-replay");
	const auto outputText = Value(args, "--out");
	if (gameText.empty() || flycastText.empty() || replayText.empty() || outputText.empty())
	{
		std::cerr << "production-scaling requires --game, --flycast, --input-replay, and --out\n";
		return 2;
	}
	const auto renderer = Value(args, "--renderer", "dx11");
	const auto api = Value(args, "--api", "d3d11");
	if ((renderer != "dx11" && renderer != "dx11-oit")
		|| (api != "d3d11" && api != "d3d11on12"))
	{
		std::cerr << "production-scaling requires a dx11 renderer and d3d11 or d3d11on12 API\n";
		return 2;
	}
	std::string error;
	std::uint32_t frames = 1, skip = 0, baseHeight = 480, timeoutMs = 120000;
	if (!Number(args, "--frames", 1, frames, error) || frames == 0 || frames > 10
		|| !Number(args, "--skip", 0, skip, error)
		|| !Number(args, "--base-height", 480, baseHeight, error)
		|| baseHeight < 120 || baseHeight > 1080
		|| !Number(args, "--timeout-ms", 120000, timeoutMs, error) || timeoutMs < 1000)
	{
		std::cerr << (error.empty()
			? "production-scaling requires 1..10 frames, base height 120..1080, and timeout at least 1000"
			: error) << '\n';
		return 2;
	}
	const auto game = std::filesystem::absolute(gameText);
	const auto flycast = std::filesystem::absolute(flycastText);
	const auto replay = std::filesystem::absolute(replayText);
	const auto output = std::filesystem::absolute(outputText);
	for (const auto& required : {game, flycast, replay})
		if (!std::filesystem::is_regular_file(required))
		{
			std::cerr << "required production-scaling input is unavailable: "
				<< required.string() << '\n';
			return 3;
		}
	if (std::filesystem::exists(output / "production-scaling-report.json"))
	{
		std::cerr << "production-scaling output already contains a completed report\n";
		return 2;
	}
#ifndef _WIN32
	std::cerr << "production-scaling launcher is currently available only on Windows\n";
	return 3;
#else
	const int rendererValue = renderer == "dx11-oit" ? 6 : 2;
	for (const auto scale : {1u, 4u, 8u})
	{
		const auto scaleOutput = output / (std::to_string(scale) + "x");
		if (!RunNativeParityProcess(flycast, game, scaleOutput, replay, rendererValue,
			frames, skip, baseHeight * scale, timeoutMs, true, api == "d3d11on12", error))
		{
			std::cerr << "production " << scale << "x capture failed: " << error << '\n';
			return 1;
		}
		std::string marker;
		if (!ReadTextFile(scaleOutput / "native-parity-capture-complete.json", marker)
			|| marker.find("\"renderer\": \"" + renderer + "\"") == std::string::npos
			|| marker.find(std::string("\"d3d11on12_surface\": ")
				+ (api == "d3d11on12" ? "true" : "false")) == std::string::npos
			|| marker.find("\"neural_compiled\": true") == std::string::npos
			|| marker.find("\"neural_mode\": 0") == std::string::npos
			|| marker.find("\"neural_instrumentation_enabled\": false") == std::string::npos)
		{
			std::cerr << "production " << scale
				<< "x capture did not use the requested renderer/API or disabled neural path\n";
			return 1;
		}
	}

	std::uint64_t replayHash = 0;
	if (!HashFileFnv64Standard(replay, replayHash))
	{
		std::cerr << "cannot hash production-scaling input replay\n";
		return 1;
	}
	std::error_code ec;
	std::filesystem::create_directories(output, ec);
	std::filesystem::copy_file(replay, output / "input-replay.input",
		std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
	{
		std::cerr << "cannot retain production-scaling replay: " << ec.message() << '\n';
		return 1;
	}

	const auto incompleteReport = output / "production-scaling-report.incomplete.json";
	std::ofstream report(incompleteReport);
	report.imbue(std::locale::classic());
	report << "{\n  \"schema\": 1,\n  \"status\": \"pass\",\n"
		<< "  \"scope\": \"production-pvr-genuine-scaling\",\n"
		<< "  \"api\": \"" << api << "\",\n"
		<< "  \"renderer\": \"" << renderer << "\",\n"
		<< "  \"frames\": " << frames << ",\n"
		<< "  \"skip\": " << skip << ",\n"
		<< "  \"base_height\": " << baseHeight << ",\n"
		<< "  \"input_replay_fnv64\": \"" << Hex64(replayHash) << "\",\n"
		<< "  \"edge_threshold\": 8,\n"
		<< "  \"minimum_differing_pixels\": \"max(1000,compared/1000)\",\n"
		<< "  \"minimum_edge_differing_pixels\": \"max(1000,edge_compared/100)\",\n"
		<< "  \"minimum_diverse_edge_blocks\": \"max(100,edge_source/100)\",\n"
		<< "  \"results\": [\n";
	bool firstResult = true;
	std::string expectedSha;
	for (std::uint32_t frame = 0; frame < frames; ++frame)
	{
		std::ostringstream name;
		name << "frame-" << std::setfill('0') << std::setw(6) << frame;
		std::string baseJson;
		if (!ReadTextFile(output / "1x" / (name.str() + ".json"), baseJson))
		{
			std::cerr << "cannot read production 1x metadata\n";
			return 1;
		}
		std::uint32_t width = 0, height = 0, sourceFrame = 0;
		if (!JsonUint32Field(baseJson, "width", width)
			|| !JsonUint32Field(baseJson, "height", height)
			|| !JsonUint32Field(baseJson, "source_frame_index", sourceFrame)
			|| height != baseHeight || sourceFrame != skip || width == 0)
		{
			std::cerr << "invalid production 1x metadata contract\n";
			return 1;
		}
		const auto sha = JsonStringField(baseJson, "git_sha");
		if (sha.empty() || (!expectedSha.empty() && sha != expectedSha))
		{
			std::cerr << "production-scaling Git identity changed across captures\n";
			return 1;
		}
		expectedSha = sha;
		std::vector<std::uint8_t> source;
		if (!ReadExactBytes(output / "1x" / (name.str() + ".bgra8"),
			static_cast<std::size_t>(width) * height * 4, source))
		{
			std::cerr << "invalid production 1x raw frame\n";
			return 1;
		}
		for (const std::uint32_t scale : {4u, 8u})
		{
			std::string scaledJson;
			if (!ReadTextFile(output / (std::to_string(scale) + "x")
				/ (name.str() + ".json"), scaledJson))
			{
				std::cerr << "cannot read production scaled metadata\n";
				return 1;
			}
			std::uint32_t scaledWidth = 0, scaledHeight = 0, scaledSourceFrame = 0;
			if (!JsonUint32Field(scaledJson, "width", scaledWidth)
				|| !JsonUint32Field(scaledJson, "height", scaledHeight)
				|| !JsonUint32Field(scaledJson, "source_frame_index", scaledSourceFrame)
				|| scaledWidth != width * scale || scaledHeight != height * scale
				|| scaledSourceFrame != sourceFrame
				|| JsonStringField(scaledJson, "git_sha") != expectedSha)
			{
				std::cerr << "scaled production frame identity/dimension contract failed at "
					<< scale << "x\n";
				return 1;
			}
			std::vector<std::uint8_t> scaled;
			if (!ReadExactBytes(output / (std::to_string(scale) + "x")
				/ (name.str() + ".bgra8"),
				static_cast<std::size_t>(scaledWidth) * scaledHeight * 4, scaled))
			{
				std::cerr << "invalid production scaled raw frame\n";
				return 1;
			}
			const auto stats = CompareProductionScale(source, width, height, scaled, scale);
			if (!AcceptProductionScaleStats(stats))
			{
				std::cerr << "production " << scale
					<< "x is indistinguishable from nearest or lacks material edge sampling: differing="
					<< stats.differingPixels << " edge=" << stats.edgeDifferingPixels
					<< " diverse=" << stats.subpixelDiverseEdgeBlocks << '\n';
				return 1;
			}
			std::uint64_t rawHash = 0;
			if (!HashFileFnv64Standard(output / (std::to_string(scale) + "x")
				/ (name.str() + ".bgra8"), rawHash))
				return 1;
			if (!firstResult) report << ",\n";
			firstResult = false;
			report << "    {\"frame\": " << frame << ", \"source_frame_index\": "
				<< sourceFrame << ", \"scale\": " << scale << ", \"dimensions\": ["
				<< scaledWidth << ", " << scaledHeight << "], \"fnv64\": \""
				<< Hex64(rawHash) << "\", \"compared_pixels\": " << stats.comparedPixels
				<< ", \"differing_pixels\": " << stats.differingPixels
				<< ", \"edge_source_pixels\": " << stats.edgeSourcePixels
				<< ", \"edge_compared_pixels\": " << stats.edgeComparedPixels
				<< ", \"edge_differing_pixels\": " << stats.edgeDifferingPixels
				<< ", \"subpixel_diverse_edge_blocks\": " << stats.subpixelDiverseEdgeBlocks
				<< ", \"max_delta\": " << stats.maxDelta
				<< ", \"mean_absolute_delta\": "
				<< (stats.comparedPixels == 0 ? 0.0 : static_cast<double>(stats.absoluteDelta)
					/ static_cast<double>(stats.comparedPixels * 4)) << "}";
		}
	}
	report << "\n  ],\n  \"wrong_nearest_control\": {\"differing_pixels\": 0, "
		"\"edge_differing_pixels\": 0, \"subpixel_diverse_edge_blocks\": 0, "
		"\"accepted\": false},\n"
		<< "  \"git_sha\": \"" << expectedSha << "\",\n"
		<< "  \"synchronous_developer_capture\": true,\n"
		<< "  \"clean_close\": true,\n"
		<< "  \"performance_eligible\": false,\n"
		<< "  \"media_path_recorded\": false\n}\n";
	const bool reportWritten = static_cast<bool>(report);
	report.close();
	if (!reportWritten || AcceptProductionScaleStats({}))
	{
		std::cerr << "cannot write production-scaling report or nearest negative was accepted\n";
		return 1;
	}
	std::filesystem::rename(incompleteReport, output / "production-scaling-report.json", ec);
	if (ec)
	{
		std::cerr << "cannot publish production-scaling report: " << ec.message() << '\n';
		return 1;
	}
	std::cout << "production-scaling pass api=" << api << " renderer=" << renderer
		<< " frames=" << frames << " git_sha=" << expectedSha << '\n';
	return 0;
#endif
}

bool ParseExternalEvidenceLog(const std::filesystem::path& path,
	ExternalEvidenceLog& result, std::string& error)
{
	std::ifstream stream(path);
	if (!stream)
	{
		error = "evidence log is unavailable: " + path.string();
		return false;
	}
	const std::regex evidence(
		R"(DLSS 5 developer evidence: git_sha=([0-9A-Fa-f]+) captures=[0-9]+ failures=([0-9]+) frame=([0-9]+) color_fnv64=([0-9A-Fa-f]{16}) depth_fnv64=([0-9A-Fa-f]{16}) motion_fnv64=([0-9A-Fa-f]{16}) mask_fnv64=([0-9A-Fa-f]{16}) returned_fnv64=([0-9A-Fa-f]{16}) marked_fnv64=([0-9A-Fa-f]{16}))");
	const std::regex marker(
		R"(DLSS 5 developer present evidence: capture=[0-9]+ frame=([0-9]+).*marker_pixels=1024/1024)");
	const std::regex present(
		R"(DLSS 5 candidate public-output present completed: frame=([0-9]+) route=d3d11on12)");
	std::string line;
	while (std::getline(stream, line))
	{
		std::smatch match;
		if (std::regex_search(line, match, evidence))
		{
			if (match[2] != "0")
			{
				error = "evidence log contains a capture failure";
				return false;
			}
			ExternalEvidenceRecord record;
			record.gitSha = match[1];
			record.frameId = std::stoull(match[3]);
			record.color = match[4];
			record.depth = match[5];
			record.motion = match[6];
			record.mask = match[7];
			record.returned = match[8];
			record.marked = match[9];
			result.records.push_back(std::move(record));
		}
		if (std::regex_search(line, match, marker))
			result.markerPresentedFrames.insert(std::stoull(match[1]));
		if (std::regex_search(line, match, present))
			result.completedPresentFrames.insert(std::stoull(match[1]));
		result.contractEvaluated = result.contractEvaluated
			|| line.find("readiness=contract-evaluated contract_evaluated=1") != std::string::npos;
	}
	if (result.records.empty())
	{
		error = "evidence log has no current-schema records";
		return false;
	}
	return true;
}

bool ShaMatches(const std::string& recorded, const std::string& expected)
{
	if (recorded.empty() || expected.empty()) return false;
	return recorded.size() <= expected.size()
		? expected.compare(0, recorded.size(), recorded) == 0
		: recorded.compare(0, expected.size(), expected) == 0;
}

bool ReplaceExactlyOnce(std::string& text, const std::string& from,
	const std::string& to)
{
	const auto position = text.find(from);
	if (position == std::string::npos || text.find(from, position + from.size()) != std::string::npos)
		return false;
	text.replace(position, from.size(), to);
	return true;
}

std::string ExternalSettingsProofJson(const neuraltest::ExternalConsumerSettings& settings,
	const std::string& indentation = "  ")
{
	std::ostringstream json;
	json.imbue(std::locale::classic());
	json << std::setprecision(std::numeric_limits<double>::max_digits10)
		<< indentation << "\"external_settings_proof\": {\n"
		<< indentation << "  \"schema\": 1,\n"
		<< indentation << "  \"source\": \"consumer ON host log\",\n"
		<< indentation << "  \"stable_across_log\": true,\n"
		<< indentation << "  \"upscaling\": " << (settings.upscaling ? "true" : "false") << ",\n"
		<< indentation << "  \"intensity\": " << settings.intensity << ",\n"
		<< indentation << "  \"global_tone\": " << settings.globalTone << ",\n"
		<< indentation << "  \"diffuse_white_nits\": " << settings.diffuseWhiteNits << ",\n"
		<< indentation << "  \"preset\": " << settings.preset << ",\n"
		<< indentation << "  \"style\": " << settings.style << ",\n"
		<< indentation << "  \"enabled\": " << (settings.enabled ? "true" : "false") << "\n"
		<< indentation << "}";
	return json.str();
}

int ConfirmExternalCaptureCommand(const Args& args)
{
	const auto captureText = Value(args, "--capture");
	const auto onLogText = Value(args, "--on-log");
	const auto onHostLogText = Value(args, "--on-host-log");
	const auto offLogText = Value(args, "--off-log");
	const auto offHostLogText = Value(args, "--off-host-log");
	const auto expectedSha = Value(args, "--git-sha");
	if (captureText.empty() || onLogText.empty() || onHostLogText.empty()
		|| offLogText.empty() || offHostLogText.empty() || expectedSha.empty())
	{
		std::cerr << "confirm-external-capture requires --capture, --on-log, --on-host-log, --off-log, --off-host-log, and --git-sha\n";
		return 2;
	}
	const auto captureRoot = std::filesystem::absolute(captureText);
	if (!std::filesystem::is_directory(captureRoot))
	{
		std::cerr << "capture root is unavailable\n";
		return 3;
	}
	ExternalEvidenceLog on, off;
	std::string error;
	if (!ParseExternalEvidenceLog(std::filesystem::absolute(onLogText), on, error)
		|| !ParseExternalEvidenceLog(std::filesystem::absolute(offLogText), off, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	std::string onHostLog, offHostLog;
	if (!ReadTextFile(std::filesystem::absolute(onHostLogText), onHostLog)
		|| !ReadTextFile(std::filesystem::absolute(offHostLogText), offHostLog))
	{
		std::cerr << "host evidence log is unavailable\n";
		return 1;
	}
	if (!on.contractEvaluated
		|| onHostLog.find("feature 18 created") == std::string::npos
		|| onHostLog.find("feature 18 evaluation succeeded") == std::string::npos)
	{
		std::cerr << "ON evidence lacks the bounded contract and consumer activity controls\n";
		return 1;
	}
	neuraltest::ExternalConsumerSettings activeSettings;
	if (!neuraltest::ParseExternalConsumerSettingsLog(onHostLog, activeSettings, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	if (!activeSettings.enabled)
	{
		std::cerr << "consumer-reported active settings say DLSS5 is disabled\n";
		return 1;
	}
	if (offHostLog.find("SAFE MODE: EnableHooks=0, all hooks off (no NR)") == std::string::npos)
	{
		std::cerr << "OFF evidence lacks the explicit no-neural host-policy control\n";
		return 1;
	}
	for (const auto& record : on.records)
		if (!ShaMatches(record.gitSha, expectedSha))
		{
			std::cerr << "ON evidence Git SHA does not match the requested build\n";
			return 1;
		}
	for (const auto& record : off.records)
		if (!ShaMatches(record.gitSha, expectedSha))
		{
			std::cerr << "OFF evidence Git SHA does not match the requested build\n";
			return 1;
		}

	struct Promotion {
		std::filesystem::path manifest;
		std::string json;
		const ExternalEvidenceRecord *on = nullptr;
		const ExternalEvidenceRecord *off = nullptr;
		std::filesystem::path imagePending;
		std::filesystem::path manifestPending;
	};
	std::vector<Promotion> promotions;
	std::error_code ec;
	for (std::filesystem::recursive_directory_iterator iterator(captureRoot, ec), end;
		!ec && iterator != end; iterator.increment(ec))
	{
		if (!iterator->is_regular_file() || iterator->path().filename() != "manifest.json")
			continue;
		Promotion promotion;
		promotion.manifest = iterator->path();
		if (!ReadTextFile(promotion.manifest, promotion.json)
			|| !ShaMatches(JsonStringField(promotion.json, "git_sha"), expectedSha)
			|| JsonScalarField(promotion.json, "external_output_confirmed") != "false"
			|| JsonScalarField(promotion.json, "external_contract_evaluated") != "true"
			|| JsonScalarField(promotion.json, "public_output_present") != "true")
		{
			std::cerr << "capture manifest is not an eligible unconfirmed external candidate: "
				<< promotion.manifest.string() << '\n';
			return 1;
		}
		const auto color = JsonStringField(promotion.json, "color_fnv64");
		const auto depth = JsonStringField(promotion.json, "depth_fnv64");
		const auto motion = JsonStringField(promotion.json, "motion_fnv64");
		const auto mask = JsonStringField(promotion.json, "mask_fnv64");
		const auto returned = JsonStringField(promotion.json, "returned_fnv64");
		for (const auto& record : on.records)
			if (record.color == color && record.depth == depth && record.motion == motion
				&& record.mask == mask && record.returned == returned
				&& record.returned != record.marked
				&& on.markerPresentedFrames.count(record.frameId) != 0
				&& on.completedPresentFrames.count(record.frameId) != 0)
			{
				promotion.on = &record;
				break;
			}
		if (!promotion.on)
		{
			std::cerr << "no exact ON mutation-plus-presentation proof for "
				<< promotion.manifest.string() << '\n';
			return 1;
		}
		for (const auto& record : off.records)
			if (record.color == color && record.depth == depth && record.motion == motion
				&& record.mask == mask && record.returned != returned)
			{
				promotion.off = &record;
				break;
			}
		if (!promotion.off)
		{
			std::cerr << "no exact-input policy-OFF difference proof for "
				<< promotion.manifest.string() << '\n';
			return 1;
		}
		const auto frameRoot = promotion.manifest.parent_path();
		if (!std::filesystem::is_regular_file(frameRoot / "public-dlaa-output.png")
			|| std::filesystem::exists(frameRoot / "neural-rendering-output.png"))
		{
			std::cerr << "candidate output is missing or already promoted\n";
			return 1;
		}
		promotions.push_back(std::move(promotion));
	}
	if (ec || promotions.empty())
	{
		std::cerr << (ec ? "capture scan failed: " + ec.message()
			: "capture contains no manifests") << '\n';
		return 1;
	}

	for (auto& promotion : promotions)
	{
		const auto frameRoot = promotion.manifest.parent_path();
		if (!ReplaceExactlyOnce(promotion.json, "\"external_output_confirmed\": false",
			"\"external_output_confirmed\": true")
			|| !ReplaceExactlyOnce(promotion.json, "\"neural_rendering_output_present\": false",
				"\"neural_rendering_output_present\": true"))
		{
			std::cerr << "manifest promotion fields are ambiguous\n";
			return 1;
		}
		const std::string proof = ",\n  \"external_output_proof\": {"
			"\n    \"schema\": 1,"
			"\n    \"method\": \"exact-input ON/OFF mutation plus same-frame sentinel Present\","
			"\n    \"git_sha\": \"" + expectedSha + "\","
			"\n    \"on_frame_id\": " + std::to_string(promotion.on->frameId) + ","
			"\n    \"off_frame_id\": " + std::to_string(promotion.off->frameId) + ","
			"\n    \"on_returned_fnv64\": \"" + promotion.on->returned + "\","
			"\n    \"off_returned_fnv64\": \"" + promotion.off->returned + "\","
			"\n    \"sentinel_marker_pixels\": 1024,"
			"\n    \"same_frame_present_completed\": true"
			"\n  },\n" + ExternalSettingsProofJson(activeSettings);
		if (!ReplaceExactlyOnce(promotion.json, ",\n  \"capture_stalls_gpu\": true",
			proof + ",\n  \"capture_stalls_gpu\": true"))
		{
			std::cerr << "manifest proof insertion point is unavailable\n";
			return 1;
		}
		promotion.imagePending = frameRoot / "neural-rendering-output.png.pending";
		promotion.manifestPending = frameRoot / "manifest.json.pending";
		std::filesystem::remove(promotion.imagePending, ec);
		ec.clear();
		std::filesystem::remove(promotion.manifestPending, ec);
		ec.clear();
		std::filesystem::copy_file(frameRoot / "public-dlaa-output.png",
			promotion.imagePending, std::filesystem::copy_options::none, ec);
		if (ec)
		{
			std::cerr << "failed to stage external output: " << ec.message() << '\n';
			return 1;
		}
		std::ofstream manifest(promotion.manifestPending, std::ios::trunc);
		manifest << promotion.json;
		if (!manifest)
		{
			std::filesystem::remove(promotion.imagePending, ec);
			std::cerr << "failed to stage promoted manifest\n";
			return 1;
		}
	}
	for (auto& promotion : promotions)
	{
		const auto frameRoot = promotion.manifest.parent_path();
		const auto manifestBackup = frameRoot / "manifest.json.unconfirmed-backup";
		std::filesystem::remove(manifestBackup, ec);
		ec.clear();
		std::filesystem::rename(promotion.manifest, manifestBackup, ec);
		if (!ec)
			std::filesystem::rename(promotion.imagePending,
				frameRoot / "neural-rendering-output.png", ec);
		if (!ec)
			std::filesystem::rename(promotion.manifestPending, promotion.manifest, ec);
		if (ec)
		{
			std::error_code cleanup;
			std::filesystem::remove(frameRoot / "neural-rendering-output.png", cleanup);
			std::filesystem::remove(promotion.imagePending, cleanup);
			std::filesystem::remove(promotion.manifestPending, cleanup);
			if (!std::filesystem::exists(promotion.manifest))
				std::filesystem::rename(manifestBackup, promotion.manifest, cleanup);
			std::cerr << "failed to commit promoted capture: " << ec.message() << '\n';
			return 1;
		}
		std::filesystem::remove(manifestBackup, ec);
		ec.clear();
	}
	std::ofstream report(captureRoot / "external-confirmation.json");
	report << "{\n  \"schema\": 1,\n  \"git_sha\": \"" << expectedSha
		<< "\",\n  \"confirmed_frames\": " << promotions.size()
		<< ",\n  \"method\": \"exact-input ON/OFF mutation plus same-frame sentinel Present\","
		<< "\n" << ExternalSettingsProofJson(activeSettings) << ","
		<< "\n  \"synchronous_evidence_excluded_from_performance\": true,"
		<< "\n  \"status\": \"confirmed\"\n}\n";
	if (!report)
	{
		std::cerr << "failed to write confirmation report\n";
		return 1;
	}
	std::cout << "external capture confirmed frames=" << promotions.size()
		<< " git_sha=" << expectedSha
		<< " consumer_settings_verified=true\n";
	return 0;
}

int CaptureIndexCommand(const Args& args)
{
	const auto rootText = Value(args, "--root");
	if (rootText.empty())
	{
		std::cerr << "capture-index requires --root DIR\n";
		return 2;
	}
	const auto root = std::filesystem::absolute(rootText);
	if (!std::filesystem::is_directory(root))
	{
		std::cerr << "capture-index root is unavailable\n";
		return 3;
	}
	const auto output = std::filesystem::absolute(Value(args, "--out",
		(root / "comparison-index.html").string()));
	std::vector<std::filesystem::path> manifests;
	std::size_t rejectedManifests = 0;
	std::error_code ec;
	for (std::filesystem::recursive_directory_iterator iterator(root, ec), end;
		!ec && iterator != end; iterator.increment(ec))
	{
		if (!iterator->is_regular_file() || iterator->path().filename() != "manifest.json")
			continue;
		const auto frameRoot = iterator->path().parent_path();
		if (std::filesystem::is_regular_file(frameRoot / "source-color.png")
			&& std::filesystem::is_regular_file(frameRoot / "final-composited.png"))
		{
			std::ifstream stream(iterator->path());
			const std::string json((std::istreambuf_iterator<char>(stream)), {});
			if (!stream.is_open() || !JsonUnsignedArrayField(json, "render_size", 2)
				|| !JsonUnsignedArrayField(json, "output_size", 2)
				|| !JsonUnsignedArrayField(json, "content_rect", 4))
			{
				++rejectedManifests;
				continue;
			}
			manifests.push_back(iterator->path());
		}
	}
	if (ec)
	{
		std::cerr << "capture-index scan failed: " << ec.message() << '\n';
		return 1;
	}
	std::sort(manifests.begin(), manifests.end());
	std::filesystem::create_directories(output.parent_path(), ec);
	if (ec)
	{
		std::cerr << "capture-index output directory failed: " << ec.message() << '\n';
		return 1;
	}
	std::ofstream html(output);
	if (!html)
	{
		std::cerr << "capture-index output is unwritable\n";
		return 1;
	}
	html << "<!doctype html><meta charset=\"utf-8\"><title>Flycast neural quality captures</title>"
		"<style>body{font:14px system-ui;background:#111;color:#eee;margin:24px}"
		"h1{margin-bottom:4px}.note{color:#bbb}.card{border:1px solid #444;margin:18px 0;padding:14px}"
		".images{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:10px}"
		"figure{margin:0;background:#1b1b1b;padding:8px}img{width:100%;image-rendering:auto}"
		"figcaption{margin-top:5px;color:#ccc}a{color:#8cf}code{overflow-wrap:anywhere}</style>"
		"<h1>Flycast neural quality captures</h1><p class=\"note\">Evidence index only; no winner is inferred from still frames.</p>";
	static constexpr const char *artifacts[][2] = {
		{"native-pvr-color.png", "Native PVR"}, {"source-color.png", "Contract source"},
		{"depth.png", "Depth"}, {"motion.png", "Motion"},
		{"bias-mask.png", "Bias mask"}, {"confidence.png", "Confidence"},
		{"draw-id.png", "Draw ID"}, {"overlay-classification.png", "Overlay"},
		{"public-dlaa-output.png", "Public DLAA"},
		{"neural-rendering-output.png", "External Neural Rendering"},
		{"final-composited.png", "Final composite"},
		{"native-versus-output-difference.png", "Native/output difference"},
		{"temporal-flicker.png", "Temporal flicker"}
	};
	std::vector<std::string> relativeManifests;
	for (const auto& manifest : manifests)
	{
		std::ifstream stream(manifest);
		const std::string json((std::istreambuf_iterator<char>(stream)), {});
		const auto frameRoot = manifest.parent_path();
		const auto relativeFrame = std::filesystem::relative(frameRoot, output.parent_path(), ec);
		if (ec) { ec.clear(); continue; }
		const auto relativeText = relativeFrame.generic_string();
		relativeManifests.push_back(relativeText + "/manifest.json");
		html << "<section class=\"card\"><h2>" << HtmlEscape(JsonStringField(json, "game_id"))
			<< " - " << HtmlEscape(frameRoot.filename().string()) << "</h2><p><code>"
			<< HtmlEscape(JsonStringField(json, "profile")) << "</code><br>"
			<< HtmlEscape(JsonStringField(json, "renderer")) << " / "
			<< HtmlEscape(JsonStringField(json, "api")) << "; accepted="
			<< HtmlEscape(JsonScalarField(json, "evaluation_accepted")) << "; external="
			<< HtmlEscape(JsonScalarField(json, "external_output_confirmed"))
			<< "; status=" << HtmlEscape(JsonStringField(json, "submit_status"))
			<< "<br><code>"
			<< HtmlEscape(relativeText) << "</code><br><a href=\"" << HtmlEscape(relativeText)
			<< "/manifest.json\">manifest</a> | <a href=\"" << HtmlEscape(relativeText)
			<< "/metrics.json\">metrics</a></p><div class=\"images\">";
		for (const auto& artifact : artifacts)
		{
			if (!std::filesystem::is_regular_file(frameRoot / artifact[0])) continue;
			html << "<figure><img loading=\"lazy\" src=\"" << HtmlEscape(relativeText)
				<< '/' << artifact[0] << "\"><figcaption>" << artifact[1]
				<< "</figcaption></figure>";
		}
		html << "</div></section>";
	}
	html << "<p class=\"note\">Packages: " << manifests.size()
		<< "; rejected malformed packages: " << rejectedManifests
		<< ". Review moving sequences and numeric components before acceptance.</p>";
	if (!html)
	{
		std::cerr << "capture-index HTML write failed\n";
		return 1;
	}
	auto jsonOutput = output;
	jsonOutput.replace_extension(".json");
	std::ofstream report(jsonOutput);
	report << "{\n  \"schema\": 1,\n  \"winner_declared\": false,\n  \"package_count\": "
		<< relativeManifests.size() << ",\n  \"rejected_package_count\": "
		<< rejectedManifests << ",\n  \"manifests\": [";
	for (std::size_t i = 0; i < relativeManifests.size(); ++i)
	{
		if (i != 0) report << ',';
		report << "\n    \"" << relativeManifests[i] << "\"";
	}
	report << "\n  ]\n}\n";
	if (!report)
	{
		std::cerr << "capture-index JSON write failed\n";
		return 1;
	}
	std::cout << "capture index packages=" << manifests.size()
		<< " rejected=" << rejectedManifests
		<< " html=" << output.string() << '\n';
	return manifests.empty() ? 3 : 0;
}

int DepthContractCommand(const Args& args)
{
	const auto api = Value(args, "--api", "d3d11");
	const auto output = Value(args, "--out");
	if ((api != "d3d11" && api != "d3d11on12") || output.empty())
	{
		std::cerr << "depth-contract requires --api d3d11|d3d11on12 and --out DIR\n";
		return 2;
	}
	neuraltest::DepthContractResult result;
	std::string error;
	if (!neuraltest::RunDepthContractFixture(api == "d3d11on12", result, error))
	{
		std::cerr << (error.empty() ? "depth contract assertions failed" : error) << '\n';
		return 1;
	}
	std::filesystem::create_directories(output);
	if (!neuraltest::WritePng(std::filesystem::path(output) / "correct-color.png",
		result.correctColor, error) || !neuraltest::WritePng(std::filesystem::path(output) /
		"reversed-color.png", result.reversedColor, error) ||
		!neuraltest::WritePng(std::filesystem::path(output) / "wrong-polarity-color.png",
		result.wrongColor, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	auto writeRaw = [&](const char *name, const std::vector<float>& values) {
		std::ofstream file(std::filesystem::path(output) / name, std::ios::binary);
		file.write(reinterpret_cast<const char *>(values.data()),
			static_cast<std::streamsize>(values.size() * sizeof(float)));
		return file.good();
	};
	if (!writeRaw("correct-depth-r32.raw", result.correctDepth) ||
		!writeRaw("reversed-depth-r32.raw", result.reversedDepth) ||
		!writeRaw("wrong-polarity-depth-r32.raw", result.wrongDepth))
	{
		std::cerr << "failed to write depth-contract raw artifacts\n";
		return 1;
	}
	std::ofstream report(std::filesystem::path(output) / "depth-contract.json");
	report << std::setprecision(9) << "{\n  \"surface\": \"" << result.surface
		<< "\",\n  \"adapter\": \"" << result.adapter << "\",\n  \"samples\": {"
		<< "\n    \"clear\": " << result.clearDepth << ",\n    \"far_opaque\": "
		<< result.farDepth << ",\n    \"near_opaque\": " << result.nearDepth
		<< ",\n    \"near_punch_through\": " << result.punchDepth
		<< ",\n    \"expected_far\": " << result.expectedFarDepth
		<< ",\n    \"expected_near\": " << result.expectedNearDepth << "\n  },"
		<< "\n  \"near_is_greater\": true,\n  \"clear_is_no_geometry\": true,"
		<< "\n  \"visible_ordering_agrees\": true,\n  \"punch_through_agrees\": true,"
		<< "\n  \"reversed_submission_stable\": true,\n  \"wrong_polarity_failed\": true,"
		<< "\n  \"native_export_exact\": true\n}\n";
	std::cout << std::setprecision(9) << "surface=" << result.surface << " clear="
		<< result.clearDepth << " far=" << result.farDepth << " near=" << result.nearDepth
		<< " punch=" << result.punchDepth << " near_is_greater=yes wrong_control=failed\n";
	return 0;
}

int MotionContractCommand(const Args& args)
{
	const auto output = Value(args, "--out");
	if (output.empty())
	{
		std::cerr << "motion-contract requires --out DIR\n";
		return 2;
	}
	neuraltest::MotionContractResult result;
	std::string error;
	if (!neuraltest::RunMotionContractFixture(result, error))
	{
		std::cerr << (error.empty() ? "motion contract assertions failed" : error) << '\n';
		return 1;
	}
	std::filesystem::create_directories(output);
	if (!neuraltest::WritePng(std::filesystem::path(output) / "previous-color.png",
		result.previousColor, error) || !neuraltest::WritePng(std::filesystem::path(output) /
		"current-color.png", result.currentColor, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	auto writeMotion = [&](const char *name, const std::vector<std::uint16_t>& values) {
		std::ofstream file(std::filesystem::path(output) / name, std::ios::binary);
		file.write(reinterpret_cast<const char *>(values.data()),
			static_cast<std::streamsize>(values.size() * sizeof(std::uint16_t)));
		return file.good();
	};
	if (!writeMotion("correct-motion-rg16f.raw", result.correctMotion) ||
		!writeMotion("reversed-motion-rg16f.raw", result.reversedMotion) ||
		!writeMotion("doubled-motion-rg16f.raw", result.doubledMotion))
	{
		std::cerr << "failed to write motion-contract raw artifacts\n";
		return 1;
	}
	std::ofstream report(std::filesystem::path(output) / "motion-contract.json");
	report << std::setprecision(9) << "{\n  \"adapter\": \"" << result.adapter
		<< "\",\n  \"samples\": {\n    \"static\": [" << result.staticX << ','
		<< result.staticY << "],\n    \"translate_plus_4_x\": [" << result.translateX
		<< ',' << result.translateY << "],\n    \"translate_minus_3_y\": ["
		<< result.verticalX << ',' << result.verticalY << "],\n    \"camera\": ["
		<< result.cameraX << ',' << result.cameraY << "],\n    \"deformation\": ["
		<< result.deformationX << ',' << result.deformationY
		<< "],\n    \"deformation_expected\": [" << result.expectedDeformationX << ','
		<< result.expectedDeformationY << "],\n    \"jitter_only\": [" << result.jitterX
		<< ',' << result.jitterY << "]\n  },\n  \"reprojection_mae\": {"
		<< "\n    \"correct\": " << result.correctReprojectionError
		<< ",\n    \"reversed\": " << result.reversedReprojectionError
		<< ",\n    \"doubled\": " << result.doubledReprojectionError
		<< "\n  },\n  \"analytic_truth\": true,\n  \"negative_controls_fail\": true\n}\n";
	std::cout << std::setprecision(9) << "static=[" << result.staticX << ',' << result.staticY
		<< "] plus4x=[" << result.translateX << ',' << result.translateY << "] minus3y=["
		<< result.verticalX << ',' << result.verticalY << "] camera=[" << result.cameraX
		<< ',' << result.cameraY << "] reprojection_mae=" << result.correctReprojectionError
		<< " reversed=" << result.reversedReprojectionError << " doubled="
		<< result.doubledReprojectionError << '\n';
	return 0;
}

int ColorContractCommand(const Args& args)
{
	const auto output = Value(args, "--out");
	if (output.empty())
	{
		std::cerr << "color-contract requires --out DIR\n";
		return 2;
	}
	neuraltest::ColorContractResult result;
	std::string error;
	if (!neuraltest::RunColorContractFixture(result, error))
	{
		std::cerr << (error.empty() ? "color contract assertions failed" : error) << '\n';
		return 1;
	}
	std::filesystem::create_directories(output);
	if (!neuraltest::WritePng(std::filesystem::path(output) / "source-color.png",
		result.source, error) || !neuraltest::WritePng(std::filesystem::path(output) /
		"roundtrip-color.png", result.roundTrip, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	std::ofstream report(std::filesystem::path(output) / "color-contract.json");
	report << "{\n  \"adapter\": \"" << result.adapter
		<< "\",\n  \"format\": \"R8G8B8A8_UNORM\","
		<< "\n  \"pre_exposure\": 1,\n  \"exposure_scale\": 1,"
		<< "\n  \"byte_exact\": true,\n  \"channels_exact\": true,"
		<< "\n  \"grayscale_exact\": true,\n  \"alpha_independent_rgb\": true,"
		<< "\n  \"content_rectangles_exact\": true,"
		<< "\n  \"differing_pixels\": " << result.differingPixels
		<< ",\n  \"max_delta\": " << static_cast<unsigned>(result.maxDelta) << "\n}\n";
	if (!report.good())
	{
		std::cerr << "failed to write color-contract report\n";
		return 1;
	}
	std::cout << "format=R8G8B8A8_UNORM byte_exact=yes channels_exact=yes"
		<< " grayscale_exact=yes alpha_independent_rgb=yes content_rectangles_exact=yes"
		<< " differing_pixels=" << result.differingPixels << " max_delta="
		<< static_cast<unsigned>(result.maxDelta) << '\n';
	return 0;
}

int DisocclusionContractCommand(const Args& args)
{
	const auto api = Value(args, "--api", "d3d11");
	const auto output = Value(args, "--out");
	if ((api != "d3d11" && api != "d3d11on12") || output.empty())
	{
		std::cerr << "disocclusion-contract requires --api d3d11|d3d11on12 and --out DIR\n";
		return 2;
	}
	neuraltest::DisocclusionContractResult result;
	std::string error;
	if (!neuraltest::RunDisocclusionContractFixture(api == "d3d11on12", result, error))
	{
		std::cerr << (error.empty() ? "disocclusion contract assertions failed" : error) << '\n';
		return 1;
	}
	std::filesystem::create_directories(output);
	if (!neuraltest::WritePng(std::filesystem::path(output) / "resolved-mask.png",
		result.resolvedMask, error) || !neuraltest::WritePng(std::filesystem::path(output)
		/ "wrong-disocclusion-mask.png", result.wrongMask, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	std::ofstream report(std::filesystem::path(output) / "disocclusion-contract.json");
	report << "{\n  \"surface\": \"" << result.surface << "\",\n  \"adapter\": \""
		<< result.adapter << "\",\n  \"static_trusted\": true,"
		<< "\n  \"camera_pan_trusted\": true,\n  \"depth_tolerance_trusted\": true,"
		<< "\n  \"outside_protected\": true,"
		<< "\n  \"depth_mismatch_protected\": true,\n  \"crossing_protected\": true,"
		<< "\n  \"reveal_protected\": true,\n  \"newly_visible_protected\": true,"
		<< "\n  \"scene_cut_protected\": true,\n  \"protected_pixels\": "
		<< result.protectedPixels << ",\n  \"wrong_missed_pixels\": "
		<< result.wrongMissedPixels << ",\n  \"correct_trail_energy\": "
		<< result.correctTrailEnergy << ",\n  \"wrong_trail_energy\": "
		<< result.wrongTrailEnergy << "\n}\n";
	if (!report.good()) { std::cerr << "failed to write disocclusion report\n"; return 1; }
	std::cout << "surface=" << result.surface << " protected_pixels="
		<< result.protectedPixels << " wrong_missed_pixels=" << result.wrongMissedPixels
		<< " correct_trail_energy=" << result.correctTrailEnergy
		<< " wrong_trail_energy=" << result.wrongTrailEnergy << '\n';
	return 0;
}

int TransparencyContractCommand(const Args& args)
{
	const auto api = Value(args, "--api", "d3d11");
	const auto output = Value(args, "--out");
	if ((api != "d3d11" && api != "d3d11on12") || output.empty())
	{
		std::cerr << "transparency-contract requires --api d3d11|d3d11on12 and --out DIR\n";
		return 2;
	}
	neuraltest::TransparencyContractResult result;
	std::string error;
	if (!neuraltest::RunTransparencyContractFixture(api == "d3d11on12", result, error))
	{
		std::cerr << (error.empty() ? "transparency contract assertions failed" : error) << '\n';
		return 1;
	}
	std::filesystem::create_directories(output);
	neuraltest::Image wrong = result.reactiveMask;
	for (std::size_t i = 0; i < wrong.rgba.size(); i += 4)
		wrong.rgba[i] = wrong.rgba[i + 1] = wrong.rgba[i + 2] = 0;
	if (!neuraltest::WritePng(std::filesystem::path(output) / "reactive-mask.png",
		result.reactiveMask, error) || !neuraltest::WritePng(std::filesystem::path(output)
		/ "wrong-omitted-mask.png", wrong, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	std::ofstream report(std::filesystem::path(output) / "transparency-contract.json");
	report << "{\n  \"surface\": \"" << result.surface << "\",\n  \"adapter\": \""
		<< result.adapter << "\",\n  \"empty_modifier_clear\": true,"
		<< "\n  \"single_layer_reactive\": true,\n  \"multi_layer_reactive\": true,"
		<< "\n  \"wrong_omitted_mask_failed\": true,"
		<< "\n  \"merge_preserves_base_mask\": true\n}\n";
	if (!report.good()) { std::cerr << "failed to write transparency report\n"; return 1; }
	std::cout << "surface=" << result.surface
		<< " empty_modifier_clear=yes single_layer_reactive=yes"
		<< " multi_layer_reactive=yes wrong_omitted_mask_failed=yes"
		<< " merge_preserves_base_mask=yes\n";
	return 0;
}

int OverlayContractCommand(const Args& args)
{
	const auto api = Value(args, "--api", "d3d11");
	const auto output = Value(args, "--out");
	if ((api != "d3d11" && api != "d3d11on12") || output.empty())
	{
		std::cerr << "overlay-contract requires --api d3d11|d3d11on12 and --out DIR\n";
		return 2;
	}
	neuraltest::OverlayContractResult result;
	std::string error;
	if (!neuraltest::RunOverlayContractFixture(api == "d3d11on12", result, error))
	{
		std::cerr << (error.empty() ? "overlay contract assertions failed" : error) << '\n';
		return 1;
	}
	std::filesystem::create_directories(output);
	if (!neuraltest::WritePng(std::filesystem::path(output) / "original-scene.png",
		result.original, error)
		|| !neuraltest::WritePng(std::filesystem::path(output) / "neural-scene.png",
			result.neural, error)
		|| !neuraltest::WritePng(std::filesystem::path(output) / "overlay-mask.png",
			result.mask, error)
		|| !neuraltest::WritePng(std::filesystem::path(output) / "composited.png",
			result.composited, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	std::ofstream report(std::filesystem::path(output) / "overlay-contract.json");
	report << "{\n  \"surface\": \"" << result.surface << "\",\n  \"adapter\": \""
		<< result.adapter << "\",\n  \"protected_pixels\": " << result.protectedPixels
		<< ",\n  \"protected_mismatch\": " << result.protectedMismatch
		<< ",\n  \"world_changed\": " << result.worldChanged
		<< ",\n  \"wrong_protected_mismatch\": " << result.wrongProtectedMismatch
		<< "\n}\n";
	if (!report.good()) { std::cerr << "failed to write overlay report\n"; return 1; }
	std::cout << "surface=" << result.surface << " protected_pixels="
		<< result.protectedPixels << " protected_mismatch=" << result.protectedMismatch
		<< " world_changed=" << result.worldChanged << " wrong_protected_mismatch="
		<< result.wrongProtectedMismatch << '\n';
	return 0;
}

int NeuralCommand(const Args& args)
{
	using namespace flycast::rend::neural;
	const auto input = Value(args, "--in");
	const auto output = Value(args, "--out");
	const auto backend = Value(args, "--backend", "passthrough");
	const auto api = Value(args, "--api", "d3d11");
	const auto mode = Value(args, "--mode", "quality");
	const auto depthPolarity = Value(args, "--depth-polarity", "inverted");
	const auto preset = Value(args, "--preset", "auto");
	const std::uint32_t dlssPreset = preset == "j" ? 10u : preset == "k" ? 11u : 0u;
	const auto effectiveMode = backend == "sr" ? mode : backend;
	if (input.empty() || output.empty())
	{
		std::cerr << "neural requires --in and --out\n";
		return 2;
	}
	if (api != "d3d11" && api != "d3d12")
	{
		std::cerr << "--api must be d3d11 or d3d12\n";
		return 2;
	}
	if (backend != "passthrough" && backend != "dlaa" && backend != "dlaa-hook"
		&& backend != "dlss5-hook" && backend != "sr")
	{
		std::cerr << "unsupported --backend value\n";
		return 2;
	}
	if (mode != "quality" && mode != "balanced" && mode != "performance"
		&& mode != "ultra-performance")
	{
		std::cerr << "unsupported --mode value\n";
		return 2;
	}
	if (depthPolarity != "inverted" && depthPolarity != "normal")
	{
		std::cerr << "--depth-polarity must be inverted or normal\n";
		return 2;
	}
	if (preset != "auto" && preset != "j" && preset != "k")
	{
		std::cerr << "--preset must be auto, j, or k\n";
		return 2;
	}
	neuraltest::Image image;
	neuraltest::Image previousImage;
	std::string error;
	if (!neuraltest::ReadPng(ResolveImage(input), image, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	const auto previousInput = Value(args, "--previous-in");
	if (!previousInput.empty() && !neuraltest::ReadPng(ResolveImage(previousInput),
		previousImage, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	if (!previousInput.empty() && (previousImage.width != image.width ||
		previousImage.height != image.height))
	{
		std::cerr << "--previous-in dimensions must match --in\n";
		return 2;
	}
	std::uint32_t frames = 1;
	std::uint32_t outputWidth = image.width;
	std::uint32_t outputHeight = image.height;
	float motionX = 0.f;
	float motionY = 0.f;
	if (!Number(args, "--frames", 1, frames, error)
		|| !Number(args, "--output-width", image.width, outputWidth, error)
		|| !Number(args, "--output-height", image.height, outputHeight, error)
		|| !Scalar(args, "--motion-x", 0.f, motionX, error)
		|| !Scalar(args, "--motion-y", 0.f, motionY, error)
		|| frames == 0 || outputWidth == 0 || outputHeight == 0)
	{
		std::cerr << (error.empty() ? "frame count and output dimensions must be positive" : error) << '\n';
		return 2;
	}
	if (backend != "passthrough")
	{
		std::filesystem::create_directories(output);
		neuraltest::NeuralRunResult run;
		if (!(api == "d3d12"
			? neuraltest::RunLiveNeuralD3D12(image, backend, mode, outputWidth, outputHeight,
				args.count("--no-ngx") != 0, args.count("--warp") != 0,
				depthPolarity == "inverted", previousInput.empty() ? nullptr : &previousImage,
				motionX, motionY, frames, dlssPreset, run, error)
			: neuraltest::RunLiveNeuralD3D11(image, backend, mode, outputWidth, outputHeight,
				args.count("--no-ngx") != 0, args.count("--warp") != 0,
				depthPolarity == "inverted", previousInput.empty() ? nullptr : &previousImage,
				motionX, motionY, frames, dlssPreset, run, error)))
		{
			std::cerr << error << '\n';
			return 1;
		}
		std::ofstream statusFile(std::filesystem::path(output) / "ngx-status.json");
		statusFile << "{\n  \"backend\": \"" << backend << "\",\n  \"mode\": \"" << effectiveMode
			<< "\",\n  \"api\": \"" << api
			<< "\",\n  \"depth_polarity\": \"" << depthPolarity
			<< "\",\n  \"preset\": \"" << preset
			<< "\",\n  \"motion\": [" << motionX << ", " << motionY << ']'
			<< ",\n  \"surface\": \"" << run.surface
			<< "\",\n  \"status\": \"" << run.status << "\",\n  \"adapter\": \"" << run.adapter
			<< "\",\n  \"reason\": \"" << run.reason << "\",\n  \"requested_frames\": " << frames
			<< ",\n  \"submissions\": " << run.submissions << ",\n  \"busy_skips\": " << run.busySkips
			<< ",\n  \"fallbacks\": " << run.fallbacks << ",\n  \"last_ngx_result\": "
			<< run.lastNgxResult << ",\n  \"last_exception_code\": " << run.lastExceptionCode
			<< ",\n  \"compatibility_rebuilds\": " << run.compatibilityRebuilds
			<< ",\n  \"compatibility_rebuild_attempts\": " << run.compatibilityRebuildAttempts
			<< ",\n  \"compatibility_rebuild_failures\": " << run.compatibilityRebuildFailures
			<< ",\n  \"compatibility_rebuild_reason\": \""
			<< Dlss5RebuildReasonName(run.compatibilityRebuildReason) << "\""
			<< ",\n  \"dlss5_contract_evaluated\": " << (run.dlss5ContractEvaluated ? "true" : "false")
			<< ",\n  \"dlss5_route\": \"" << Dlss5HookRouteName(run.dlss5Route) << "\""
			<< ",\n  \"dlss5_readiness\": \"" << Dlss5HookReadinessName(run.dlss5Readiness) << "\""
			<< ",\n  \"dlss5_components\": {\"reshade\": "
			<< (run.dlss5Components.reshadeHostLoaded ? "true" : "false")
			<< ", \"interceptor\": " << (run.dlss5Components.interceptorLoaded ? "true" : "false")
			<< ", \"neural_runtime\": " << (run.dlss5Components.neuralRuntimeLoaded ? "true" : "false")
			<< ", \"dlss_runtime\": " << (run.dlss5Components.dlssRuntimeLoaded ? "true" : "false") << "}"
			<< ",\n  \"invalid_frames\": " << run.invalidFrames
			<< ",\n  \"output_changes\": " << run.outputChanges
			<< ",\n  \"max_temporal_changed_pixels\": " << run.maxTemporalChangedPixels
			<< ",\n  \"max_temporal_delta\": " << static_cast<unsigned>(run.maxTemporalDelta)
			<< ",\n  \"min_temporal_psnr\": " << run.minTemporalPsnr
			<< ",\n  \"output_hash\": \"0x" << std::hex << run.outputHash << std::dec << "\"\n}\n";
		std::ofstream report(std::filesystem::path(output) / "report.md");
		report << "# neuraltest neural report\n\nBackend: `" << backend << "`  \nMode: `" << effectiveMode
			<< "`  \nAPI: `" << api
			<< "`  \nDepth polarity: `" << depthPolarity
			<< "`  \nPublic DLSS preset: `" << preset
			<< "`  \nMotion: `[" << motionX << ", " << motionY << "]`"
			<< "  \nSurface: `" << run.surface
			<< "`  \nStatus: `" << run.status << "`  \nAdapter: `" << run.adapter
			<< "`  \nRender/output: " << image.width << 'x' << image.height << " -> "
			<< outputWidth << 'x' << outputHeight
			<< "  \nSubmissions: " << run.submissions << "/" << frames
			<< "  \nInvalid frames: " << run.invalidFrames
			<< "  \nOutput changes: " << run.outputChanges << "  \nOutput hash: `0x"
			<< std::hex << run.outputHash << std::dec << "`  \nWorst adjacent frame: "
			<< run.maxTemporalChangedPixels << " pixels, delta "
			<< static_cast<unsigned>(run.maxTemporalDelta) << ", PSNR "
			<< run.minTemporalPsnr << " dB\n\nReason: " << run.reason << "\n";
		if (run.status == "submitted"
			&& !neuraltest::WritePng(std::filesystem::path(output) / "neural_output.png",
				run.output, error))
		{
			std::cerr << error << '\n';
			return 1;
		}
		std::cout << "status=" << run.status << " adapter=\"" << run.adapter
			<< "\" submissions=" << run.submissions << '/' << frames
			<< " ngx_result=" << run.lastNgxResult << " exception=" << run.lastExceptionCode
			<< " invalid_frames=" << run.invalidFrames << " output_hash=0x" << std::hex
			<< run.outputHash << std::dec << " output_changes=" << run.outputChanges
			<< " reason=\"" << run.reason << "\"\n";
		return run.invalidFrames == 0 ? 0 : 1;
	}
	StageConfig config;
	config.mode = NeuralMode::Passthrough;
	config.api = api == "d3d12" ? Api::D3D12 : Api::D3D11;
	config.outputWidth = image.width;
	config.outputHeight = image.height;
	NeuralStage stage(config);
	NeuralFrame frame;
	frame.source = FrameSource::Geometry;
	frame.color.api = api == "d3d12" ? TextureApi::D3D12 : TextureApi::D3D11;
	frame.color.resource = image.rgba.data(); // opaque identity token for the CPU artifact
	frame.renderWidth = frame.outputWidth = image.width;
	frame.renderHeight = frame.outputHeight = image.height;
	const auto status = stage.TrySubmit(frame);
	std::filesystem::create_directories(output);
	std::ofstream statusFile(std::filesystem::path(output) / "ngx-status.json");
	const bool submitted = status == SubmitStatus::Submitted;
	statusFile << "{\n  \"backend\": \"" << backend << "\",\n  \"api\": \"" << api
		<< "\",\n  \"status\": \"" << (submitted ? "submitted" : "unsupported")
		<< "\",\n  \"ngx_enabled\": "
#ifdef FLYCAST_ENABLE_NGX
		<< "true"
#else
		<< "false"
#endif
		<< ",\n  \"note\": \"offline CPU artifact handoff; live NGX evaluation not yet implemented\"\n}\n";
	std::ofstream report(std::filesystem::path(output) / "report.md");
	report << "# neuraltest neural report\n\nBackend: `" << backend << "`  \nAPI: `" << api
		<< "`  \nStatus: `" << (submitted ? "submitted" : "unsupported") << "`\n\n"
		<< "Depth, motion, and mask: no data. Live NGX evaluation is not implemented.\n";
	if (backend == "passthrough")
	{
		if (!submitted || !neuraltest::WritePng(std::filesystem::path(output) / "prepared-color.png", image, error) ||
			!neuraltest::WritePng(std::filesystem::path(output) / "neural_output.png", image, error))
		{
			std::cerr << (error.empty() ? "passthrough stage rejected frame" : error) << '\n';
			return 1;
		}
		std::cout << "passthrough submitted; output is byte-identical to input\n";
		return 0;
	}
	return 0;
}

int CompareCommand(const Args& args)
{
	const auto aPath = Value(args, "--a");
	const auto bPath = Value(args, "--b");
	if (aPath.empty() || bPath.empty())
	{
		std::cerr << "compare requires --a and --b\n";
		return 2;
	}
	if (args.count("--edge-only") != 0)
	{
		std::cerr << "edge-only comparison is unavailable until provenance edge masks exist\n";
		return 3;
	}
	std::string error;
	neuraltest::Image a;
	neuraltest::Image b;
	if (!neuraltest::ReadPng(ResolveImage(aPath), a, error) ||
		!neuraltest::ReadPng(ResolveImage(bPath), b, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	if (a.width != b.width || a.height != b.height)
	{
		std::cerr << "image dimensions differ: " << a.width << 'x' << a.height << " vs "
			<< b.width << 'x' << b.height << '\n';
		return 1;
	}
	std::uint32_t differing = 0;
	std::uint8_t maxDelta = 0;
	const double psnr = neuraltest::ComputePsnr(a, b, differing, maxDelta);
	std::cout << "differing_pixels=" << differing << " max_delta=" << static_cast<unsigned>(maxDelta)
		<< " psnr=" << (std::isinf(psnr) ? "inf" : std::to_string(psnr)) << '\n';
	std::uint32_t maxAllowed = 0;
	if (!Number(args, "--maxabs", 0, maxAllowed, error))
	{
		std::cerr << error << '\n';
		return 2;
	}
	double minPsnr = 0.;
	if (!Value(args, "--psnr").empty())
	{
		try { minPsnr = std::stod(Value(args, "--psnr")); }
		catch (const std::exception&) { std::cerr << "invalid --psnr\n"; return 2; }
	}
	return (maxDelta > maxAllowed || psnr < minPsnr) ? 1 : 0;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc == 2 && std::string(argv[1]) == "--version")
	{
		std::cout << "neuraltest phase-1\n";
		return 0;
	}
	if (argc < 2)
	{
		Usage();
		return 2;
	}
	Args args;
	std::string error;
	if (!ParseArgs(argc, argv, 2, args, error))
	{
		std::cerr << error << '\n';
		return 2;
	}
	const std::string command = argv[1];
	if (command == "selftest") return neuraltest::RunSelfTests();
	if (command == "render") return RenderCommand(args);
	if (command == "determinism") return DeterminismCommand(args);
	if (command == "scaling") return ScalingCommand(args);
	if (command == "depth-contract") return DepthContractCommand(args);
	if (command == "motion-contract") return MotionContractCommand(args);
	if (command == "color-contract") return ColorContractCommand(args);
	if (command == "disocclusion-contract") return DisocclusionContractCommand(args);
	if (command == "transparency-contract") return TransparencyContractCommand(args);
	if (command == "overlay-contract") return OverlayContractCommand(args);
	if (command == "depth" || command == "motion") return NoDataCommand(command, args);
	if (command == "neural") return NeuralCommand(args);
	if (command == "compare") return CompareCommand(args);
	if (command == "native-parity") return NativeParityCommand(args);
	if (command == "production-scaling") return ProductionScalingCommand(args);
	if (command == "capture") return CaptureCommand(args);
	if (command == "capture-index") return CaptureIndexCommand(args);
	if (command == "compare-captures")
		return neuraltest::CompareCaptureSequences(Value(args, "--a"), Value(args, "--b"),
			Value(args, "--out"), Value(args, "--a-output", "external"),
			Value(args, "--b-output", "public"));
	if (command == "confirm-external-capture") return ConfirmExternalCaptureCommand(args);
	if (command == "performance") return PerformanceCommand(args);
	Usage();
	return 2;
}
