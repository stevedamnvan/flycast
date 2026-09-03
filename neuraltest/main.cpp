// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "rend/neural/neural_stage.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>

namespace {

using Args = std::map<std::string, std::string>;

void Usage()
{
	std::cout <<
		"neuraltest render --fixture NAME --renderer dx11|dx11-oit --scale 1|4|8 --frames N --out DIR [--jitter on|off] [--warp]\n"
		"neuraltest determinism --fixture NAME --renderer dx11|dx11-oit [--runs 5] [--warp]\n"
		"neuraltest scaling --fixture NAME --renderer dx11|dx11-oit [--out DIR] [--warp]\n"
		"neuraltest depth|motion --in DIR\n"
		"neuraltest neural --in DIR --out DIR --backend passthrough|dlaa|dlaa-hook|dlss5-hook|sr --api d3d11|d3d12 [--mode quality|balanced|performance|ultra-performance] [--output-width N --output-height N] [--no-ngx] [--warp]\n"
		"neuraltest compare --a DIR|PNG --b DIR|PNG [--maxabs N] [--psnr N] [--edge-only]\n"
		"neuraltest capture --game PATH --frames N --skip M --out DIR\n";
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

int NeuralCommand(const Args& args)
{
	using namespace flycast::rend::neural;
	const auto input = Value(args, "--in");
	const auto output = Value(args, "--out");
	const auto backend = Value(args, "--backend", "passthrough");
	const auto api = Value(args, "--api", "d3d11");
	const auto mode = Value(args, "--mode", "quality");
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
	neuraltest::Image image;
	std::string error;
	if (!neuraltest::ReadPng(ResolveImage(input), image, error))
	{
		std::cerr << error << '\n';
		return 1;
	}
	std::uint32_t frames = 1;
	std::uint32_t outputWidth = image.width;
	std::uint32_t outputHeight = image.height;
	if (!Number(args, "--frames", 1, frames, error)
		|| !Number(args, "--output-width", image.width, outputWidth, error)
		|| !Number(args, "--output-height", image.height, outputHeight, error)
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
				args.count("--no-ngx") != 0, args.count("--warp") != 0, frames, run, error)
			: neuraltest::RunLiveNeuralD3D11(image, backend, mode, outputWidth, outputHeight,
				args.count("--no-ngx") != 0, args.count("--warp") != 0, frames, run, error)))
		{
			std::cerr << error << '\n';
			return 1;
		}
		std::ofstream statusFile(std::filesystem::path(output) / "ngx-status.json");
		statusFile << "{\n  \"backend\": \"" << backend << "\",\n  \"mode\": \"" << effectiveMode
			<< "\",\n  \"api\": \"" << api
			<< "\",\n  \"surface\": \"" << run.surface
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
			<< "`  \nSurface: `" << run.surface
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
	if (command == "depth" || command == "motion") return NoDataCommand(command, args);
	if (command == "neural") return NeuralCommand(args);
	if (command == "compare") return CompareCommand(args);
	if (command == "capture")
	{
		std::cerr << "capture unavailable until the FC-054 emulator CLI is implemented\n";
		return 3;
	}
	Usage();
	return 2;
}
