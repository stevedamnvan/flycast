// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using Microsoft::WRL::ComPtr;

namespace neuraltest {
namespace {

bool ExtractRawString(const std::string& source, const char *symbol, std::string& value)
{
	const std::string marker = std::string(symbol) + " = R\"(";
	const auto begin = source.find(marker);
	if (begin == std::string::npos)
		return false;
	const auto contentBegin = begin + marker.size();
	const auto end = source.find(")\";", contentBegin);
	if (end == std::string::npos)
		return false;
	value = source.substr(contentBegin, end - contentBegin);
	return true;
}

class PixelInclude final : public ID3DInclude
{
public:
	explicit PixelInclude(const std::string& common) : common_(common) {}

	HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID,
		LPCVOID *data, UINT *bytes) override
	{
		if (std::strcmp(fileName, "pixel_common.hlsl") != 0)
			return E_FAIL;
		*data = common_.data();
		*bytes = static_cast<UINT>(common_.size());
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Close(LPCVOID) override { return S_OK; }

private:
	const std::string& common_;
};

class OitInclude final : public ID3DInclude
{
public:
	explicit OitInclude(const std::string& header) : header_(header) {}

	HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID,
		LPCVOID *data, UINT *bytes) override
	{
		if (std::strcmp(fileName, "oit_header.hlsl") != 0)
			return E_FAIL;
		*data = header_.data();
		*bytes = static_cast<UINT>(header_.size());
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Close(LPCVOID) override { return S_OK; }

private:
	const std::string& header_;
};

bool CompileStandalonePixel(const std::string& source, const char *name,
	const D3D_SHADER_MACRO *macros, ID3DInclude *includes, std::string& error)
{
	ComPtr<ID3DBlob> code;
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT result = D3DCompile(source.data(), source.size(), name, macros,
		includes, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(result))
		return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : "D3DCompile failed without diagnostics";
	return false;
}

bool CompilePixel(const std::string& source, const char *neuralExport,
	PixelInclude& includes, std::string& error)
{
	D3D_SHADER_MACRO macros[] = {
		{"pp_Gouraud", "1"}, {"DIV_POS_Z", "0"}, {"pp_Texture", "1"},
		{"pp_UseAlpha", "1"}, {"pp_IgnoreTexA", "0"}, {"pp_ShadInstr", "3"},
		{"pp_Offset", "1"}, {"pp_FogCtrl", "1"}, {"pp_BumpMap", "0"},
		{"FogClamping", "1"}, {"pp_TriLinear", "1"}, {"pp_Palette", "0"},
		{"cp_AlphaTest", "1"}, {"pp_ClipInside", "1"}, {"DITHERING", "1"},
		{"NEURAL_EXPORT", neuralExport}, {nullptr, nullptr}
	};
	ComPtr<ID3DBlob> code;
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT result = D3DCompile(source.data(), source.size(), "dx11_shaders.cpp",
		macros, &includes, "main", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(result))
		return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : "D3DCompile failed without diagnostics";
	return false;
}

bool CompileVertex(const std::string& source, bool divPosZ, bool neuralExport,
	bool naomi2, std::string& error)
{
	D3D_SHADER_MACRO macros[] = {
		{"pp_Gouraud", "1"}, {"DIV_POS_Z", divPosZ ? "1" : "0"},
		{"POSITION_ONLY", "0"}, {"pp_TwoVolumes", "0"}, {"LIGHT_ON", "1"},
		{"MODIFIER_VOLUME", "0"}, {"NEURAL_EXPORT", neuralExport ? "1" : "0"},
		{nullptr, nullptr}
	};
	ComPtr<ID3DBlob> code;
	ComPtr<ID3DBlob> diagnostics;
	const HRESULT result = D3DCompile(source.data(), source.size(),
		naomi2 ? "dx11_naomi2.cpp" : "dx11_shaders.cpp", macros, nullptr,
		"main", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		code.GetAddressOf(), diagnostics.GetAddressOf());
	if (SUCCEEDED(result)) return true;
	error = diagnostics ? std::string(static_cast<const char *>(diagnostics->GetBufferPointer()),
		diagnostics->GetBufferSize()) : "D3DCompile failed without diagnostics";
	return false;
}

bool ReadSource(const char *relativePath, std::string& source)
{
	std::ifstream input(std::string(NEURAL_SOURCE_DIR) + '/' + relativePath, std::ios::binary);
	if (!input) return false;
	std::ostringstream stream;
	stream << input.rdbuf();
	source = stream.str();
	return true;
}

} // namespace

bool ValidateProductionExportShader(std::string& error)
{
	std::string source;
	std::string naomiSource;
	std::string oitSource;
	if (!ReadSource("core/rend/dx11/dx11_shaders.cpp", source)
		|| !ReadSource("core/rend/dx11/dx11_naomi2.cpp", naomiSource)
		|| !ReadSource("core/rend/dx11/oit/dx11_oitshaders.cpp", oitSource))
	{
		error = "cannot open production DX11 shader sources";
		return false;
	}
	std::string common;
	std::string pixel;
	std::string vertex;
	std::string naomiVertex;
	std::string naomiColor;
	std::string reactiveCoverage;
	std::string oitHeader;
	std::string oitFinal;
	if (!ExtractRawString(source, "PixelShaderCommon", common)
		|| !ExtractRawString(source, "PixelShader", pixel)
		|| !ExtractRawString(source, "VertexShader", vertex)
		|| !ExtractRawString(source, "NeuralReactiveCoveragePixelShader", reactiveCoverage)
		|| !ExtractRawString(naomiSource, "DX11N2VertexShader", naomiVertex)
		|| !ExtractRawString(naomiSource, "DX11N2ColorShader", naomiColor)
		|| !ExtractRawString(oitSource, "static const char OITShaderHeader[]", oitHeader)
		|| !ExtractRawString(oitSource, "static const char OITFinalShaderSource[]", oitFinal))
	{
		error = "cannot extract production DX11 shader raw strings";
		return false;
	}
	PixelInclude includes(common);
	OitInclude oitIncludes(oitHeader);
	D3D_SHADER_MACRO oitMacros[] = {
		{"MAX_PIXELS_PER_FRAGMENT", "32"}, {"DITHERING", "1"}, {nullptr, nullptr}
	};
	const std::string naomi = naomiVertex + '\n' + naomiColor;
	return CompilePixel(pixel, "0", includes, error)
		&& CompilePixel(pixel, "1", includes, error)
		&& CompileVertex(vertex, false, false, false, error)
		&& CompileVertex(vertex, true, false, false, error)
		&& CompileVertex(vertex, false, true, false, error)
		&& CompileVertex(vertex, true, true, false, error)
		&& CompileVertex(naomi, false, false, true, error)
		&& CompileVertex(naomi, false, true, true, error)
		&& CompileStandalonePixel(reactiveCoverage, "neural-reactive-coverage",
			nullptr, nullptr, error)
		&& oitFinal.find("reactiveCoverage = num_frag > 0") != std::string::npos
		&& oitFinal.find("SV_Target1") != std::string::npos
		&& CompileStandalonePixel(oitFinal, "oit-final-reactive", oitMacros,
			&oitIncludes, error);
}

} // namespace neuraltest
