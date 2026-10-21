// The hats for the portal menu's details view, in the menu's language, as Giants' menu has them
// (GiantsRecomp src/game/toy_hats.cpp; this disc has the same layout in IGA 11 / IGZ 8). Read
// 2026-10-22, after the owner saw only a hat's number in the later games' menus:
//
// - Text: a scene (an archive's level.bld) has a StringInfo object per text it uses, +0x08 its key;
//   ENGLISH.pak and GERMAN.pak next to it hold one LanguagePackInfo each (+0x14 the language, 6
//   English, 9 German; +0x18 a memory reference to the texts, UTF-16BE, NUL-separated, in the
//   order of the scene's StringInfo objects). Every archive here has both packs, so the menu
//   shows the hats in its own language.
// - Names: permanent/global.bld's texts "HatName(kkk)", 254 of them ("HatName(000)" "Combat Hat"
//   / "Kampfhut"). kkk is the hat's number minus one up to 95, and the number itself from 97 on:
//   "HatName(096)" is empty and the list goes on with the same numbers as the hats ("HatName(097)"
//   "Rain Hat" is hat 97). Checked against SuperChargers' hat table (its CHatCollectibleTracker
//   numbers and names, which the figures share): all 245 names agree with that rule, none with a
//   neighbour (Gold Top Hat 96, Silver Top Hat 95, Palm Hat 183, Molekin Mountain Hat 250).
// - Bonuses and pictures: misc/StoreItem_Hat_kkk.bld for the 72 hats the game sells, the same kkk:
//   its scene's text "Hat_Desc_kkk" lists the bonuses, one per line ("+15 Elemental Power" /
//   "+15 Elementarkraft"); misc/StoreItem_Hat_kkk.arc holds the icon
//   (".../ui_main/sprites/haticons/sky2/army.png/...igb.tex.igz", stored upside down like the
//   portraits).
// - Stat symbols: character/UI_StatsScreen_Extra.bld, the stats screen's StatIcon_Armor,
//   _CriticalHit, _ElementalPower and _Speed; which stat a bonus line is comes from the English
//   text of the same line.

#include "toy_hats.h"

#include <cstdio>
#include <mutex>
#include <optional>
#include <regex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga11_archive.h"
#include "game/alchemy/igz8_file.h"
#include "game/xenos_dxt.h"
#include "portal_rex/menu_draw.h"

