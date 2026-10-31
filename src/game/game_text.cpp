// The game's texts for the portal menu, and the levels' names for the Discord status.
//
// Text in this game is stored as in Giants (GiantsRecomp src/game/game_text.cpp), in IGA 11 /
// IGZ 8 here: a scene (an archive's level.bld) has a StringInfo object for every text it uses,
// +0x08 pointing to its key ("458_Name(00)"); the archive's language packs, ENGLISH.pak and
// GERMAN.pak on this disc, hold one LanguagePackInfo each: +0x14 the language (6 English, 9
// German), +0x18 a memory reference to the texts, UTF-16BE and NUL-terminated, in the order of
// the scene's StringInfo objects (read 2026-10-22 on global.bld, the hat archives and the
// character archives).
// Magic items: every item has rom/item/Item_<name>.bld whose first "..._String" text is its
// name ("Anvil Rain" / "Ambossregen"). Which catalog figure an archive is: the catalog entry with
// the same English name, so no table is needed here; the German name then wins for the plain
// variants (an alternate deco keeps the catalog's, as in Giants).
// This game's own items (Hand of Fate, Piggy Bank, Rocket Ram, Tiki Speaky) have no texts in their
// archives; they are named, like every item and the older adventure packs, in the game's toy lists
// in rom/character/Init_Setup.bld: the texts "Item_List" and "Item_List - Legacy Location Pieces",
// one line per toy, "<toy id>,<archive>,<name>" ("230,Item_HelpingHand,Schicksalshand" in the
// German pack). Read 2026-10-23; a toy the item archives didn't name gets the list's name (the list
// shortens a few, "Kanone" for the Dragonfire Cannon, which is why the archives come first). This
// game's adventure packs are in "Location_List" as "<toy id>,<archive>,level=<number>"; their
// name is the level's, "Level_<number>_<name>(1)" in rom/permanent/global.bld
// ("Level_327_Kaos_Exp(1)": Mirror of Mystery).

#include "game_text.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iterator>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <rex/logging.h>

#include "alchemy/iga11_archive.h"
#include "alchemy/igz8_file.h"
#include "portal/figure_catalog.h"
#include "portal_rex/discord_presence.h"
#include "portal_rex/menu_draw.h"
#include "portal_rex/portal_menu.h"

namespace trapteamrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

constexpr char kItemDir[] = "item";
constexpr char kItemPrefix[] = "Item_";
constexpr char kScene[] = "level.bld";
constexpr char kEnglishPack[] = "ENGLISH.pak";
constexpr char kGermanPack[] = "GERMAN.pak";
constexpr std::string_view kNameSuffix = "_String";
constexpr char kToyListArchive[] = "character/Init_Setup.bld";
constexpr std::string_view kToyLists[] = {"Item_List", "Item_List - Legacy Location Pieces",
                                          "Location_List"};
constexpr std::string_view kLevelField = "level=";
constexpr char kGlobalArchive[] = "permanent/global.bld";
constexpr std::string_view kLevelPrefix = "Level_";       // "Level_327_Kaos_Exp(1)"
constexpr std::string_view kLevelNameSuffix = "(1)";
constexpr std::string_view kHubPrefix = "Level_Hub";               // "Level_Hub_2014_01"
constexpr std::string_view kHubNameKey = "999_Level_Village(1)";  // "Skylanders Academy"
constexpr uint32_t kKeyField = 0x08;    // StringInfo
constexpr uint32_t kTextsField = 0x18;  // LanguagePackInfo
constexpr uint16_t kAlternateDeco = 0x400;

std::mutex g_mutex;
std::unordered_map<uint16_t, std::string> g_names;  // by catalog id, UTF-8
std::unordered_map<uint16_t, std::string_view> g_plain_names;  // the catalog's, by id

void AppendUtf8(std::string& out, uint32_t c) {
  if (c < 0x80) {
    out.push_back(char(c));
  } else if (c < 0x800) {
    out.push_back(char(0xC0 | (c >> 6)));
    out.push_back(char(0x80 | (c & 0x3F)));
  } else if (c < 0x10000) {
    out.push_back(char(0xE0 | (c >> 12)));
    out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
    out.push_back(char(0x80 | (c & 0x3F)));
  } else {
    out.push_back(char(0xF0 | (c >> 18)));
    out.push_back(char(0x80 | ((c >> 12) & 0x3F)));
    out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
    out.push_back(char(0x80 | (c & 0x3F)));
  }
}

