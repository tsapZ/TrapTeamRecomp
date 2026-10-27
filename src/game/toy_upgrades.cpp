// Every Skylander's upgrades for the portal menu's details view, as in Giants' menu (GiantsRecomp
// src/game/toy_upgrades.cpp has how they were worked out there); this disc stores them the same
// way. Read 2026-10-22 over all 342 character archives:
//
// - The texts are in the Skylander's scene archive (rom/character/NNN_<model>.bld, NNN its figure
//   id; see game_text.cpp for the texts) as "NNN_..." keys: the upgrades Name(kk) / Desc(kk), the
//   two starting attacks (Action1PowerName, PrimaryAttackName, ...), the two paths
//   (UpgradePath1Name or UpgradePath1, UpgradePath1_description). kk is 00-03 the basic upgrades,
//   04-06 the first path, 07-09 the second, 10 the Soul Gem, and from 11 the Wow Pows of the later
//   years' figures: 10 + the figure's year (Spyro's Adventure's Skylanders have 11 "Wow Pow -
//   Fireslam!" for Giants' Series 2 and 12 "Head Start" for Swap Force's Series 3; Giants' 12
//   "Shoot the Breeze!" for Swap Force and 13 "Double Barrel!" for this game; this game's own have
//   none).
// - Swap Force's halves (rom/character/1000_BoomJet_Bot.bld .. 2015_..._Top.bld, the file number
//   their figure id) have one starting attack, three basic upgrades "Name(1)" .. "Name(3)", two per
//   path keyed by the path letter ("Name(4-4a)", "Name(4 - 5a)", "Name(a- 6a)": a or b, in the order
//   of the number before it) and "Name(soulgem)", and Swap Force's bits for a half: the basic
//   upgrades 2-4, the paths 5-6 (the other path's from 11), the Soul Gem 7 (SwapForceRecomp
//   src/game/game_art.cpp). Their pictures are .tga files named after the half's part
//   (".../shapeshifters/legs/stormbird/stormbird_legs_base1.tga"): "base1", "stat" (the second
//   basic upgrade, which raises a stat), "base2", "path1_1" .. "path2_2", "sg" and the starting
//   attack's own name. The prices are the same list's (500, 700, 900; 1700, 2200; 4000), assumed:
//   this disc doesn't store them.
// - The prices are Giants' (500, 700, 900, 1200 for the basic upgrades, 1700, 2200, 3000 along a
//   path, 4000 for the Soul Gem, 5000 for a Wow Pow): this disc's global scene has no
//   UpgradeCost values to read them from.
// - The upgrade bits of a figure are the same as in Giants and Swap Force: bit 0 a path is
//   chosen, bit 1 the second path, the basic four from bit 2, the chosen path's three from 6, the
//   Soul Gem 9, the Wow Pow 10 and the other path's upgrades from 11.
// - The pictures sit in the character's .arc, ".../upgradeposes/<model>/final/<model>_<suffix>
//   .png/...tex.igz", stored upside down: Spyro's Adventure's "up1" ... "up4", "up5a" ... "up7b",
//   "up8" (Soul Gem), "wowpow" or "upgradepowwow" (Series 2), Giants' "upgrade1" ... "upgrade8",
//   this game's "up1" ... "up11" in order, "primary" / "secondary" for the starting attacks, and
//   ".../upgradeposes/s3upgrades/png/512/<name>_s3.png" for Swap Force's Wow Pow. Each is read
//   when the details view first asks for it.

#include "toy_upgrades.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <map>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rex/logging.h>

#include "alchemy/iga11_archive.h"
#include "alchemy/igz8_file.h"
#include "game/game_text.h"
#include "alchemy/xenos_dxt.h"

namespace trapteamrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

using skylanders::FigureImage;
using skylanders::FigureUpgrade;
using skylanders::FigureUpgradePath;
using skylanders::FigureUpgrades;

