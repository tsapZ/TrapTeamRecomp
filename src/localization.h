#pragma once

#include <filesystem>
#include <string>

namespace trapteamrecomp {

// The game's data folder (rom): which of the languages taken from the console's country (Dutch and
// the Nordic four) the disc has is seen from its text files. Call before ApplyLanguageSetting and
// before the settings menu is built.
void SetDiscRoot(const std::filesystem::path& root);

// Whether the disc has one of the languages taken from the console's country ("dutch", "swedish",
// "norwegian", "danish", "finnish"): its text file (item\3d_font_msg_<language>.bld). False for any
// other name; true for these while no data folder is set.
bool DiscHasCountryLanguage(const std::string& name);

// Resolves the `language` cvar (a friendly name, or a raw numeric Xbox 360 language ID for
// experimentation) and applies it to the `user_language` cvar that XGetLanguage/
// ExGetXConfigSetting actually read. Call once at startup, before the game can query either.
void ApplyLanguageSetting();

}  // namespace trapteamrecomp
