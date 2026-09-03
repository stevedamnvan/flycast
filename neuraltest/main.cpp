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
		"neuraltest capture --game PATH --frames N --skip M --out DIR [--flycast EXE] [--lane native|dlaa|sr-quality|dlss5] [--api d3d11|d3d11on12] [--renderer dx11|dx11-oit] [--preset auto|j|k] [--profile faithful|enhanced|photoreal] [--style auto|realistic|stylized|cel|racing|particles|sprite-2d|mixed-video] [--render-height N] [--feature-path DIR] [--timeout-ms N]\n"
		"neuraltest capture-index --root DIR [--out HTML]\n";
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
		if (key == "--warp" || key == "--no-ngx" || key == "--edge-only")
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
#endif

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
	std::uint32_t frames = 0, skip = 0, timeoutMs = 120000, renderHeight = 480;
	if (!Number(args, "--frames", 0, frames, error) || frames == 0 || frames > 240
		|| !Number(args, "--skip", 0, skip, error)
		|| !Number(args, "--render-height", 480, renderHeight, error)
		|| renderHeight < 120 || renderHeight > 8640
		|| !Number(args, "--timeout-ms", 120000, timeoutMs, error) || timeoutMs < 1000)
	{
		std::cerr << (error.empty() ? "--frames must be 1..240, --render-height 120..8640, and --timeout-ms at least 1000" : error) << '\n';
		return 2;
	}
	const auto lane = Value(args, "--lane", "dlaa");
	const auto api = Value(args, "--api", "d3d11");
	const auto renderer = Value(args, "--renderer", "dx11");
	const auto preset = Value(args, "--preset", "auto");
	const auto profile = Value(args, "--profile", "faithful");
	const auto style = Value(args, "--style", "auto");
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
	const int mode = lane == "native" ? 1 : lane == "dlaa" ? 2
		: lane == "sr-quality" ? 4 : 8;
	const int rendererValue = renderer == "dx11-oit" ? 6 : 2;
	const int presetValue = preset == "j" ? 10 : preset == "k" ? 11 : 0;
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
		+ L",config:rend.NeuralDlss5EvidenceCapture=no"
		+ L",config:rend.NeuralDlssPreset=" + std::to_wstring(presetValue)
		+ L",config:rend.NeuralQualityProfile=" + std::to_wstring(profileValue)
		+ L",config:rend.NeuralStyleFamily=" + std::to_wstring(styleValue)
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
	std::ofstream launchReport(output / "capture-launch.json");
	launchReport << "{\n  \"schema\": 1,\n  \"lane\": \"" << lane
		<< "\",\n  \"api\": \"" << api << "\",\n  \"renderer\": \"" << renderer
		<< "\",\n  \"preset\": \"" << preset
		<< "\",\n  \"profile\": \"" << profile
		<< "\",\n  \"style\": \"" << style
		<< "\",\n  \"render_height\": " << renderHeight
		<< ",\n  \"requested_frames\": " << frames
		<< ",\n  \"skip\": " << skip
		<< ",\n  \"clean_window_close\": " << (forcedTermination ? "false" : "true")
		<< ",\n  \"media_path_recorded\": false\n}\n";
	std::cout << "capture complete frames=" << frames << " lane=" << lane
		<< " api=" << api << " renderer=" << renderer
		<< " clean_close=" << (forcedTermination ? "no" : "yes") << '\n';
	return launchReport ? 0 : 1;
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
	std::error_code ec;
	for (std::filesystem::recursive_directory_iterator iterator(root, ec), end;
		!ec && iterator != end; iterator.increment(ec))
	{
		if (!iterator->is_regular_file() || iterator->path().filename() != "manifest.json")
			continue;
		const auto frameRoot = iterator->path().parent_path();
		if (std::filesystem::is_regular_file(frameRoot / "source-color.png")
			&& std::filesystem::is_regular_file(frameRoot / "final-composited.png"))
			manifests.push_back(iterator->path());
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
			<< HtmlEscape(JsonScalarField(json, "external_contract_evaluated"))
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
		<< relativeManifests.size() << ",\n  \"manifests\": [";
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
	if (command == "capture") return CaptureCommand(args);
	if (command == "capture-index") return CaptureIndexCommand(args);
	Usage();
	return 2;
}