constexpr char kCharacterDir[] = "character";
constexpr std::string_view kPosesPath = "/upgradeposes/";
// This game's own Skylanders (the 53 archives from 450_Typhoon to 504_Mini_Hex, Trap Masters and
// cores alike) keep theirs in another folder, named after the character rather than the model:
// ".../ui_sky4/upgrades/467_enigma/467_enigma_up1.png" in 467_Void.arc (the owner, 2026-10-25:
// Enigma's tree showed only arrows). The suffix is the same ("up1" ... "up11", "primary").
constexpr std::string_view kSky4Path = "/ui_sky4/upgrades/";
constexpr std::string_view kKeyPrefix = "upgrade:";

constexpr uint32_t kCosts[] = {500, 700, 900, 1200, 1700, 2200, 3000, 4000, 5000};
constexpr int kBasicCount = 4;
constexpr int kPathLength = 3;
constexpr int kFirstPath = 4;
constexpr int kSoulGem = 10;
constexpr int kFirstYearCode = 2011;  // Spyro's Adventure, catalog year 0
constexpr int kFirstUpgradeBit = 2;
constexpr int kFirstPathBit = kFirstUpgradeBit + kBasicCount;
constexpr int kSoulGemBit = kFirstPathBit + kPathLength;
constexpr int kWowPowBit = kSoulGemBit + 1;
constexpr int kFirstAlternateBit = kWowPowBit + 1;

// Swap Force's halves (see the top).
constexpr int kHalfBasic = 0;
constexpr int kHalfPath = 100;
constexpr int kHalfSoulGem = 200;
constexpr int kHalfBasicCount = 3;
constexpr int kHalfPathLength = 2;
constexpr int kHalfFirstPathBit = kFirstUpgradeBit + kHalfBasicCount;
constexpr int kHalfSoulGemBit = kHalfFirstPathBit + kHalfPathLength;
constexpr uint16_t kFirstHalfId = 1000;

// Catalog variants: year << 12; Spyro's Adventure's Legendaries are 400 + its id, Dark Spyro 28.
constexpr int kYearShift = 12;
constexpr uint16_t kLegendaryIds = 400;
constexpr uint16_t kLegendaryEnd = 450;  // this game's own Skylanders start at 450
constexpr uint16_t kDarkSpyro = 28;
constexpr uint16_t kSpyro = 16;

struct CharacterText {
  std::string start_name[2], start_desc[2];
  std::string path_name[2], path_desc[2];
  std::map<int, std::string> name, desc;  // by upgrade number
  // A Swap Force half's: kHalfBasic + 0..2, kHalfPath + path * 10 + its number, kHalfSoulGem.
  std::map<int, std::string> half_name, half_desc;
  std::filesystem::path arc;
  std::unordered_map<std::string, size_t> pictures;  // suffix -> the .arc's entry
};

std::mutex g_mutex;
std::unordered_map<uint16_t, CharacterText> g_characters;
std::unordered_map<uint32_t, std::shared_ptr<const FigureUpgrades>> g_upgrades;  // id << 4 | year
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_images;
std::unordered_set<std::string> g_requested;
std::deque<std::string> g_queue;
bool g_worker = false;

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

bool Has(const std::string& s, std::string_view part) { return s.find(part) != std::string::npos; }

// The year a character's first figure came out (catalog years: 0 Spyro's Adventure, 1 Giants, 2
// Swap Force, 3 this game), by its figure id.
int FirstYear(uint16_t id) {
  if (id < 100) return 0;
  if (id < 200) return 1;
  if (id >= kFirstHalfId) return 2;
  return 3;
}

// The game's button glyphs in its texts, as the button's letter. This game's glyphs differ from
// Giants': "]" is the first attack (X; Whirlwind "Hold ] to charge up a super powerful Rainbow of
// Doom", Wildfire's Shield Bash), "{" the second (Y; Gill Grunt's Power Hose, Wildfire's Chains of
// Fire), "}" the third (B; "Press } to fly"). Checked on Whirlwind's, Spyro's, Gill Grunt's and
// Wildfire's texts against their attacks.
std::string ButtonNames(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case ']': out += 'X'; break;
      case '{': out += 'Y'; break;
      case '}': out += 'B'; break;
      default: out.push_back(c);
    }
  }
  return out;
}

