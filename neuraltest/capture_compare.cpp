// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"
#include "json/json.hpp"
#include "stb_image.h"
#include "version.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace neuraltest {
namespace {
using Json = nlohmann::json;
constexpr const char *InputNames[] = {"color_fnv64", "depth_fnv64", "motion_fnv64", "mask_fnv64"};

void Require(bool value, const std::string& reason)
{
	if (!value) throw std::runtime_error(reason);
}

Json ParseManifest(const std::string& text)
{
	bool duplicate = false;
	std::vector<std::set<std::string>> keys;
	auto callback = [&](int, Json::parse_event_t event, Json& parsed) {
		if (event == Json::parse_event_t::object_start) keys.emplace_back();
		if (event == Json::parse_event_t::key)
			duplicate |= !keys.back().insert(parsed.get<std::string>()).second;
		if (event == Json::parse_event_t::object_end) keys.pop_back();
		return true;
	};
	auto json = Json::parse(text, callback);
	Require(!duplicate, "manifest contains duplicate JSON keys");
	Require(json.is_object(), "manifest must be an object");
	return json;
}

bool IsHash(const Json& value)
{
	return value.is_string() && std::regex_match(value.get<std::string>(),
		std::regex("[0-9A-F]{16}"));
}

bool IsSha(const Json& value)
{
	return value.is_string() && std::regex_match(value.get<std::string>(),
		std::regex("[0-9a-f]{7,40}"));
}

bool SameSha(const Json& a, const Json& b)
{
	if (!IsSha(a) || !IsSha(b)) return false;
	const auto x = a.get<std::string>(), y = b.get<std::string>();
	return x.substr(0, std::min(x.size(), y.size())) == y.substr(0, std::min(x.size(), y.size()));
}

void UnsignedArray(const Json& value, std::size_t count)
{
	Require(value.is_array() && value.size() == count, "invalid dimension/rectangle array");
	for (const auto& item : value)
		Require(item.is_number_unsigned() && item.get<std::uint64_t>() <= 16384,
			"invalid dimension/rectangle component");
}

void ValidateManifest(const Json& j, const std::string& output)
{
	Require(j.at("schema") == 3 && IsSha(j.at("git_sha")), "expected schema-3 capture and Git SHA");
	Require(j.at("game_id").is_string() && !j.at("game_id").get<std::string>().empty(), "missing game ID");
	Require(j.at("frame_id").is_number_unsigned(), "frame ID must be unsigned");
	Require(j.at("evaluation_accepted") == true && j.at("public_output_present") == true,
		"capture has no accepted public-contract output");
	Require(j.at("capture_stalls_gpu") == true && j.at("eligible_for_performance_metrics") == false,
		"capture lacks synchronous/performance-ineligible labels");
	UnsignedArray(j.at("render_size"), 2);
	UnsignedArray(j.at("output_size"), 2);
	UnsignedArray(j.at("content_rect"), 4);
	Require(j.at("render_size") == j.at("output_size"), "comparison requires target-native captures; no implicit resampling");
	const auto width = j.at("render_size")[0].get<std::uint64_t>();
	const auto height = j.at("render_size")[1].get<std::uint64_t>();
	Require(width > 0 && height > 0 && width * height <= 32 * 1024 * 1024,
		"capture dimensions exceed bounded image size");
	Require(j.at("content_rect")[2] == width && j.at("content_rect")[3] == height,
		"content rectangle does not match target output");
	for (const auto name : InputNames)
		Require(IsHash(j.at("contract_hashes").at(name)), "invalid input contract hash");
	Require(IsHash(j.at("contract_hashes").at("returned_fnv64")), "invalid returned hash");
	if (output == "external")
	{
		Require(j.at("external_contract_evaluated") == true && j.at("external_output_confirmed") == true
			&& j.at("neural_rendering_output_present") == true, "external output is not confirmed");
		const auto& proof = j.at("external_output_proof");
		Require(proof.at("schema") == 1 && SameSha(proof.at("git_sha"), j.at("git_sha"))
			&& proof.at("method") == "exact-input ON/OFF mutation plus same-frame sentinel Present"
			&& proof.at("sentinel_marker_pixels") == 1024 && proof.at("same_frame_present_completed") == true
			&& proof.at("on_returned_fnv64") == j.at("contract_hashes").at("returned_fnv64")
			&& IsHash(proof.at("off_returned_fnv64"))
			&& proof.at("on_returned_fnv64") != proof.at("off_returned_fnv64"),
			"external proof does not agree with the captured output");
	}
	else
		Require(j.at("external_contract_evaluated") == false && j.at("external_output_confirmed") == false
			&& j.at("neural_rendering_output_present") == false,
			"public reference cannot be an intercepted or confirmed external candidate");
}

struct Frame {
	std::filesystem::path manifest;
	Json metadata;
	std::string key;
	std::uint64_t id = 0;
};

Frame Describe(const Json& metadata, const std::string& output)
{
	ValidateManifest(metadata, output);
	Frame f;
	f.metadata = metadata;
	f.id = metadata.at("frame_id").get<std::uint64_t>();
	for (const auto name : InputNames) f.key += metadata.at("contract_hashes").at(name).get<std::string>();
	return f;
}

std::vector<Frame> Scan(const std::filesystem::path& root, const std::string& output)
{
	Require(std::filesystem::is_directory(root), "capture root is unavailable");
	std::vector<Frame> frames;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
	{
		Require(!entry.is_symlink(), "capture scan does not follow symbolic links");
		if (!entry.is_regular_file() || entry.path().filename() != "manifest.json") continue;
		Require(entry.file_size() <= 1024 * 1024, "manifest exceeds one MiB");
		std::ifstream stream(entry.path());
		Require(stream.is_open(), "cannot open manifest");
		const std::string text((std::istreambuf_iterator<char>(stream)), {});
		auto frame = Describe(ParseManifest(text), output);
		frame.manifest = entry.path();
		frames.push_back(std::move(frame));
		Require(frames.size() <= 240, "comparison exceeds 240 frames per input");
	}
	std::sort(frames.begin(), frames.end(), [](const Frame& a, const Frame& b) { return a.id < b.id; });
	return frames;
}

std::vector<std::size_t> Pair(const std::vector<Frame>& a, const std::vector<Frame>& b)
{
	Require(a.size() >= 2 && a.size() == b.size(), "comparison needs equal sequences of 2..240 frames");
	std::map<std::string, std::size_t> lookup;
	std::set<std::string> aKeys;
	for (std::size_t i = 0; i < b.size(); ++i)
		Require(lookup.emplace(b[i].key, i).second, "ambiguous repeated input contract in B");
	std::vector<std::size_t> result;
	for (std::size_t i = 0; i < a.size(); ++i)
	{
		Require(aKeys.insert(a[i].key).second, "ambiguous repeated input contract in A");
		const auto found = lookup.find(a[i].key);
		Require(found != lookup.end(), "no exact four-input match; nominal frame IDs are not correspondence");
		const auto j = found->second;
		for (const auto field : {"git_sha", "game_id", "api", "renderer", "render_size", "output_size", "content_rect"})
			Require(a[i].metadata.at(field) == b[j].metadata.at(field)
				&& a[i].metadata.at(field) == a.front().metadata.at(field), "sequence identity/geometry mismatch");
		for (const auto field : {"profile", "public_dlss_preset", "overlay_policy", "neural_mode", "external_settings"})
			Require(a[i].metadata.at(field) == a.front().metadata.at(field)
				&& b[j].metadata.at(field) == b.front().metadata.at(field), "quality lane changes within a sequence");
		if (i)
			Require(a[i].id == a[i - 1].id + 1 && b[j].id == b[result.back()].id + 1,
				"matched sequence has a frame gap or reordered chronology");
		result.push_back(j);
	}
	return result;
}

std::string RawHash(const std::vector<std::uint8_t>& bytes)
{
	std::uint64_t hash = 14695981039346656037ull;
	for (auto byte : bytes) { hash ^= byte; hash *= 1099511628211ull; }
	std::ostringstream text;
	text.imbue(std::locale::classic());
	text << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << hash;
	return text.str();
}

std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path, std::size_t size)
{
	Require(std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) == size,
		"raw capture size mismatch: " + path.filename().string());
	std::vector<std::uint8_t> bytes(size);
	std::ifstream stream(path, std::ios::binary);
	stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	Require(static_cast<std::size_t>(stream.gcount()) == size, "cannot read complete raw capture");
	return bytes;
}

