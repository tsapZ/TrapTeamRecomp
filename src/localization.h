#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace trapteamrecomp {

// The game's data folder (rom): its alchemy.xml names the disc's SKU, which decides the languages the game
// offers (localization.cpp). Call before ApplyLanguageSetting and before the settings menu is built.
void SetDiscRoot(const std::filesystem::path& root);

// The languages this disc offers, as the language setting names them ("english", "german", "swedish", ...), the
// disc's default first.
const std::vector<std::string>& DiscLanguages();

// A language's name in itself for the settings menu ("Deutsch", "Svenska", ...), UTF-8.
const char* LanguageLabel(const std::string& name);

// Resolves the `language` cvar (one the disc offers, "auto", or a raw numeric Xbox 360 language ID for
// experimentation) and applies it to `user_language` (XGetLanguage) and, where the disc picks the language by
// the console's locale, `user_country` (XGetLocale). Call once at startup, before the game can query either.
void ApplyLanguageSetting();

}  // namespace trapteamrecomp
