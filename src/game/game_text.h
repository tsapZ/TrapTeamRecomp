#pragma once

// The game's texts: reading a scene's texts, and the magic items' names for the portal menu from
// the disc's item archives (see game_text.cpp).

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace trapteamrecomp {

namespace alchemy {
class Iga11Archive;
class Igz8File;
}  // namespace alchemy

// The language pack the menu reads ("GERMAN.pak" when the game runs in German, else
// "ENGLISH.pak", the two this disc has).
std::string LanguagePackName();

// An IGZ file inside an archive, read through the archive (only the blocks it needs).
std::optional<alchemy::Igz8File> ParseEntry(alchemy::Iga11Archive& archive, size_t index);

// The texts of an archive's scene in language pack `pack`: (key, text) pairs in the scene's
// order, the texts in UTF-8; empty without a scene or that pack.
std::vector<std::pair<std::string, std::string>> ReadSceneTexts(alchemy::Iga11Archive& archive,
                                                                const std::string& pack);

// Reads the magic items' names from `rom_dir`/item on a background thread, in the menu's
// language, and registers the portal menu's figure name provider.
void InstallGameText(const std::filesystem::path& rom_dir);

}  // namespace trapteamrecomp