Image LoadImage(const Frame& f, const char *name)
{
	Image image;
	std::string error;
	const auto path = f.manifest.parent_path() / name;
	int width = 0, height = 0, components = 0;
	Require(stbi_info(path.string().c_str(), &width, &height, &components) != 0
		&& width == f.metadata.at("render_size")[0] && height == f.metadata.at("render_size")[1],
		"PNG header dimensions disagree with bounded manifest");
	Require(ReadPng(path, image, error), error);
	Require(image.width == f.metadata.at("render_size")[0]
		&& image.height == f.metadata.at("render_size")[1], "PNG dimensions disagree with manifest");
	return image;
}

struct Pixels {
	Image source, output;
	std::array<std::vector<std::uint8_t>, 4> inputs;
};

void VerifyHash(const std::vector<std::uint8_t>& bytes, const Json& expected)
{
	Require(RawHash(bytes) == expected, "saved pixel/raw bytes do not match manifest hash");
}

Pixels LoadPixels(const Frame& f, const std::string& output)
{
	Pixels pixels;
	pixels.source = LoadImage(f, "source-color.png");
	pixels.output = LoadImage(f, output == "external" ? "neural-rendering-output.png" : "public-dlaa-output.png");
	pixels.inputs[0] = pixels.source.rgba;
	const auto size = pixels.source.rgba.size();
	pixels.inputs[1] = ReadBytes(f.manifest.parent_path() / "depth.f32", size);
	pixels.inputs[2] = ReadBytes(f.manifest.parent_path() / "motion.rg16f", size);
	const auto mask = LoadImage(f, "bias-mask.png");
	for (std::size_t i = 0; i < size; i += 4)
	{
		Require(mask.rgba[i] == mask.rgba[i + 1] && mask.rgba[i] == mask.rgba[i + 2]
			&& mask.rgba[i + 3] == 255, "bias mask is not an exact opaque R8 visualization");
		pixels.inputs[3].push_back(mask.rgba[i]);
	}
	for (std::size_t i = 0; i < 4; ++i)
		VerifyHash(pixels.inputs[i], f.metadata.at("contract_hashes").at(InputNames[i]));
	VerifyHash(pixels.output.rgba, f.metadata.at("contract_hashes").at("returned_fnv64"));
	return pixels;
}

