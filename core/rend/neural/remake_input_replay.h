// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "remake_live_channel.h"
namespace flycast::rend::neural {
bool PrepareLockedRemakeInput(const std::filesystem::path& root,std::string& error);
bool SameRemakeReplayScene(const remake::Packet& retained,const remake::Packet& current,std::string& error);
bool WriteLockedRemakeInput(const std::filesystem::path& directory,const remake::Packet&,
 const RemakeReturnedImage&,std::string& error);
bool ReadLockedRemakeInput(const std::filesystem::path& root,const remake::Packet& current,
 RemakeReturnedImage& output,std::uint64_t& originalFrame,std::string& error,
 std::filesystem::path* matchedDirectory=nullptr);
}
