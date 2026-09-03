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

bool Compile(const std::string& source, const char *neuralExport,
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

} // namespace

bool ValidateProductionExportShader(std::string& error)
{
	std::ifstream input(std::string(NEURAL_SOURCE_DIR) + "/core/rend/dx11/dx11_shaders.cpp",
		std::ios::binary);
	if (!input)
	{
		error = "cannot open production dx11_shaders.cpp";
		return false;
	}
	std::ostringstream stream;
	stream << input.rdbuf();
	std::string common;
	std::string pixel;
	const auto source = stream.str();
	if (!ExtractRawString(source, "PixelShaderCommon", common)
		|| !ExtractRawString(source, "PixelShader", pixel))
	{
		error = "cannot extract production pixel shader raw strings";
		return false;
	}
	PixelInclude includes(common);
	return Compile(pixel, "0", includes, error) && Compile(pixel, "1", includes, error);
}

} // namespace neuraltest