// Sorts one text of a character's scene into `t`; `key` is without its "NNN_". Besides "Name(kk)"
// the keys come as "Name(kk)_1" / "Name(kk)_5a" / "Name(kk)_Aftershock_1" (this game's own
// Skylanders: the picture's number after it), "Name(kk)_2011" (another year's wording of the same
// upgrade, used only where the plain key is missing) and "Name(11_2013)" / "Name(11_2014)" (Gill
// Grunt's and others' Wow Pows for Swap Force and this game, upgrade 10 + the year).
void Classify(std::string key, const std::string& game_text, CharacterText& t) {
  static const std::regex kUpgrade(R"(^(name|descr?)\(?(\d\d)(?:_(\d{4}))?\)?(?:_(\w+))?$)");
  static const std::regex kPath(R"((?:path|branch)_?(1|2|a|b|left|right))");
  static const std::regex kYear(R"(^\d{4}$)");
  static const std::regex kHalfBasicKey(R"(^(name|descr?)\((\d)\)$)");
  static const std::regex kHalfPathKey(R"(^(name|descr?)\(\s*[0-9ab]\s*-\s*(\d)([ab])\)$)");
  static const std::regex kHalfSoulGemKey(R"(^(name|descr?)\(soulgem\)$)");
  const std::string text = ButtonNames(game_text);
  key = Lower(std::move(key));
  std::smatch m;
  if (std::regex_match(key, m, kHalfBasicKey)) {
    (m[1] == "name" ? t.half_name : t.half_desc)[kHalfBasic + std::stoi(m[2]) - 1] = text;
    return;
  }
  if (std::regex_match(key, m, kHalfPathKey)) {
    const int path = m[3] == "a" ? 0 : 1;
    (m[1] == "name" ? t.half_name : t.half_desc)[kHalfPath + path * 10 + std::stoi(m[2])] = text;
    return;
  }
  if (std::regex_match(key, m, kHalfSoulGemKey)) {
    (m[1] == "name" ? t.half_name : t.half_desc)[kHalfSoulGem] = text;
    return;
  }
  if (std::regex_match(key, m, kUpgrade)) {
    int upgrade = std::stoi(m[2]);
    if (m[3].matched) {
      if (upgrade != kSoulGem + 1) return;
      upgrade = kSoulGem + (std::stoi(m[3]) - kFirstYearCode);
    }
    auto& texts = m[1] == "name" ? t.name : t.desc;
    const std::string tag = m[4];
    if (m[4].matched && std::regex_match(tag, kYear)) {
      texts.emplace(upgrade, text);  // only where the plain key is missing
    } else {
      texts[upgrade] = text;
    }
    return;
  }
  const bool desc = Has(key, "desc");
  if (std::regex_search(key, m, kPath)) {
    const std::string which = m[1];
    const int path = which == "1" || which == "a" || which == "left" ? 0 : 1;
    (desc ? t.path_desc : t.path_name)[path] = text;
    return;
  }
  int start = -1;
  if (Has(key, "action1") || Has(key, "primary")) {
    start = 0;
  } else if (Has(key, "action2") || Has(key, "secondary")) {
    start = 1;
  }
  if (start < 0) return;
  if (desc) {
    t.start_desc[start] = text;
  } else if (Has(key, "name")) {
    t.start_name[start] = text;
  }
}

// The picture suffixes an upgrade may have; this game's own numbering ("up" + kk + 1) is tried
// where it can't be another upgrade's picture.
std::vector<std::string> PictureSuffixes(int upgrade, int year) {
  const std::string own = "up" + std::to_string(upgrade + 1);
  if (upgrade < kBasicCount) return {own, "upgrade" + std::to_string(upgrade + 1)};
  if (upgrade < kSoulGem) {
    const int in_path = (upgrade - kFirstPath) % kPathLength;
    const char path = (upgrade - kFirstPath) / kPathLength == 0 ? 'a' : 'b';
    const std::string n = std::to_string(kBasicCount + 1 + in_path) + path;
    return {"up" + n, "upgrade" + n, own};
  }
  if (upgrade == kSoulGem) return {own, "up8", "upgrade8", "upgradesoulgem", "soulgem"};
  if (year == 1) return {"wowpow", "upgradepowwow"};
  if (year == 2) return {"s3"};
  return {};
}

