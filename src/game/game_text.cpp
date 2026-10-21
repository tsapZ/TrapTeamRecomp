// The game's texts for the portal menu.
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

#include "game_text.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <rex/logging.h>

#include "game/alchemy/iga11_archive.h"
#include "game/alchemy/igz8_file.h"
#include "portal/figure_catalog.h"
#include "portal_rex/menu_draw.h"
#include "portal_rex/portal_menu.h"

namespace trapteamrecomp {
namespace {

constexpr char kItemDir[] = "item";
constexpr char kItemPrefix[] = "Item_";
constexpr char kScene[] = "level.bld";
constexpr char kEnglishPack[] = "ENGLISH.pak";
constexpr char kGermanPack[] = "GERMAN.pak";
constexpr std::string_view kNameSuffix = "_String";
constexpr uint32_t kKeyField = 0x08;    // StringInfo
constexpr uint32_t kTextsField = 0x18;  // LanguagePackInfo
constexpr uint16_t kAlternateDeco = 0x400;

std::mutex g_mutex;
std::unordered_map<uint16_t, std::string> g_names;  // by catalog id, UTF-8

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
  REXLOG_INFO("Game text: {} item names from {}", found, pack);
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
  skylanders::SetFigureNameProvider([](uint16_t id, uint16_t variant) -> std::optional<std::string> {
    if (variant & kAlternateDeco) return std::nullopt;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_names.find(id);
    if (it == g_names.end()) return std::nullopt;
    return it->second;
  });
}

}  // namespace trapteamrecomp
