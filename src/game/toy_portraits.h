#pragma once

// The game's own pictures of the Skylanders for the portal menu (toy_portraits.cpp).

#include <filesystem>

namespace trapteamrecomp {

// Reads the collection screens' cards (and the HUD portraits from the character scenes, for ids
// without a card) and the element symbols and gold coin in the background and gives them to the
// portal menu (SetFigureImageProvider, SetArtImageProvider).
// `rom_dir` is the game's folder (rom/).
void InstallToyPortraits(const std::filesystem::path& rom_dir);

}  // namespace trapteamrecomp