std::string PictureKey(uint16_t id, const CharacterText& t, const std::vector<std::string>& suffixes) {
  for (const std::string& suffix : suffixes) {
    if (t.pictures.contains(suffix)) return std::string(kKeyPrefix) + std::to_string(id) + ":" + suffix;
  }
  return {};
}

void Load(std::filesystem::path rom, std::string pack) {
  std::error_code ec;
  int characters = 0;
  for (const auto& entry : std::filesystem::directory_iterator(rom / kCharacterDir, ec)) {
    const std::string file = entry.path().filename().string();
    // "NNN_" or, for Swap Force's halves, "NNNN_": the figure id.
    size_t digits = 0;
    while (digits < file.size() && std::isdigit((unsigned char)file[digits])) ++digits;
    if (digits < 3 || digits > 4 || digits >= file.size() || file[digits] != '_' ||
        entry.path().extension() != ".bld") {
      continue;
    }
    // The decos share their Skylander's upgrades.
    const std::string stem = Lower(entry.path().stem().string());
    if (stem.ends_with("_legendary") || stem.ends_with("_alt") || stem.ends_with("_red") ||
        stem.ends_with("_dark") || stem.ends_with("musicmini")) {
      continue;
    }
    auto archive = alchemy::Iga11Archive::Open(entry.path());
    if (!archive) continue;
    const auto texts = ReadSceneTexts(*archive, pack);
    const std::string prefix = file.substr(0, digits + 1);
    CharacterText t;
    for (const auto& [key, text] : texts) {
      if (key.starts_with(prefix)) Classify(key.substr(prefix.size()), text, t);
    }
    // Not a playable Skylander (the villains share the halves' numbers: 1001_ChompyMage).
    if (!t.name.contains(kSoulGem) && !t.half_name.contains(kHalfSoulGem)) continue;
    t.arc = entry.path();
    t.arc.replace_extension(".arc");
    if (auto arc = alchemy::Iga11Archive::Open(t.arc)) {
      for (size_t i = 0; i < arc->size(); ++i) {
        const std::string name = Lower(arc->name(i));
        size_t at = name.find(kPosesPath);
        if (at == std::string::npos) at = name.find(kSky4Path);
        if (at == std::string::npos) continue;
        size_t ext = name.find(".png", at);
        if (ext == std::string::npos) ext = name.find(".tga", at);
        if (ext == std::string::npos) continue;
        const std::string base = name.substr(0, ext);
        // A half's part: "stormbird_legs_path1_1" -> "path1_1"; else the last word.
        size_t part = std::string::npos;
        for (std::string_view word : {"_legs_", "_body_"}) {
          if (const size_t w = base.rfind(word); w != std::string::npos) part = w + word.size();
        }
        t.pictures[part != std::string::npos ? base.substr(part) : base.substr(base.find_last_of("/_") + 1)] = i;
      }
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    // The first file of an id wins: "1005_FireKraken_Bot" before its "_Jade" deco.
    g_characters.emplace(uint16_t(std::stoi(file.substr(0, digits))), std::move(t));
    ++characters;
  }
  REXLOG_INFO("Toy upgrades: the upgrades of {} Skylanders from {}", characters, pack);
}

// A Swap Force half's upgrades (see the top).
std::shared_ptr<const FigureUpgrades> BuildHalf(uint16_t id, const CharacterText& t) {
  auto result = std::make_shared<FigureUpgrades>();
  auto text = [](const std::map<int, std::string>& m, int i) {
    auto it = m.find(i);
    return it == m.end() ? std::string() : it->second;
  };
  auto upgrade = [&](int key, int bit, uint32_t cost, const char* picture) {
    FigureUpgrade u;
    u.name = text(t.half_name, key);
    u.description = text(t.half_desc, key);
    u.bit = bit;
    u.cost = cost;
    u.image = PictureKey(id, t, {picture});
    return u;
  };
  // The starting attack: its picture is the one that isn't an upgrade's.
  static const std::unordered_set<std::string> kUpgradePictures = {"base1", "base2", "stat", "path1_1",
                                                                  "path1_2", "path2_1", "path2_2", "sg"};
  FigureUpgradePath start;
  start.kind = FigureUpgradePath::Kind::kStart;
  for (int k = 0; k < 2; ++k) {
    if (t.start_name[k].empty()) continue;
    FigureUpgrade u;
    u.name = t.start_name[k];
    u.description = t.start_desc[k];
    for (const auto& [suffix, entry] : t.pictures) {
      if (!kUpgradePictures.contains(suffix)) {
        u.image = PictureKey(id, t, {suffix});
        break;
      }
    }
    start.upgrades.push_back(std::move(u));
  }
  if (!start.upgrades.empty()) result->paths.push_back(std::move(start));
  FigureUpgradePath basic;
  basic.kind = FigureUpgradePath::Kind::kBasic;
  const char* basic_pictures[kHalfBasicCount] = {"base1", "stat", "base2"};
  for (int i = 0; i < kHalfBasicCount; ++i) {
    basic.upgrades.push_back(upgrade(kHalfBasic + i, kFirstUpgradeBit + i, kCosts[i], basic_pictures[i]));
  }
  result->paths.push_back(std::move(basic));
  for (int p = 0; p < 2; ++p) {
    FigureUpgradePath path;
    path.kind = FigureUpgradePath::Kind::kPath;
    path.name = t.path_name[p];
    path.description = t.path_desc[p];
    // The path's two upgrades in the order of their numbers.
    std::vector<int> keys;
    for (const auto& [key, name] : t.half_name) {
      if (key >= kHalfPath + p * 10 && key < kHalfPath + p * 10 + 10) keys.push_back(key);
    }
    for (int j = 0; j < kHalfPathLength && j < int(keys.size()); ++j) {
      const std::string picture = "path" + std::to_string(p + 1) + "_" + std::to_string(j + 1);
      FigureUpgrade u = upgrade(keys[j], kHalfFirstPathBit + j, kCosts[kBasicCount + j], picture.c_str());
      u.alternate_bit = kFirstAlternateBit + j;
      path.upgrades.push_back(std::move(u));
    }
    result->paths.push_back(std::move(path));
  }
  FigureUpgradePath soul_gem;
  soul_gem.kind = FigureUpgradePath::Kind::kSoulGem;
  soul_gem.upgrades.push_back(upgrade(kHalfSoulGem, kHalfSoulGemBit, kCosts[kBasicCount + kPathLength], "sg"));
  result->paths.push_back(std::move(soul_gem));
  return result;
}

std::shared_ptr<const FigureUpgrades> Build(uint16_t id, const CharacterText& t, int year) {
  if (!t.half_name.empty() && !t.name.contains(kSoulGem)) return BuildHalf(id, t);
  auto result = std::make_shared<FigureUpgrades>();
  auto text = [](const std::map<int, std::string>& m, int i) {
    auto it = m.find(i);
    return it == m.end() ? std::string() : it->second;
  };
  auto upgrade = [&](int i, int bit, uint32_t cost) {
    FigureUpgrade u;
    u.name = text(t.name, i);
    u.description = text(t.desc, i);
    u.bit = bit;
    u.cost = cost;
    u.image = PictureKey(id, t, PictureSuffixes(i, year));
    return u;
  };
  FigureUpgradePath start;
  start.kind = FigureUpgradePath::Kind::kStart;
  for (int k = 0; k < 2; ++k) {
    if (t.start_name[k].empty()) continue;
    FigureUpgrade u;
    u.name = t.start_name[k];
    u.description = t.start_desc[k];
    u.image = PictureKey(id, t, {k == 0 ? "primary" : "secondary"});
    start.upgrades.push_back(std::move(u));
  }
  if (!start.upgrades.empty()) result->paths.push_back(std::move(start));
  FigureUpgradePath basic;
  basic.kind = FigureUpgradePath::Kind::kBasic;
  for (int i = 0; i < kBasicCount; ++i) basic.upgrades.push_back(upgrade(i, kFirstUpgradeBit + i, kCosts[i]));
  result->paths.push_back(std::move(basic));
  for (int p = 0; p < 2; ++p) {
    FigureUpgradePath path;
    path.kind = FigureUpgradePath::Kind::kPath;
    path.name = t.path_name[p];
    path.description = t.path_desc[p];
    for (int j = 0; j < kPathLength; ++j) {
      FigureUpgrade u = upgrade(kFirstPath + kPathLength * p + j, kFirstPathBit + j, kCosts[kBasicCount + j]);
      u.alternate_bit = kFirstAlternateBit + j;
      path.upgrades.push_back(std::move(u));
    }
    result->paths.push_back(std::move(path));
  }
  FigureUpgradePath soul_gem;
  soul_gem.kind = FigureUpgradePath::Kind::kSoulGem;
  soul_gem.upgrades.push_back(upgrade(kSoulGem, kSoulGemBit, kCosts[kBasicCount + kPathLength]));
  result->paths.push_back(std::move(soul_gem));
  // A later year's figure of an earlier Skylander has that year's Wow Pow.
  const int wow_pow = kSoulGem + year;
  if (year > FirstYear(id) && !text(t.name, wow_pow).empty()) {
    FigureUpgradePath path;
    path.kind = FigureUpgradePath::Kind::kWowPow;
    path.upgrades.push_back(upgrade(wow_pow, kWowPowBit, kCosts[kBasicCount + kPathLength + 1]));
    result->paths.push_back(std::move(path));
  }
  return result;
}

std::shared_ptr<const FigureUpgrades> Lookup(uint16_t id, uint16_t variant) {
  if (id >= kLegendaryIds && id < kLegendaryEnd) {
    id = id - kLegendaryIds;
  } else if (id == kDarkSpyro) {
    id = kSpyro;
  }
  const int year = (variant >> kYearShift) & 0xF;
  std::lock_guard<std::mutex> lock(g_mutex);
  const uint32_t key = (uint32_t(id) << 4) | uint32_t(year);
  if (auto it = g_upgrades.find(key); it != g_upgrades.end()) return it->second;
  auto character = g_characters.find(id);
  if (character == g_characters.end()) return nullptr;
  return g_upgrades[key] = Build(id, character->second, year);
}

// Reads one upgrade picture: the .arc entry is an IGZ with a single igImage2, upright.
std::shared_ptr<const FigureImage> ReadPicture(const std::filesystem::path& arc_path, size_t entry) {
  auto arc = alchemy::Iga11Archive::Open(arc_path);
  if (!arc) return nullptr;
  auto igz = ParseEntry(*arc, entry);
  if (!igz) return nullptr;
  const auto images = igz->Images();
  if (images.empty()) return nullptr;
  auto rgba = igz->Decode(images.front());
  if (!rgba) return nullptr;
  alchemy::FlipVertically(*rgba);  // stored upside down
  auto out = std::make_shared<FigureImage>();
  out->width = rgba->width;
  out->height = rgba->height;
  out->rgba = std::move(rgba->pixels);
  return out;
}

// Reads the requested pictures one after the other, and ends when there are none left.
void PictureWorker() {
  for (;;) {
    std::string key;
    std::filesystem::path arc;
    size_t entry = 0;
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      if (g_queue.empty()) {
        g_worker = false;
        return;
      }
      key = std::move(g_queue.front());
      g_queue.pop_front();
      // "upgrade:<id>:<suffix>"
      const size_t colon = key.find(':', kKeyPrefix.size());
      if (colon == std::string::npos) continue;
      const uint16_t id = uint16_t(std::stoi(key.substr(kKeyPrefix.size(), colon - kKeyPrefix.size())));
      auto character = g_characters.find(id);
      if (character == g_characters.end()) continue;
      auto picture = character->second.pictures.find(key.substr(colon + 1));
      if (picture == character->second.pictures.end()) continue;
      arc = character->second.arc;
      entry = picture->second;
    }
    auto image = ReadPicture(arc, entry);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_images[key] = std::move(image);
  }
}

}  // namespace

void InstallToyUpgrades(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir, LanguagePackName()).detach();
  skylanders::SetFigureUpgradesProvider([](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
}

std::shared_ptr<const FigureImage> UpgradeImage(const std::string& key) {
  if (!key.starts_with(kKeyPrefix)) return nullptr;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (auto it = g_images.find(key); it != g_images.end()) return it->second;
  if (g_requested.insert(key).second) {
    g_queue.push_back(key);
    if (!g_worker) {
      g_worker = true;
      std::thread(PictureWorker).detach();
    }
  }
  return nullptr;
}

}  // namespace trapteamrecomp