Json Metrics(const Image& reference, const Image& output)
{
	Require(reference.width == output.width && reference.height == output.height
		&& reference.rgba.size() == output.rgba.size() && !reference.rgba.empty(), "metric image dimensions disagree");
	double absolute = 0, squared = 0;
	std::uint64_t changed = 0, alphaChanged = 0;
	int maxDelta = 0;
	for (std::size_t i = 0; i < reference.rgba.size(); i += 4)
	{
		bool different = false;
		for (std::size_t c = 0; c < 3; ++c)
		{
			const int delta = std::abs(int(reference.rgba[i + c]) - int(output.rgba[i + c]));
			absolute += delta;
			squared += delta * delta;
			maxDelta = std::max(maxDelta, delta);
			different |= delta != 0;
		}
		changed += different;
		alphaChanged += reference.rgba[i + 3] != output.rgba[i + 3];
	}
	const double samples = reference.rgba.size() / 4 * 3;
	const double mse = squared / samples;
	return {{"rgb_mae", absolute / samples}, {"rgb_mse", mse},
		{"rgb_psnr_db", mse == 0 ? Json(nullptr) : Json(10. * std::log10(255. * 255. / mse))},
		{"rgb_exact", changed == 0}, {"rgb_changed_pixels", changed},
		{"rgb_max_abs", maxDelta}, {"alpha_mismatch_pixels", alphaChanged}};
}