// The UTF-16BE texts, NUL-separated, as UTF-8.
std::vector<std::string> SplitTexts(const std::vector<uint8_t>& data) {
  std::vector<std::string> out(1);
  for (size_t i = 0; i + 1 < data.size(); i += 2) {
    uint32_t c = (uint32_t(data[i]) << 8) | data[i + 1];
    if (!c) {
      out.emplace_back();
      continue;
    }
    if (c >= 0xD800 && c < 0xDC00 && i + 3 < data.size()) {
      const uint32_t low = (uint32_t(data[i + 2]) << 8) | data[i + 3];
      if (low >= 0xDC00 && low < 0xE000) {
        c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
        i += 2;
      }
    }
    AppendUtf8(out.back(), c);
  }
  return out;
}

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

// A text's lines, without line ends.
std::vector<std::string> SplitLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(std::move(line));
    start = end + 1;
  }
  return lines;
}

// The name text of an item archive in `pack`: its scene's first "..._String" text.
std::string ItemName(alchemy::Iga11Archive& archive, const std::string& pack) {
  for (const auto& [key, text] : ReadSceneTexts(archive, pack)) {
    if (key.ends_with(kNameSuffix)) return text;
  }
  return {};
}

void Load(std::filesystem::path rom, std::string pack) {
  // The catalog's plain figures by English name, lower case.
  std::unordered_map<std::string, uint16_t> by_name;
  for (const auto& sky : skylanders::portal::AllSkylanders()) {
    if (sky.category == "Items" && !(sky.variant & kAlternateDeco)) by_name.emplace(Lower(std::string(sky.name)), sky.id);
  }
  int found = 0;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(rom / kItemDir, ec)) {
    const std::string file = entry.path().filename().string();
    if (!file.starts_with(kItemPrefix) || entry.path().extension() != ".bld") continue;
    auto archive = alchemy::Iga11Archive::Open(entry.path());
    if (!archive) continue;
    auto id = by_name.find(Lower(ItemName(*archive, kEnglishPack)));
    if (id == by_name.end()) continue;
    std::string name = ItemName(*archive, pack);
    if (name.empty()) continue;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_names[id->second] = std::move(name);
    ++found;
  }
  // The levels' names by number ("327"), for the adventure packs of Location_List.
  std::unordered_map<std::string, std::string> level_names;
  std::string hub;
  if (auto global = alchemy::Iga11Archive::Open(rom / kGlobalArchive)) {
    for (auto& [key, text] : ReadSceneTexts(*global, pack)) {
      if (key == kHubNameKey) hub = text;
      if (!key.starts_with(kLevelPrefix) || !key.ends_with(kLevelNameSuffix)) continue;
      const size_t number = kLevelPrefix.size(), end = key.find('_', number);
      if (end != std::string::npos) level_names.emplace(key.substr(number, end - number), std::move(text));
    }
  }
  // And for the Discord status: the level as where_hook.cpp names it, its file ("Level_301_The_Machine"
  // -> "301"), the hub's ("Level_Hub_2014_01") by the Academy's own key ("999_Level_Village(1)",
  // "Skylanders Academy" / "Skylander-Akademie"); the challenges, arenas and the title have none.
  auto by_level = std::make_shared<const std::unordered_map<std::string, std::string>>(level_names);
  skylanders::SetDiscordLevelNames([by_level, hub](const std::string& level) {
    if (level.starts_with(kHubPrefix)) return hub;
    if (!level.starts_with(kLevelPrefix)) return std::string();
    const size_t number = kLevelPrefix.size(), end = level.find('_', number);
    const auto it = by_level->find(level.substr(number, end == std::string::npos ? end : end - number));
    return it == by_level->end() ? std::string() : it->second;
  });
  REXLOG_INFO("Game text: {} level names", level_names.size());
  int listed = 0;
  if (auto setup = alchemy::Iga11Archive::Open(rom / kToyListArchive)) {
    for (const auto& [key, text] : ReadSceneTexts(*setup, pack)) {
      if (std::find(std::begin(kToyLists), std::end(kToyLists), key) == std::end(kToyLists)) continue;
      for (const auto& line : SplitLines(text)) {
        // "<toy id>,<archive>,<name>" or "<toy id>,<archive>,level=<number>"
        const size_t first = line.find(','), second = line.find(',', first + 1);
        if (first == std::string::npos || second == std::string::npos) continue;
        uint32_t id = 0;
        const auto [end, ec] = std::from_chars(line.data(), line.data() + first, id);
        std::string name = line.substr(second + 1);
        while (!name.empty() && (name.back() == ',' || name.back() == ' ')) name.pop_back();
        if (name.starts_with(kLevelField)) {
          auto level = level_names.find(name.substr(kLevelField.size()));
          name = level == level_names.end() ? std::string() : level->second;
        }
        if (ec != std::errc() || end != line.data() + first || id > 0xFFFF || name.empty()) continue;
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_names.emplace(uint16_t(id), std::move(name)).second) ++listed;
      }
    }
  }
  // The catalog's name for each toy's plain figure (its lowest variant), for Name().
  std::unordered_map<uint16_t, std::string_view> plain;
  std::unordered_map<uint16_t, uint16_t> lowest;
  for (const auto& sky : skylanders::portal::AllSkylanders()) {
    auto [it, added] = lowest.emplace(sky.id, sky.variant);
    if (added || sky.variant < it->second) {
      it->second = sky.variant;
      plain[sky.id] = sky.name;
    }
  }
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_plain_names = std::move(plain);
  }
  REXLOG_INFO("Game text: {} item names from {}, {} more from the toy lists", found, pack, listed);
}