namespace trapteamrecomp {
namespace {

using alchemy::Iga11Archive;
using alchemy::Igz8File;
using skylanders::FigureImage;
using skylanders::HatInfo;
using skylanders::HatStat;

constexpr char kGlobalArchive[] = "permanent/global.bld";
constexpr char kHatTexts[] = "misc/StoreItem_Hat_%03u.bld";
constexpr char kHatIcons[] = "misc/StoreItem_Hat_%03u.arc";
constexpr char kStatsArchive[] = "character/UI_StatsScreen_Extra.bld";
constexpr char kScene[] = "level.bld";
constexpr char kEnglishPack[] = "ENGLISH.pak";
constexpr char kGermanPack[] = "GERMAN.pak";
constexpr std::string_view kIconPath = "/haticons/";
constexpr std::string_view kHatKey = "hat:";
constexpr std::string_view kDescPrefix = "Hat_Desc_";
constexpr uint32_t kKeyField = 0x08;    // StringInfo
constexpr uint32_t kTextsField = 0x18;  // LanguagePackInfo
constexpr uint32_t kLastIndex = 253;    // "HatName(253)"
constexpr uint32_t kGapIndex = 96;      // "HatName(096)", empty

// A bonus's stat by its English name, and the stats screen's picture of it.
constexpr std::pair<std::string_view, std::string_view> kStatPictures[] = {
    {"Elemental Power", "StatIcon_ElementalPower"},
    {"Armor", "StatIcon_Armor"},
    {"Critical Hit", "StatIcon_CriticalHit"},
    {"Speed", "StatIcon_Speed"},
};

struct Hat {
  std::string name[2];             // English, German
  std::vector<HatStat> stats[2];
  bool picture = false;
};

std::mutex g_mutex;
std::unordered_map<uint16_t, Hat> g_hats;
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_images;
std::unordered_map<uint32_t, std::shared_ptr<const HatInfo>> g_infos;  // hat << 1 | German

// A hat's number for a text index kkk, and back.
uint32_t HatNumber(uint32_t index) { return index < kGapIndex ? index + 1 : index; }

std::optional<Igz8File> ParseEntry(Iga11Archive& archive, size_t index) {
  Iga11Archive* a = &archive;
  return Igz8File::Parse(
      [a, index](uint64_t offset, size_t size, uint8_t* out) { return a->Read(index, offset, size, out); },
      archive.length(index));
}

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

// The scene's texts in one language pack: (key, text) in the scene's order.
std::vector<std::pair<std::string, std::string>> SceneTexts(Iga11Archive& archive, const char* pack) {
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

// "+5 Armor\n+5 Elemental Power" -> the two bonuses.
std::vector<HatStat> ParseBonuses(const std::string& text) {
  static const std::regex kBonus(R"(([+-])\s*(\d+)\s+([^\n]+))");
  std::vector<HatStat> out;
  for (std::sregex_iterator it(text.begin(), text.end(), kBonus), end; it != end; ++it) {
    HatStat stat;
    stat.value = float(std::stoi((*it)[2])) * ((*it)[1] == "-" ? -1.0f : 1.0f);
    stat.name = (*it)[3];
    while (!stat.name.empty() && (stat.name.back() == '\r' || stat.name.back() == ' ')) {
      stat.name.pop_back();
    }
    out.push_back(std::move(stat));
  }
  return out;
}

std::shared_ptr<FigureImage> Upright(RgbaImage rgba) {
  FlipVertically(rgba);  // stored upside down
  auto out = std::make_shared<FigureImage>();
  out->width = rgba.width;
  out->height = rgba.height;
  out->rgba = std::move(rgba.pixels);
  return out;
}

// The first picture in an archive file whose name `want` accepts, upright.
template <typename Want>
std::shared_ptr<FigureImage> FirstPicture(Iga11Archive& archive, Want want) {
  for (size_t i = 0; i < archive.size(); ++i) {
    if (!want(archive.name(i))) continue;
    auto igz = ParseEntry(archive, i);
    if (!igz) continue;
    for (const auto& image : igz->Images()) {
      if (auto rgba = igz->Decode(image)) return Upright(std::move(*rgba));
    }
  }
  return nullptr;
}

int ReadStatPictures(const std::filesystem::path& rom,
                     std::unordered_map<std::string, std::shared_ptr<const FigureImage>>& images) {
  auto archive = Iga11Archive::Open(rom / kStatsArchive);
  const auto scene_index = archive ? archive->Find(kScene) : std::nullopt;
  if (!scene_index) return 0;
  auto igz = ParseEntry(*archive, *scene_index);
  if (!igz) return 0;
  int count = 0;
  for (const auto& image : igz->Images()) {
    for (const auto& [stat, picture] : kStatPictures) {
      if (!image.name.ends_with(std::string("/") + std::string(picture) + ".png")) continue;
      if (auto rgba = igz->Decode(image)) {
        images["stat:" + std::string(picture)] = Upright(std::move(*rgba));
        ++count;
      }
    }
  }
  return count;
}

void Load(std::filesystem::path rom) {
  std::unordered_map<uint16_t, Hat> hats;
  std::unordered_map<std::string, std::shared_ptr<const FigureImage>> images;
  static const std::regex kName(R"(^HatName\((\d+)\)$)");
  if (auto global = Iga11Archive::Open(rom / kGlobalArchive)) {
    for (int german = 0; german < 2; ++german) {
      for (auto& [key, text] : SceneTexts(*global, german ? kGermanPack : kEnglishPack)) {
        std::smatch m;
        if (text.empty() || !std::regex_match(key, m, kName)) continue;
        const uint32_t index = uint32_t(std::stoul(m[1]));
        if (index > kLastIndex || index == kGapIndex) continue;
        hats[uint16_t(HatNumber(index))].name[german] = std::move(text);
      }
    }
  }
  const int stat_pictures = ReadStatPictures(rom, images);
  int sold = 0;
  for (uint32_t index = 0; index <= kLastIndex; ++index) {
    char file[64];
    std::snprintf(file, sizeof(file), kHatTexts, index);
    auto texts = Iga11Archive::Open(rom / file);
    if (!texts) continue;
    Hat& hat = hats[uint16_t(HatNumber(index))];
    for (int german = 0; german < 2; ++german) {
      for (const auto& [key, text] : SceneTexts(*texts, german ? kGermanPack : kEnglishPack)) {
        if (key.starts_with(kDescPrefix)) hat.stats[german] = ParseBonuses(text);
      }
    }
    // Each line's symbol, by the English line's stat.
    for (int german = 0; german < 2; ++german) {
      for (size_t i = 0; i < hat.stats[german].size() && i < hat.stats[0].size(); ++i) {
        for (const auto& [stat, picture] : kStatPictures) {
          if (hat.stats[0][i].name == stat) hat.stats[german][i].image = "stat:" + std::string(picture);
        }
      }
    }
    std::snprintf(file, sizeof(file), kHatIcons, index);
    if (auto icons = Iga11Archive::Open(rom / file)) {
      auto icon = FirstPicture(*icons, [](const std::string& name) {
        return name.find(kIconPath) != std::string::npos;
      });
      if (icon) {
        images[std::string(kHatKey) + std::to_string(HatNumber(index))] = std::move(icon);
        hat.picture = true;
      }
    }
    ++sold;
  }
  size_t named = 0, pictures = 0;
  for (const auto& [number, hat] : hats) {
    named += !hat.name[0].empty();
    pictures += hat.picture;
  }
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_hats = std::move(hats);
    g_images = std::move(images);
    g_infos.clear();
  }
  REXLOG_INFO("Toy hats: {} hat names, {} sold with bonuses, {} pictures, {} stat symbols", named,
              sold, pictures, stat_pictures);
}

std::shared_ptr<const HatInfo> InfoFor(uint16_t number) {
  const int german = skylanders::menu_draw::GermanText() ? 1 : 0;
  std::lock_guard<std::mutex> lock(g_mutex);
  const uint32_t key = (uint32_t(number) << 1) | uint32_t(german);
  if (auto it = g_infos.find(key); it != g_infos.end()) return it->second;
  auto hat = g_hats.find(number);
  if (hat == g_hats.end()) return nullptr;
  auto info = std::make_shared<HatInfo>();
  info->name = hat->second.name[german].empty() ? hat->second.name[0] : hat->second.name[german];
  info->stats = hat->second.stats[german].empty() ? hat->second.stats[0] : hat->second.stats[german];
  if (hat->second.picture) info->image = std::string(kHatKey) + std::to_string(number);
  g_infos[key] = info;
  return info;
}

}  // namespace

void InstallToyHats(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir).detach();
  skylanders::SetHatInfoProvider([](uint16_t hat) { return InfoFor(hat); });
}

std::shared_ptr<const FigureImage> HatImage(const std::string& key) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_images.find(key);
  return it == g_images.end() ? nullptr : it->second;
}

}  // namespace trapteamrecomp