bool Fails(const std::function<void()>& operation)
{
	try { operation(); return false; } catch (const std::exception&) { return true; }
}
} // namespace

int CompareCaptureSequences(const std::filesystem::path& aPath, const std::filesystem::path& bPath,
	const std::filesystem::path& outputPath, const std::string& aOutput, const std::string& bOutput)
{
	if (aPath.empty() || bPath.empty() || outputPath.empty()
		|| (aOutput != "external" && aOutput != "public") || (bOutput != "external" && bOutput != "public"))
	{
		std::cerr << "compare-captures requires --a DIR --b DIR --out JSON and external/public output selectors\n";
		return 2;
	}
	try
	{
		const auto output = std::filesystem::absolute(outputPath);
		Require(!std::filesystem::exists(output) && !std::filesystem::is_symlink(output),
			"comparison output already exists; refusing overwrite");
		Require(std::filesystem::is_directory(output.parent_path()), "comparison output parent must already exist");
		const auto a = Scan(std::filesystem::absolute(aPath), aOutput);
		const auto b = Scan(std::filesystem::absolute(bPath), bOutput);
		const auto paired = Pair(a, b);
		Json frames = Json::array();
		Image previousA, previousB;
		double temporalA = 0, temporalB = 0;
		for (std::size_t i = 0; i < a.size(); ++i)
		{
			const auto& left = a[i];
			const auto& right = b[paired[i]];
			auto x = LoadPixels(left, aOutput);
			auto y = LoadPixels(right, bOutput);
			Require(x.inputs == y.inputs, "hash-matched pair differs in actual input bytes");
			if (aOutput == "external" && bOutput == "public")
				Require(left.metadata.at("external_output_proof").at("off_returned_fnv64")
					== right.metadata.at("contract_hashes").at("returned_fnv64"), "public reference differs from external OFF proof");
			if (bOutput == "external" && aOutput == "public")
				Require(right.metadata.at("external_output_proof").at("off_returned_fnv64")
					== left.metadata.at("contract_hashes").at("returned_fnv64"), "public reference differs from external OFF proof");
			Json frame = {{"a_frame_id", left.id}, {"b_frame_id", right.id},
				{"a_manifest", left.manifest.lexically_relative(output.parent_path()).generic_string()},
				{"b_manifest", right.manifest.lexically_relative(output.parent_path()).generic_string()},
				{"pair", Metrics(x.output, y.output)}, {"a_vs_source", Metrics(x.source, x.output)},
				{"b_vs_source", Metrics(y.source, y.output)},
				{"a_scene_cut", left.metadata.at("scene_cut")}, {"b_scene_cut", right.metadata.at("scene_cut")},
				{"a_raw_temporal_rgb_mae", nullptr}, {"b_raw_temporal_rgb_mae", nullptr}};
			if (i)
			{
				const double da = Metrics(previousA, x.output).at("rgb_mae").get<double>();
				const double db = Metrics(previousB, y.output).at("rgb_mae").get<double>();
				frame["a_raw_temporal_rgb_mae"] = da;
				frame["b_raw_temporal_rgb_mae"] = db;
				temporalA += da;
				temporalB += db;
			}
			previousA = std::move(x.output);
			previousB = std::move(y.output);
			frames.push_back(std::move(frame));
		}
		auto lane = [](const Frame& f) {
			return Json{{"profile", f.metadata.at("profile")}, {"public_dlss_preset", f.metadata.at("public_dlss_preset")},
				{"overlay_policy", f.metadata.at("overlay_policy")}, {"neural_mode", f.metadata.at("neural_mode")},
				{"external_settings_recommendation", f.metadata.at("external_settings")},
				{"actual_external_settings_verified", false}};
		};
		const Json report = {{"schema", 1}, {"tool_git_sha", GIT_HASH},
			{"capture_git_sha", a.front().metadata.at("git_sha")}, {"game_id", a.front().metadata.at("game_id")},
			{"status", "exact-input-paired"}, {"pair_count", frames.size()},
			{"a_output", aOutput}, {"b_output", bOutput}, {"all_saved_input_and_output_hashes_verified", true},
			{"a_lane", lane(a.front())}, {"b_lane", lane(b.front())},
			{"all_four_input_buffers_byte_identical", true}, {"winner_declared", false},
			{"eligible_for_performance_metrics", false},
			{"metric_definition", "RGB byte-domain MAE/MSE/PSNR; alpha counted separately; null PSNR with rgb_exact means infinity; raw temporal MAE includes motion and cuts and is not a flicker or stability score"},
			{"a_mean_raw_temporal_rgb_mae", temporalA / (a.size() - 1)},
			{"b_mean_raw_temporal_rgb_mae", temporalB / (a.size() - 1)}, {"frames", frames}};
		// All packages, correspondence, hashes, and pixels are checked before any write.
		std::ofstream stream(output);
		stream.imbue(std::locale::classic());
		stream << report.dump(2) << '\n';
		stream.close();
		Require(!stream.fail(), "failed writing comparison report");
		std::cout << "capture comparison exact_pairs=" << a.size() << " winner_declared=false\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "capture comparison rejected: " << error.what() << '\n';
		return 1;
	}
}

