#pragma once

// The hats for the portal menu's details view (toy_hats.cpp).

#include <filesystem>
#include <memory>
#include <string>

#include "portal_rex/portal_menu.h"

namespace trapteamrecomp {

// Reads the hats' names, bonuses and pictures from the disc in the background and gives them to
// the portal menu (SetHatInfoProvider). `rom_dir` is the game's folder (rom/).
void InstallToyHats(const std::filesystem::path& rom_dir);

// The hat pictures and stat symbols for the menu's art provider ("hat:<number>",
// "stat:<name>"), nullptr for other keys or while loading.
std::shared_ptr<const skylanders::FigureImage> HatImage(const std::string& key);

}  // namespace trapteamrecomp
