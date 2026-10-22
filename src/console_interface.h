// Which console interface a PC pak is drawn with; see console_interface.cpp.
#pragma once

#include <string>
#include <vector>

/// The folder to mount as `RendererConfig::console_ui` (beside the Xbox pak nothing is
/// mounted): what was asked for, else the port's own interface pack, which every build
/// carries (cmake/ui_pack.cmake).
std::string console_interface(const std::string& asked, const std::vector<std::string>& extra_data);
