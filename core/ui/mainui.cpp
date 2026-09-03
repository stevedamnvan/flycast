/*
	Copyright 2020 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "mainui.h"
#include "hw/pvr/Renderer_if.h"
#include "gui.h"
#include "oslib/oslib.h"
#include "wsi/context.h"
#include "cfg/option.h"
#include "emulator.h"
#include "imgui_driver.h"
#include "profiler/fc_profiler.h"
#include "oslib/i18n.h"
#if defined(_WIN32) && defined(FLYCAST_ENABLE_NEURAL)
#include "rend/dx11/dx11context.h"
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <locale>
#include <thread>

static bool mainui_enabled;
u32 MainFrameCount;
static bool forceReinit;
#ifdef FLYCAST_ENABLE_NEURAL
static bool neuralDeveloperReinitTriggered;
static bool neuralDeveloperReinitPending;
static bool neuralDeveloperSwitchTriggered;
static bool neuralDeveloperSwitchPending;
static int neuralDeveloperSwitchFrom;
static int neuralDeveloperSwitchTo;
static bool neuralDeveloperSurfaceSwitchTriggered;
static bool neuralDeveloperSurfaceSwitchPending;
static int neuralDeveloperSurfaceSwitchFrom;
static int neuralDeveloperSurfaceSwitchTo;
static bool neuralDeveloperGameReloadTriggered;
static bool neuralDeveloperGameReloadCompleted;
static bool neuralDeveloperGameUnloadObserved;
static bool neuralDeveloperGameIdUnchanged;
static bool neuralDeveloperGamePathUnchanged;
static bool neuralDeveloperSaveStateTriggered;
static bool neuralDeveloperSaveStateSaved;
static bool neuralDeveloperSaveStateLoaded;
static u32 neuralDeveloperSaveStateFrame;
static u32 neuralDeveloperLoadStateFrame;
static std::vector<u8> neuralDeveloperSaveState;
static bool neuralDeveloperPauseTriggered;
static bool neuralDeveloperPauseObserved;
static bool neuralDeveloperResumeObserved;
static u32 neuralDeveloperPauseFrame;
static u32 neuralDeveloperResumeFrame;
static bool neuralDeveloperModeRoundtripTriggered;
static bool neuralDeveloperModeRoundtripCompleted;
static int neuralDeveloperModeRoundtripOriginal;
static u32 neuralDeveloperModeOffFrame;
static u32 neuralDeveloperModeOnFrame;
static bool neuralDeveloperDeviceRemovalTriggered;
static bool neuralDeveloperDeviceRemovalPending;
static bool neuralDeveloperDeviceRemovalRequested;
static bool neuralDeveloperDeviceRemovalObserved;
static bool neuralDeveloperDeviceRemovalRecovered;
static u32 neuralDeveloperDeviceRemovalFrame;
static std::uint32_t neuralDeveloperDeviceRemovalReason;

static void writeNeuralDeveloperReinitMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "renderer-reinit-complete.json");
	marker << "{\n  \"schema\": 1,\n  \"completed\": true,\n  \"main_frame\": "
		<< MainFrameCount << ",\n  \"renderer\": "
		<< static_cast<int>(config::RendererType.get())
		<< ",\n  \"performance_sampling_restarted\": true\n}\n";
}

static void writeNeuralDeveloperSwitchMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "renderer-switch-complete.json");
	marker << "{\n  \"schema\": 1,\n  \"completed\": true,\n  \"main_frame\": "
		<< MainFrameCount << ",\n  \"renderer_from\": " << neuralDeveloperSwitchFrom
		<< ",\n  \"renderer_to\": " << neuralDeveloperSwitchTo
		<< ",\n  \"performance_sampling_restarted\": true\n}\n";
}

static void writeNeuralDeveloperSurfaceSwitchMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "surface-switch-complete.json");
	marker << "{\n  \"schema\": 1,\n  \"completed\": true,\n  \"main_frame\": "
		<< MainFrameCount << ",\n  \"surface_from\": " << neuralDeveloperSurfaceSwitchFrom
		<< ",\n  \"surface_to\": " << neuralDeveloperSurfaceSwitchTo
		<< ",\n  \"performance_sampling_restarted\": true\n}\n";
}

static void writeNeuralDeveloperGameReloadMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "game-reload-complete.json");
	marker << "{\n  \"schema\": 1,\n  \"completed\": "
		<< (neuralDeveloperGameReloadCompleted ? "true" : "false")
		<< ",\n  \"main_frame\": " << MainFrameCount
		<< ",\n  \"unload_observed\": "
		<< (neuralDeveloperGameUnloadObserved ? "true" : "false")
		<< ",\n  \"same_game_id\": "
		<< (neuralDeveloperGameIdUnchanged ? "true" : "false")
		<< ",\n  \"same_media_path\": "
		<< (neuralDeveloperGamePathUnchanged ? "true" : "false") << "\n}\n";
}

static void writeNeuralDeveloperSaveStateMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "savestate-roundtrip-complete.json");
	marker << "{\n  \"schema\": 1,\n  \"completed\": "
		<< (neuralDeveloperSaveStateSaved && neuralDeveloperSaveStateLoaded
			? "true" : "false")
		<< ",\n  \"in_memory\": true"
		<< ",\n  \"save_allowed\": "
		<< (dc_savestateAllowed() ? "true" : "false")
		<< ",\n  \"saved\": " << (neuralDeveloperSaveStateSaved ? "true" : "false")
		<< ",\n  \"loaded\": " << (neuralDeveloperSaveStateLoaded ? "true" : "false")
		<< ",\n  \"save_main_frame\": " << neuralDeveloperSaveStateFrame
		<< ",\n  \"load_main_frame\": " << neuralDeveloperLoadStateFrame
		<< ",\n  \"state_bytes\": "
		<< std::to_string(neuralDeveloperSaveState.size()) << "\n}\n";
}

static void writeNeuralDeveloperPauseMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "pause-roundtrip-complete.json");
	marker << "{\n  \"schema\": 1,\n  \"completed\": "
		<< (neuralDeveloperPauseObserved && neuralDeveloperResumeObserved
			? "true" : "false")
		<< ",\n  \"pause_observed\": "
		<< (neuralDeveloperPauseObserved ? "true" : "false")
		<< ",\n  \"resume_observed\": "
		<< (neuralDeveloperResumeObserved ? "true" : "false")
		<< ",\n  \"pause_main_frame\": " << neuralDeveloperPauseFrame
		<< ",\n  \"resume_main_frame\": " << neuralDeveloperResumeFrame << "\n}\n";
}

static void writeNeuralDeveloperModeRoundtripMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "neural-mode-roundtrip-complete.json");
	marker << "{\n  \"schema\": 1,\n  \"completed\": "
		<< (neuralDeveloperModeRoundtripCompleted ? "true" : "false")
		<< ",\n  \"original_mode\": " << neuralDeveloperModeRoundtripOriginal
		<< ",\n  \"off_mode\": 0"
		<< ",\n  \"restored_mode\": " << config::NeuralMode.get()
		<< ",\n  \"off_main_frame\": " << neuralDeveloperModeOffFrame
		<< ",\n  \"on_main_frame\": " << neuralDeveloperModeOnFrame
		<< ",\n  \"renderer_restarted\": false"
		<< ",\n  \"performance_sampling_restarted\": false\n}\n";
}

static void writeNeuralDeveloperDeviceRemovalMarker()
{
	const auto root = std::filesystem::path(config::NeuralPerformanceDirectory.get());
	if (root.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec) return;
	std::ofstream marker(root / "actual-device-removal-complete.json");
	marker.imbue(std::locale::classic());
	marker << "{\n  \"schema\": 1,\n  \"completed\": "
		<< (neuralDeveloperDeviceRemovalRequested
			&& neuralDeveloperDeviceRemovalObserved
			&& neuralDeveloperDeviceRemovalRecovered ? "true" : "false")
		<< ",\n  \"main_frame\": " << neuralDeveloperDeviceRemovalFrame
		<< ",\n  \"method\": \"ID3D12Device5::RemoveDevice\""
		<< ",\n  \"removal_requested\": "
		<< (neuralDeveloperDeviceRemovalRequested ? "true" : "false")
		<< ",\n  \"removal_observed\": "
		<< (neuralDeveloperDeviceRemovalObserved ? "true" : "false")
		<< ",\n  \"removed_reason\": \"0x" << std::hex << std::uppercase
		<< static_cast<unsigned long>(neuralDeveloperDeviceRemovalReason) << std::dec
		<< "\",\n  \"recovery_initialized\": "
		<< (neuralDeveloperDeviceRemovalRecovered ? "true" : "false")
		<< ",\n  \"d3d11on12_restored\": "
		<< (neuralDeveloperDeviceRemovalRecovered ? "true" : "false")
		<< ",\n  \"performance_sampling_restarted\": true\n}\n";
}
#endif

bool mainui_rend_frame()
{
	FC_PROFILE_SCOPE;

	os_DoEvents();
	os_UpdateInputState();

	if (gui_is_open())
	{
		try {
			gui_display_ui();
		} catch (const FlycastException& e) {
			// Assume this is a graphics API issue
			forceReinit = true;
			return false;
		}
#ifndef TARGET_IPHONE
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
	}
	else
	{
		try {
			if (!emu.render())
				return false;
			if (config::ProfilerEnabled && config::ProfilerDrawToGUI)
				gui_display_profiler();
		} catch (const RendererException& e) {
			gui_error(i18n::Ts("Renderer error:") + "\n" + e.what() + "\n\n"
					+ i18n::Ts("The game has been paused but it is recommended to restart Flycast"));
			rend_term_renderer();
			if (!rend_init_renderer())
				ERROR_LOG(RENDERER, "Renderer re-initialization failed");
			gui_open_settings();
			return false;
		} catch (const FlycastException& e) {
			gui_stop_game(e.what());
			return false;
		}
	}
	MainFrameCount++;

	return true;
}

void mainui_init()
{
	if (!rend_init_renderer()) {
		ERROR_LOG(RENDERER, "Renderer initialization failed");
		gui_error(i18n::T("Renderer initialization failed.\nPlease select a different graphics API"));
	}
}

void mainui_term()
{
	rend_term_renderer();
}

void mainui_loop(bool forceStart)
{
	ThreadName _("Flycast-rend");
	if (forceStart)
		mainui_enabled = true;
	mainui_init();
	RenderType currentRenderer = config::RendererType;

	while (mainui_enabled)
	{
		fc_profiler::startThread("main");

		if (mainui_rend_frame() && imguiDriver != nullptr)
		{
			try {
				imguiDriver->present();
			} catch (const FlycastException& e) {
				forceReinit = true;
			}
		}
		if (imguiDriver == nullptr)
			forceReinit = true;

#ifdef FLYCAST_ENABLE_NEURAL
		const int neuralReinitAfter = std::clamp(config::NeuralRendererReinitAfter.get(), 0, 10000);
		const int neuralSwitchAfter = std::clamp(config::NeuralRendererSwitchAfter.get(), 0, 10000);
		const int neuralSurfaceSwitchAfter = std::clamp(config::NeuralSurfaceSwitchAfter.get(), 0, 10000);
		const int neuralGameReloadAfter = std::clamp(config::NeuralGameReloadAfter.get(), 0, 10000);
		const int neuralSaveStateAfter = std::clamp(config::NeuralSaveStateAfter.get(), 0, 10000);
		const int neuralSaveStateLoadDelay = std::clamp(config::NeuralSaveStateLoadDelay.get(), 1, 10000);
		const int neuralPauseAfter = std::clamp(config::NeuralPauseAfter.get(), 0, 10000);
		const int neuralPauseDuration = std::clamp(config::NeuralPauseDuration.get(), 1, 10000);
		const int neuralModeRoundtripAfter = std::clamp(
			config::NeuralModeRoundtripAfter.get(), 0, 10000);
		const int neuralModeOffDuration = std::clamp(
			config::NeuralModeOffDuration.get(), 1, 10000);
		const int neuralActualDeviceRemovalAfter = std::clamp(
			config::NeuralActualDeviceRemovalAfter.get(), 0, 10000);
		if (!neuralDeveloperReinitTriggered && neuralReinitAfter > 0
			&& MainFrameCount >= static_cast<u32>(neuralReinitAfter))
		{
			neuralDeveloperReinitTriggered = true;
			neuralDeveloperReinitPending = true;
			forceReinit = true;
			NOTICE_LOG(RENDERER,
				"Neural developer renderer reinit requested at main frame %u",
				MainFrameCount);
		}
		else if (!neuralDeveloperSwitchTriggered && neuralReinitAfter == 0
			&& neuralSwitchAfter > 0 && MainFrameCount >= static_cast<u32>(neuralSwitchAfter)
			&& (currentRenderer == RenderType::DirectX11
				|| currentRenderer == RenderType::DirectX11_OIT))
		{
			neuralDeveloperSwitchTriggered = true;
			neuralDeveloperSwitchPending = true;
			neuralDeveloperSwitchFrom = static_cast<int>(currentRenderer);
			const RenderType target = currentRenderer == RenderType::DirectX11
				? RenderType::DirectX11_OIT : RenderType::DirectX11;
			neuralDeveloperSwitchTo = static_cast<int>(target);
			config::RendererType = target;
			NOTICE_LOG(RENDERER,
				"Neural developer renderer switch requested at main frame %u: %d -> %d",
				MainFrameCount, neuralDeveloperSwitchFrom, neuralDeveloperSwitchTo);
		}
		else if (!neuralDeveloperSurfaceSwitchTriggered && neuralReinitAfter == 0
			&& neuralSwitchAfter == 0 && neuralSurfaceSwitchAfter > 0
			&& MainFrameCount >= static_cast<u32>(neuralSurfaceSwitchAfter)
			&& (currentRenderer == RenderType::DirectX11
				|| currentRenderer == RenderType::DirectX11_OIT))
		{
			neuralDeveloperSurfaceSwitchTriggered = true;
			neuralDeveloperSurfaceSwitchPending = true;
			neuralDeveloperSurfaceSwitchFrom = config::NeuralD3D12Surface.get() ? 1 : 0;
			neuralDeveloperSurfaceSwitchTo = neuralDeveloperSurfaceSwitchFrom == 0 ? 1 : 0;
			config::NeuralD3D12Surface = neuralDeveloperSurfaceSwitchTo != 0;
			forceReinit = true;
			NOTICE_LOG(RENDERER,
				"Neural developer surface switch requested at main frame %u: %d -> %d",
				MainFrameCount, neuralDeveloperSurfaceSwitchFrom,
				neuralDeveloperSurfaceSwitchTo);
		}
		else if (!neuralDeveloperDeviceRemovalTriggered && neuralReinitAfter == 0
			&& neuralSwitchAfter == 0 && neuralSurfaceSwitchAfter == 0
			&& neuralActualDeviceRemovalAfter > 0
			&& MainFrameCount >= static_cast<u32>(neuralActualDeviceRemovalAfter)
			&& (currentRenderer == RenderType::DirectX11
				|| currentRenderer == RenderType::DirectX11_OIT))
		{
			neuralDeveloperDeviceRemovalTriggered = true;
			neuralDeveloperDeviceRemovalFrame = MainFrameCount;
#if defined(_WIN32)
			DX11Context *context = DX11Context::Instance();
			neuralDeveloperDeviceRemovalRequested = context && context->isD3D11On12();
			if (neuralDeveloperDeviceRemovalRequested)
			{
				const HRESULT reason = context->removeD3D12DeviceForTesting();
				neuralDeveloperDeviceRemovalReason = static_cast<std::uint32_t>(reason);
				neuralDeveloperDeviceRemovalObserved = reason == DXGI_ERROR_DEVICE_REMOVED;
			}
#endif
			neuralDeveloperDeviceRemovalPending = true;
			forceReinit = true;
			NOTICE_LOG(RENDERER,
				"Neural developer actual D3D12 device removal requested at main frame %u: requested=%d observed=%d reason=%08x",
				MainFrameCount, neuralDeveloperDeviceRemovalRequested ? 1 : 0,
				neuralDeveloperDeviceRemovalObserved ? 1 : 0,
				neuralDeveloperDeviceRemovalReason);
		}
		else if (!neuralDeveloperGameReloadTriggered && neuralReinitAfter == 0
			&& neuralSwitchAfter == 0 && neuralSurfaceSwitchAfter == 0
			&& neuralSaveStateAfter == 0 && neuralPauseAfter == 0
			&& neuralGameReloadAfter > 0
			&& MainFrameCount >= static_cast<u32>(neuralGameReloadAfter)
			&& !settings.content.path.empty())
		{
			neuralDeveloperGameReloadTriggered = true;
			const std::string mediaPath = settings.content.path;
			const std::string gameId = settings.content.gameId;
			NOTICE_LOG(RENDERER,
				"Neural developer same-media reload requested at main frame %u",
				MainFrameCount);
			try
			{
				emu.unloadGame();
				neuralDeveloperGameUnloadObserved = settings.content.path.empty();
				emu.loadGame(mediaPath.c_str());
				emu.start();
				neuralDeveloperGameIdUnchanged = !gameId.empty()
					&& settings.content.gameId == gameId;
				neuralDeveloperGamePathUnchanged = settings.content.path == mediaPath;
				neuralDeveloperGameReloadCompleted = neuralDeveloperGameUnloadObserved
					&& neuralDeveloperGameIdUnchanged && neuralDeveloperGamePathUnchanged;
			}
			catch (const FlycastException& e)
			{
				ERROR_LOG(RENDERER, "Neural developer same-media reload failed: %s", e.what());
			}
			writeNeuralDeveloperGameReloadMarker();
			NOTICE_LOG(RENDERER,
				"Neural developer same-media reload completed at main frame %u: %d",
				MainFrameCount, neuralDeveloperGameReloadCompleted ? 1 : 0);
		}
		else if (!neuralDeveloperSaveStateTriggered && neuralReinitAfter == 0
			&& neuralSwitchAfter == 0 && neuralSurfaceSwitchAfter == 0
			&& neuralGameReloadAfter == 0 && neuralPauseAfter == 0
			&& neuralSaveStateAfter > 0
			&& MainFrameCount >= static_cast<u32>(neuralSaveStateAfter)
			&& dc_savestateAllowed())
		{
			neuralDeveloperSaveStateTriggered = true;
			neuralDeveloperSaveStateFrame = MainFrameCount;
			NOTICE_LOG(RENDERER,
				"Neural developer in-memory save requested at main frame %u",
				MainFrameCount);
			try
			{
				emu.stop();
				neuralDeveloperSaveStateSaved = dc_savestateMemory(neuralDeveloperSaveState);
				emu.start();
			}
			catch (const FlycastException& e)
			{
				ERROR_LOG(RENDERER, "Neural developer in-memory save failed: %s", e.what());
				try { emu.start(); } catch (...) { }
			}
			if (!neuralDeveloperSaveStateSaved)
				writeNeuralDeveloperSaveStateMarker();
		}
		else if (neuralDeveloperSaveStateTriggered && neuralDeveloperSaveStateSaved
			&& !neuralDeveloperSaveStateLoaded
			&& MainFrameCount >= neuralDeveloperSaveStateFrame
				+ static_cast<u32>(neuralSaveStateLoadDelay))
		{
			neuralDeveloperLoadStateFrame = MainFrameCount;
			NOTICE_LOG(RENDERER,
				"Neural developer in-memory load requested at main frame %u",
				MainFrameCount);
			try
			{
				emu.stop();
				neuralDeveloperSaveStateLoaded = dc_loadstateMemory(neuralDeveloperSaveState);
				emu.start();
			}
			catch (const FlycastException& e)
			{
				ERROR_LOG(RENDERER, "Neural developer in-memory load failed: %s", e.what());
				try { emu.start(); } catch (...) { }
			}
			writeNeuralDeveloperSaveStateMarker();
			NOTICE_LOG(RENDERER,
				"Neural developer in-memory state round trip completed at main frame %u: %d",
				MainFrameCount, neuralDeveloperSaveStateLoaded ? 1 : 0);
		}
		else if (!neuralDeveloperPauseTriggered && neuralReinitAfter == 0
			&& neuralSwitchAfter == 0 && neuralSurfaceSwitchAfter == 0
			&& neuralGameReloadAfter == 0 && neuralSaveStateAfter == 0
			&& neuralPauseAfter > 0
			&& MainFrameCount >= static_cast<u32>(neuralPauseAfter))
		{
			neuralDeveloperPauseTriggered = true;
			neuralDeveloperPauseFrame = MainFrameCount;
			NOTICE_LOG(RENDERER,
				"Neural developer pause requested at main frame %u", MainFrameCount);
			gui_togglePause();
			neuralDeveloperPauseObserved = gui_state == GuiState::Pause;
			if (!neuralDeveloperPauseObserved)
				writeNeuralDeveloperPauseMarker();
		}
		else if (neuralDeveloperPauseTriggered && neuralDeveloperPauseObserved
			&& !neuralDeveloperResumeObserved
			&& MainFrameCount >= neuralDeveloperPauseFrame
				+ static_cast<u32>(neuralPauseDuration))
		{
			neuralDeveloperResumeFrame = MainFrameCount;
			NOTICE_LOG(RENDERER,
				"Neural developer resume requested at main frame %u", MainFrameCount);
			gui_togglePause();
			neuralDeveloperResumeObserved = gui_state == GuiState::Closed;
			writeNeuralDeveloperPauseMarker();
			NOTICE_LOG(RENDERER,
				"Neural developer pause round trip completed at main frame %u: %d",
				MainFrameCount, neuralDeveloperResumeObserved ? 1 : 0);
		}
		else if (!neuralDeveloperModeRoundtripTriggered && neuralReinitAfter == 0
			&& neuralSwitchAfter == 0 && neuralSurfaceSwitchAfter == 0
			&& neuralGameReloadAfter == 0 && neuralSaveStateAfter == 0
			&& neuralPauseAfter == 0 && neuralModeRoundtripAfter > 0
			&& MainFrameCount >= static_cast<u32>(neuralModeRoundtripAfter)
			&& config::NeuralMode.get() > 0)
		{
			neuralDeveloperModeRoundtripTriggered = true;
			neuralDeveloperModeRoundtripOriginal = config::NeuralMode.get();
			neuralDeveloperModeOffFrame = MainFrameCount;
			config::NeuralMode = 0;
			NOTICE_LOG(RENDERER,
				"Neural developer live mode-off requested at main frame %u: %d -> 0",
				MainFrameCount, neuralDeveloperModeRoundtripOriginal);
		}
		else if (neuralDeveloperModeRoundtripTriggered
			&& !neuralDeveloperModeRoundtripCompleted
			&& MainFrameCount >= neuralDeveloperModeOffFrame
				+ static_cast<u32>(neuralModeOffDuration))
		{
			neuralDeveloperModeOnFrame = MainFrameCount;
			config::NeuralMode = neuralDeveloperModeRoundtripOriginal;
			neuralDeveloperModeRoundtripCompleted = true;
			writeNeuralDeveloperModeRoundtripMarker();
			NOTICE_LOG(RENDERER,
				"Neural developer live mode-on restored at main frame %u: 0 -> %d",
				MainFrameCount, neuralDeveloperModeRoundtripOriginal);
		}
#endif

		if (config::RendererType != currentRenderer || forceReinit)
		{
			mainui_term();
			int prevApi = isOpenGL(currentRenderer) ? 0 : isVulkan(currentRenderer) ? 1 : currentRenderer == RenderType::DirectX9 ? 2 : 3;
			int newApi = isOpenGL(config::RendererType) ? 0 : isVulkan(config::RendererType) ? 1 : config::RendererType == RenderType::DirectX9 ? 2 : 3;
			if (newApi != prevApi || forceReinit)
			{
				try {
					switchRenderApi();
				} catch (const FlycastException& e) {
					ERROR_LOG(RENDERER, "switchRenderApi failed: %s", e.what());
					if (prevApi == newApi)
						// fatal
						throw;
					// try to go back to the previous API
					config::RendererType = currentRenderer;
					try {
						switchRenderApi();
					} catch (const FlycastException& e) {
						ERROR_LOG(RENDERER, "Falling back to previous renderer also failed: %s", e.what());
						// fatal
						throw;
					}
				}
			}
			mainui_init();
#ifdef FLYCAST_ENABLE_NEURAL
			if (neuralDeveloperReinitPending)
			{
				neuralDeveloperReinitPending = false;
				writeNeuralDeveloperReinitMarker();
				NOTICE_LOG(RENDERER,
					"Neural developer renderer reinit completed at main frame %u",
					MainFrameCount);
			}
			if (neuralDeveloperSwitchPending)
			{
				neuralDeveloperSwitchPending = false;
				writeNeuralDeveloperSwitchMarker();
				NOTICE_LOG(RENDERER,
					"Neural developer renderer switch completed at main frame %u: %d -> %d",
					MainFrameCount, neuralDeveloperSwitchFrom, neuralDeveloperSwitchTo);
			}
			if (neuralDeveloperSurfaceSwitchPending)
			{
				neuralDeveloperSurfaceSwitchPending = false;
				writeNeuralDeveloperSurfaceSwitchMarker();
				NOTICE_LOG(RENDERER,
					"Neural developer surface switch completed at main frame %u: %d -> %d",
					MainFrameCount, neuralDeveloperSurfaceSwitchFrom,
					neuralDeveloperSurfaceSwitchTo);
			}
			if (neuralDeveloperDeviceRemovalPending)
			{
				neuralDeveloperDeviceRemovalPending = false;
#if defined(_WIN32)
				DX11Context *context = DX11Context::Instance();
				neuralDeveloperDeviceRemovalRecovered = context
					&& context->isD3D11On12();
#endif
				writeNeuralDeveloperDeviceRemovalMarker();
				NOTICE_LOG(RENDERER,
					"Neural developer actual D3D12 device removal recovery completed at main frame %u: %d",
					MainFrameCount, neuralDeveloperDeviceRemovalRecovered ? 1 : 0);
			}
#endif
			forceReinit = false;
			currentRenderer = config::RendererType;
		}

		fc_profiler::endThread(config::ProfilerFrameWarningTime);
	}

	mainui_term();
}

void mainui_start()
{
#ifdef FLYCAST_ENABLE_NEURAL
	neuralDeveloperReinitTriggered = false;
	neuralDeveloperReinitPending = false;
	neuralDeveloperSwitchTriggered = false;
	neuralDeveloperSwitchPending = false;
	neuralDeveloperSurfaceSwitchTriggered = false;
	neuralDeveloperSurfaceSwitchPending = false;
	neuralDeveloperGameReloadTriggered = false;
	neuralDeveloperGameReloadCompleted = false;
	neuralDeveloperGameUnloadObserved = false;
	neuralDeveloperGameIdUnchanged = false;
	neuralDeveloperGamePathUnchanged = false;
	neuralDeveloperSaveStateTriggered = false;
	neuralDeveloperSaveStateSaved = false;
	neuralDeveloperSaveStateLoaded = false;
	neuralDeveloperSaveStateFrame = 0;
	neuralDeveloperLoadStateFrame = 0;
	neuralDeveloperSaveState.clear();
	neuralDeveloperPauseTriggered = false;
	neuralDeveloperPauseObserved = false;
	neuralDeveloperResumeObserved = false;
	neuralDeveloperPauseFrame = 0;
	neuralDeveloperResumeFrame = 0;
	neuralDeveloperModeRoundtripTriggered = false;
	neuralDeveloperModeRoundtripCompleted = false;
	neuralDeveloperModeRoundtripOriginal = 0;
	neuralDeveloperModeOffFrame = 0;
	neuralDeveloperModeOnFrame = 0;
	neuralDeveloperDeviceRemovalTriggered = false;
	neuralDeveloperDeviceRemovalPending = false;
	neuralDeveloperDeviceRemovalRequested = false;
	neuralDeveloperDeviceRemovalObserved = false;
	neuralDeveloperDeviceRemovalRecovered = false;
	neuralDeveloperDeviceRemovalFrame = 0;
	neuralDeveloperDeviceRemovalReason = 0;
#endif
	mainui_enabled = true;
}

void mainui_stop()
{
	mainui_enabled = false;
}

void mainui_reinit()
{
	forceReinit = true;
}