std::vector<std::pair<std::string, bool>> CaptureComparisonSelfTests()
{
	std::vector<std::pair<std::string, bool>> tests;
	auto expect = [&](const std::string& name, bool value) { tests.emplace_back(name, value); };
	const std::string text = R"({"schema":3,"git_sha":"123456789","game_id":"fixture","frame_id":10,
		"evaluation_accepted":true,"public_output_present":true,"capture_stalls_gpu":true,
		"eligible_for_performance_metrics":false,"render_size":[2,2],"output_size":[2,2],
		"content_rect":[0,0,2,2],"api":"d3d11on12","renderer":"dx11",
		"profile":"fixture","public_dlss_preset":0,"overlay_policy":0,"neural_mode":2,"external_settings":"user controlled",
		"external_contract_evaluated":false,"external_output_confirmed":false,"neural_rendering_output_present":false,
		"contract_hashes":{"color_fnv64":"0000000000000001","depth_fnv64":"0000000000000002",
		"motion_fnv64":"0000000000000003","mask_fnv64":"0000000000000004","returned_fnv64":"0000000000000005"}})";
	const auto j = ParseManifest(text);
	auto f = Describe(j, "public");
	auto second = j;
	second["frame_id"] = std::uint64_t(11);
	second["contract_hashes"]["color_fnv64"] = "0000000000000006";
	std::vector<Frame> a{f, Describe(second, "public")}, b = a;
	b[0].id += 30; b[1].id += 30;
	expect("exact-input comparison pairs offset frame IDs by all four input hashes", Pair(a, b) == std::vector<std::size_t>{0, 1});
	auto wrong = b; wrong[0].key[0] = 'F'; wrong[0].id = a[0].id;
	expect("nominal frame equality cannot rescue a wrong input contract", Fails([&] { Pair(a, wrong); }));
	wrong = b; wrong[1].key = wrong[0].key;
	expect("duplicate input contracts reject ambiguous sequence pairing", Fails([&] { Pair(a, wrong); }));
	wrong = b; wrong[1].id += 1;
	expect("exact-input temporal comparison rejects gaps", Fails([&] { Pair(a, wrong); }));
	wrong = b; std::swap(wrong[0].key, wrong[1].key);
	expect("exact-input temporal comparison rejects reordered chronology", Fails([&] { Pair(a, wrong); }));
	wrong = b; wrong[0].metadata["git_sha"] = "abcdef012";
	expect("exact-input comparison rejects mixed build identity", Fails([&] { Pair(a, wrong); }));
	wrong = b; wrong[1].metadata["public_dlss_preset"] = 11;
	expect("exact-input comparison rejects a preset change within one lane", Fails([&] { Pair(a, wrong); }));
	expect("strict capture JSON rejects duplicate keys and malformed input", Fails([&] { ParseManifest("{\"schema\":3,\"schema\":3}"); })
		&& Fails([&] { ParseManifest("{\"schema\":"); }));
	auto bad = j; bad["render_size"] = Json::array({5u, 120u, 2u});
	expect("comparison rejects locale-split dimension arrays", Fails([&] { Describe(bad, "public"); }));
	expect("unconfirmed external image cannot enter comparison", Fails([&] { Describe(j, "external"); }));
	bad = j; bad["external_contract_evaluated"] = true;
	expect("intercepted candidate cannot be relabeled public DLAA", Fails([&] { Describe(bad, "public"); }));
	bad = j;
	bad["external_contract_evaluated"] = true;
	bad["external_output_confirmed"] = true;
	bad["neural_rendering_output_present"] = true;
	bad["external_output_proof"] = {{"schema", 1}, {"git_sha", "123456789"},
		{"method", "exact-input ON/OFF mutation plus same-frame sentinel Present"},
		{"sentinel_marker_pixels", 1024}, {"same_frame_present_completed", true},
		{"on_returned_fnv64", "0000000000000005"}, {"off_returned_fnv64", "0000000000000007"}};
	expect("comparison accepts structurally complete confirmed external proof", !Fails([&] { Describe(bad, "external"); }));
	bad["external_output_proof"]["same_frame_present_completed"] = false;
	expect("external proof without completed Present is rejected", Fails([&] { Describe(bad, "external"); }));
	bad["external_output_proof"]["same_frame_present_completed"] = true;
	bad["external_output_proof"]["on_returned_fnv64"] = "0000000000000008";
	expect("external proof must identify the same returned output", Fails([&] { Describe(bad, "external"); }));
	const std::vector<std::uint8_t> hello{'h', 'e', 'l', 'l', 'o'};
	expect("comparison hashes use standard raw-contract FNV64", RawHash(hello) == "A430D84680AABD0B");
	expect("mutated artifact bytes fail the manifest hash", Fails([&] { VerifyHash(hello, "0000000000000000"); }));
	Image zero{1, 1, {0, 0, 0, 255}}, one{1, 1, {1, 1, 1, 255}};
	const auto exact = Metrics(zero, zero), changed = Metrics(zero, one);
	expect("RGB comparison metrics have analytic MAE/MSE and JSON-safe exact PSNR", exact.at("rgb_exact") == true
		&& exact.at("rgb_psnr_db").is_null() && changed.at("rgb_mae") == 1.0 && changed.at("rgb_mse") == 1.0
		&& std::abs(changed.at("rgb_psnr_db").get<double>() - 48.1308036087) < 1e-8);
	one = zero; one.rgba[3] = 0;
	const auto alpha = Metrics(zero, one);
	expect("comparison counts alpha changes without hiding RGB error in alpha", alpha.at("rgb_exact") == true
		&& alpha.at("alpha_mismatch_pixels") == 1 && alpha.at("rgb_mae") == 0.0);
	// Exercise the real scanner/PNG/raw loaders and fail-closed report boundary.
	const auto tempBase = std::filesystem::canonical(std::filesystem::temp_directory_path());
	const auto scratch = tempBase / ("flycast-capture-compare-"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	bool created = false;
	try
	{
		created = std::filesystem::create_directory(scratch);
		Require(created, "cannot create unique comparison-test directory");
		auto writeBytes = [&](const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
			std::ofstream stream(path, std::ios::binary);
			stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			Require(bool(stream), "cannot write synthetic raw fixture");
		};
		for (const auto lane : {"a", "b"})
			for (std::uint64_t index = 0; index < 2; ++index)
			{
				const auto root = scratch / lane / ("frame-" + std::to_string(index));
				std::filesystem::create_directories(root);
				Image color{2, 2, std::vector<std::uint8_t>(16, static_cast<std::uint8_t>(40 + index))};
				Image mask{2, 2, std::vector<std::uint8_t>(16, 0)};
				for (std::size_t i = 3; i < 16; i += 4) color.rgba[i] = mask.rgba[i] = 255;
				std::string error;
				Require(WritePng(root / "source-color.png", color, error)
					&& WritePng(root / "public-dlaa-output.png", color, error)
					&& WritePng(root / "bias-mask.png", mask, error), error);
				const std::vector<std::uint8_t> raw(16, 0), maskRaw(4, 0);
				writeBytes(root / "depth.f32", raw);
				writeBytes(root / "motion.rg16f", raw);
				auto manifest = j;
				manifest["scene_cut"] = false;
				manifest["frame_id"] = index + (std::string(lane) == "a" ? 10u : 40u);
				manifest["contract_hashes"] = {{"color_fnv64", RawHash(color.rgba)}, {"depth_fnv64", RawHash(raw)},
					{"motion_fnv64", RawHash(raw)}, {"mask_fnv64", RawHash(maskRaw)}, {"returned_fnv64", RawHash(color.rgba)}};
				std::ofstream stream(root / "manifest.json");
				stream << manifest.dump();
				Require(bool(stream), "cannot write synthetic manifest");
			}
		auto compare = [&](const char *name) {
			return CompareCaptureSequences(scratch / "a", scratch / "b", scratch / name, "public", "public");
		};
		expect("file-level comparison accepts two exact offset public frames", compare("accepted.json") == 0);
		const auto accepted = ReadBytes(scratch / "accepted.json", std::filesystem::file_size(scratch / "accepted.json"));
		expect("comparison refuses to overwrite an existing report", compare("accepted.json") == 1
			&& ReadBytes(scratch / "accepted.json", accepted.size()) == accepted);
		std::vector<std::uint8_t> changedRaw(16, 0); changedRaw[5] = 1;
		writeBytes(scratch / "b/frame-0/depth.f32", changedRaw);
		expect("actual depth-file mutation rejects without writing a report", compare("bad-depth.json") == 1
			&& !std::filesystem::exists(scratch / "bad-depth.json"));
		writeBytes(scratch / "b/frame-0/depth.f32", std::vector<std::uint8_t>(16, 0));
		Image changedOutput{2, 2, std::vector<std::uint8_t>(16, 255)};
		std::string error;
		Require(WritePng(scratch / "b/frame-0/public-dlaa-output.png", changedOutput, error), error);
		expect("actual output-PNG mutation rejects without writing a report", compare("bad-output.json") == 1
			&& !std::filesystem::exists(scratch / "bad-output.json"));
	}
	catch (const std::exception& error)
	{
		std::cerr << "comparison filesystem fixture: " << error.what() << '\n';
		expect("comparison filesystem fixture completed", false);
	}
	// Remove only this uniquely created, resolved direct child of the temp directory.
	if (created && !std::filesystem::is_symlink(scratch)
		&& std::filesystem::canonical(scratch).parent_path() == tempBase)
	{
		std::error_code error;
		std::filesystem::remove_all(scratch, error);
		expect("comparison fixture cleans up its own temporary artifacts", !error);
	}
	return tests;
}
} // namespace neuraltest