// The game's name for a catalog figure: the toy's for its plain figure, plus what the catalog adds
// to that ("Midnight Museum (LightCore)" -> "Mitternachts-Museum (LightCore)"); a variant the
// catalog names otherwise ("Platinum Treasure Chest") and an alternate deco keep the catalog's.
std::optional<std::string> Name(uint16_t id, uint16_t variant) {
  if (variant & kAlternateDeco) return std::nullopt;
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_names.find(id);
  if (it == g_names.end()) return std::nullopt;
  const auto* sky = skylanders::portal::FindSkylander(id, variant);
  auto base = g_plain_names.find(id);
  if (!sky || base == g_plain_names.end() || sky->name == base->second) return it->second;
  if (!sky->name.starts_with(base->second)) return std::nullopt;
  return it->second + std::string(sky->name.substr(base->second.size()));
}

}  // namespace

std::string LanguagePackName() {
  return skylanders::menu_draw::GermanText() ? kGermanPack : kEnglishPack;
}

std::optional<alchemy::Igz8File> ParseEntry(alchemy::Iga11Archive& archive, size_t index) {
  alchemy::Iga11Archive* a = &archive;
  return alchemy::Igz8File::Parse(
      [a, index](uint64_t offset, size_t size, uint8_t* out) { return a->Read(index, offset, size, out); },
      archive.length(index));
}

std::vector<std::pair<std::string, std::string>> ReadSceneTexts(alchemy::Iga11Archive& archive,
                                                                const std::string& pack) {
  std::vector<std::pair<std::string, std::string>> out;
  const auto scene_index = archive.Find(kScene);
  const auto pack_index = archive.Find(pack);
  if (!scene_index || !pack_index) return out;
  auto scene = ParseEntry(archive, *scene_index);
  auto texts_file = ParseEntry(archive, *pack_index);
  if (!scene || !texts_file) return out;
  const auto info = texts_file->Objects("LanguagePackInfo");
  if (info.empty()) return out;
  const auto data = texts_file->MemoryField(info.front(), kTextsField);
  if (!data) return out;
  std::vector<std::string> texts = SplitTexts(*data);
  const auto keys = scene->Objects("StringInfo");
  for (size_t i = 0; i < keys.size() && i < texts.size(); ++i) {
    out.emplace_back(scene->StringField(keys[i], kKeyField), std::move(texts[i]));
  }
  return out;
}

void InstallGameText(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir, LanguagePackName()).detach();
  skylanders::SetFigureNameProvider(Name);
}

}  // namespace trapteamrecomp
