// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_view_scene.h"
#include "remake_scene.h"
#include <functional>
namespace flycast::rend::neural {
using RemakeTextureReader=std::function<bool(const PvrCapturedDraw&,std::vector<unsigned char>&,std::string&)>;
bool BuildRemakeViewPacket(const RemakeViewScene&,const RemakeTextureReader&,remake::Packet&,std::string&);
// Explicit bounded diagnostic transport. Same owned packet can later cross an
// IPC boundary; reading a saved packet alone is never live integration proof.
bool WriteRemakeViewPacket(const std::filesystem::path&,const remake::Packet&,std::string&);
bool ReadRemakeViewPacket(const std::filesystem::path&,remake::Packet&,std::string&);
}
