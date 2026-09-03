// SPDX-License-Identifier: GPL-2.0-or-later
#include "neural_backend.h"

namespace flycast::rend::neural {
namespace {

class UnsupportedBackend final : public INeuralBackend
{
public:
	explicit UnsupportedBackend(const char *reason) : reason_(reason) {}
	BackendEvalStatus Initialize(const StageConfig&, void*, void*) noexcept override
	{
		return BackendEvalStatus::Unsupported;
	}
	BackendEvalStatus Evaluate(const NeuralFrame&) noexcept override
	{
		return BackendEvalStatus::Unsupported;
	}
	void ResetHistory() noexcept override {}
	BackendStats GetStats() const noexcept override { return {}; }
	TextureRef GetOutput() const noexcept override { return {}; }
	const char *GetStatusReason() const noexcept override { return reason_; }
	void Shutdown() noexcept override {}

private:
	const char *reason_;
};

} // namespace

#ifdef FLYCAST_ENABLE_NGX
std::unique_ptr<INeuralBackend> CreateNgxD3D11Backend();
#endif

std::unique_ptr<INeuralBackend> CreateNeuralBackend(NeuralMode mode, Api api)
{
	if (mode == NeuralMode::Dlss5Experimental)
		return std::make_unique<UnsupportedBackend>(
			"DLSS 5 experimental backend awaits a public NVIDIA developer contract");
	if (api == Api::D3D12)
		return std::make_unique<UnsupportedBackend>(
			"D3D11On12 neural surface is not initialized");
#ifdef FLYCAST_ENABLE_NGX
	return CreateNgxD3D11Backend();
#else
	return std::make_unique<UnsupportedBackend>(
		"Flycast was built without FLYCAST_NEURAL_NGX");
#endif
}

} // namespace flycast::rend::neural
