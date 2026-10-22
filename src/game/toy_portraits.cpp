// Pictures of the Skylanders for the portal menu: the cards of the game's collection screens,
// else the portraits its HUD shows; and the menu's element symbols and gold coin (kArtArchive).
//
// The cards: rom/misc/UI_Collection_Champions.arc (the Skylanders, Minis and magic items) and
// rom/misc/ui_collection_traps.arc (the traps), IGA version 11, one IGZ version 8 texture per
// file, each named after its figure: ".../collection_images/skylander_toys/2014/
// 2014_skylanders_regular/519_12288_triggersnappy.png/0x3a50f90c.png.igb.tex.igz" is figure id
// 519, variant 12288 (the portal catalog's numbers), ".../traps/square/216_12291.png/..." a trap.
// 128x128 DXT5 with the card's frame and background, stored upside down like the portraits.
// Checked on this disc (2026-10-21) against the catalog of the four games up to this one: 345 of
// its 434 figures have a card of their own id and variant, all but 34 one of their id. Those 34
// are 30 of Swap Force's bottom halves (1000 + n), which show their top's card (2000 + n), and
// four unreleased test figures. Nitro Magna Charge's top is named "2010_9128_..." where the
// figure says 9218 (kVariantFixes). A figure without a card of its own variant (chase variants,
// the earlier games' sidekicks) gets the nearest card of its id: the same game first, then the
// same alternate deco, Series 2 and LightCore.
//
// The portraits, for ids without any card: every Skylander has a scene archive of its own, rom/character/NNN_<model>[_<deco>].bld (IGA
// version 11, iga11_archive.h). NNN is its figure id, the same as the portal catalog's
// (000_AirDragon is Whirlwind, 0; 2008_HootLoop_Top Hoot Loop's top, 2008; 476_Artichoke Food
// Fight). Its scene (the archive's level.bld, IGZ version 8, igz8_file.h) holds the HUD portraits
// of every figure of that character, as 214x107 DXT5 textures named
// "ui_sky4/hud/portraits/2014/256x128/target_2/<name>_portrait.png". The name says the figure's
// game by its prefix and the variant by the rest: "ssa_whirlwind" (Spyro's Adventure),
// "sg_whirlwind_series2" (Giants, Series 2), "sf_whirlwindhornblast" (Swap Force),
// "2014_whirlwind_eonselite" (this game's Eon's Elite), "ssa_bash_legendary", "sg_hex_lightcore",
// "2014_gillgrunt_repose" (this game's Series 2/3/4 re-poses), "2014_foodfight_dark" (from
// 476_Artichoke_Dark.bld), "sg_whirlwindpolar" (from 000_AirDragon_Alt.bld). Indexed on this disc
// (2026-10-21, all 519 scenes): 232 have portraits, from Spyro's Adventure to this game.
//
// Which one a figure gets, by the catalog variant (year << 12 | Series 2 << 11 | alternate deco
// << 10 | LightCore << 9 | deco id; year 0 is Spyro's Adventure, 3 this game): every portrait of
// the figure's id is scored, the game's prefix first, then legendary / other alternate deco,
// then Series 2 (Eon's Elite for deco 16 and re-pose for the others in this game) and LightCore,
// and the best one wins, so a figure without a picture of its own (most chase variants) gets the
// nearest. Spyro's Adventure's own variant ids: 400 + n is the Legendary of n, 28 Dark Spyro.
// The Minis' scenes are numbered apart from their figure ids (494_Mini_EyeSmall is Eye Small,
// 543) and also carry their big Skylander's portraits, so they are only read for their own
// "2014_mini..." pictures, by name (kMinis); Giants' and Spyro's Adventure's sidekicks show the
// Mini of the same character. The villains' scenes ("2014_ally_...") are left out. Magic items,
// traps and adventure packs have no HUD portraits.
// The portraits are stored upside down, like Giants' (seen 2026-10-21: Whirlwind head down), and
// are wider than tall, so they are put into a square, centred, for the menu's square tiles.

#include "toy_portraits.h"

#include "controller_picture.h"
#include "toy_hats.h"
#include "toy_upgrades.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga11_archive.h"
#include "game/alchemy/igz8_file.h"
#include "game/xenos_dxt.h"
#include "portal_rex/portal_menu.h"

namespace trapteamrecomp {
namespace {

using Clock = std::chrono::steady_clock;
using skylanders::FigureImage;

constexpr std::string_view kPortraitPath = "ui_sky4/hud/portraits/";
constexpr std::string_view kPortraitSuffix = "_portrait";
constexpr std::string_view kMiniPrefix = "2014_mini";
constexpr std::string_view kVillainPrefix = "2014_ally_";
constexpr char kCharacterDir[] = "character";
constexpr char kScene[] = "level.bld";
constexpr char kMiscDir[] = "misc";
constexpr const char* kCardArchives[] = {"UI_Collection_Champions.arc", "ui_collection_traps.arc"};
// The villain vault's pictures of each villain in a trap, for the portal speaker's "who is talking"
// overlay (skylanders-portal speaker_overlay.h): ".../collection_images/villains/captured/
// 10NN_villaincaptured.png/...", NN the villain id a trap stores (1-46), as art "villain:<id>".
constexpr char kVillainArchive[] = "ui_villainvault_stream.arc";
constexpr std::string_view kVillainCaptured = "/villains/captured/";

// The menus' symbols, from the scene of permanent/global.bld: the UI's element symbols
// "Levels/Includes/UI_Main/sprites/elementalIcons/<element>_64.png" (85x85 DXT5, the game's names
// are the menu's, Light and Dark among them; villain_64 is left out) and its gold coin
// "Levels/Includes/UI_Main/sprites/stats/coin.png" (85x85). Stored upside down like the cards
// (seen 2026-10-21: the flame and the leaves pointing down, the coin's S mirrored).
constexpr char kPermanentDir[] = "permanent";
constexpr char kArtArchive[] = "global.bld";
constexpr std::string_view kElementPath = "levels/includes/ui_main/sprites/elementalicons/";
constexpr std::string_view kElementSuffix = "_64.png";
constexpr std::string_view kElements[] = {"air",   "dark",  "earth", "fire",   "life",
                                          "light", "magic", "tech",  "undead", "water"};
constexpr std::string_view kGoldPicture = "levels/includes/ui_main/sprites/stats/coin.png";

// Swap Force's halves: bottoms 1000 + n, tops 2000 + n.
constexpr uint16_t kSwapBottoms = 1000;
constexpr uint16_t kSwapTops = 2000;

// A card whose file name has another variant than its figure.
struct VariantFix {
  uint16_t id;
  uint16_t named;
  uint16_t variant;
};
constexpr VariantFix kVariantFixes[] = {{2010, 9128, 9218}};  // Nitro Magna Charge (top)

// Variant bits of the portal catalog (portal/figure_catalog.h).
constexpr uint16_t kSeries2 = 0x800;
constexpr uint16_t kAlternateDeco = 0x400;
constexpr uint16_t kLightCore = 0x200;
constexpr uint16_t kDecoIdMask = 0x1FF;
constexpr uint16_t kLegendaryDeco = 3;
constexpr uint16_t kEonsEliteDeco = 16;
constexpr int kYearShift = 12;
constexpr int kThisGame = 3;
// Spyro's Adventure's own variant ids.
constexpr uint16_t kLegendaryIds = 400;
constexpr uint16_t kLegendaryIdsEnd = 450;  // this game's Skylanders start at 450
constexpr uint16_t kDarkSpyro = 28;
constexpr uint16_t kSpyro = 16;

// The games' prefixes, by the catalog's year.
constexpr std::string_view kPrefixes[] = {"ssa_", "sg_", "sf_", "2014_"};

// The Minis (and the earlier games' sidekicks of the same ids) by catalog id, with the picture of
// the alternate deco where there is one (Eggsellent Weeruptor, Power Punch Pet-Vac, Gnarly
// Barkley).
struct MiniPicture {
  uint16_t id;
  const char* name;
  const char* alternate;
};
constexpr MiniPicture kMinis[] = {
    {502, "2014_minibash", nullptr},          {503, "2014_minispyro", nullptr},
    {504, "2014_minihex", nullptr},           {505, "2014_miniterrafin", nullptr},
    {506, "2014_miniwhirlwind", nullptr},     {507, "2014_minieruptor", "2014_minieruptoreaster"},
    {508, "2014_minijetvac", "2014_minijetvacalt"}, {509, "2014_minifryno", nullptr},
    {510, "2014_minidrobot", nullptr},        {514, "2014_minigillgrunt", nullptr},
    {519, "2014_minitriggerhappy", nullptr},  {526, "2014_ministealthelf", nullptr},
    {540, "2014_minitreerex", "2014_minitreerexalt"}, {541, "2014_minithumpback", nullptr},
    {542, "2014_minininjinni", nullptr},      {543, "2014_minieyebrawl", nullptr},
};

struct Portrait {
  std::string deco;  // the archive's suffix in lower case: "", "legendary", "alt", "dark", ...
  std::string name;  // the picture's name in lower case, without "_portrait.png"
  std::shared_ptr<const FigureImage> image;
};

struct Card {
  uint16_t variant;
  std::shared_ptr<const FigureImage> image;
};

std::mutex g_mutex;
std::unordered_map<uint16_t, std::vector<Card>> g_cards;                  // by figure id
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_art;   // by menu key
std::unordered_map<uint16_t, std::vector<Portrait>> g_portraits;           // by figure id
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_minis;  // by picture name

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

bool Has(const std::string& s, std::string_view part) {
  return s.find(part) != std::string::npos;
}

// "ui_sky4/.../ssa_whirlwind_portrait.png" -> "ssa_whirlwind"; empty if it isn't a HUD portrait.
std::string PortraitName(const std::string& path) {
  const std::string lower = Lower(path);
  if (!Has(lower, kPortraitPath)) return {};
  std::string name = lower.substr(lower.find_last_of('/') + 1);
  if (name.ends_with(".png")) name.resize(name.size() - 4);
  if (!name.ends_with(kPortraitSuffix)) return {};
  name.resize(name.size() - kPortraitSuffix.size());
  return name;
}

// The picture centred in a transparent square, for the menu's square tiles.
std::shared_ptr<const FigureImage> ToSquareImage(RgbaImage&& image) {
  auto out = std::make_shared<FigureImage>();
  const int side = std::max(image.width, image.height);
  out->width = side;
  out->height = side;
  out->rgba.assign(size_t(side) * side * 4, 0);
  const int x0 = (side - image.width) / 2, y0 = (side - image.height) / 2;
  for (int y = 0; y < image.height; ++y) {
    std::copy_n(image.pixels.data() + size_t(y) * image.width * 4, size_t(image.width) * 4,
                out->rgba.data() + (size_t(y0 + y) * side + x0) * 4);
  }
  return out;
}

// Calls `use(name, picture)` for every picture in the scene of `archive_path` that `name_of`
// names (from its source path; empty to skip it), the picture upright and square.
template <typename NameOf, typename Use>
int ReadScenePictures(const std::filesystem::path& archive_path, NameOf name_of, Use use) {
  auto archive = alchemy::Iga11Archive::Open(archive_path);
  if (!archive) return 0;
  const auto scene = archive->Find(kScene);
  if (!scene) return 0;
  auto* a = archive.get();
  const size_t index = *scene;
  auto igz = alchemy::Igz8File::Parse(
      [a, index](uint64_t offset, size_t size, uint8_t* out) {
        return a->Read(index, offset, size, out);
      },
      archive->length(index));
  if (!igz) return 0;
  int count = 0;
  for (const auto& image : igz->Images()) {
    const std::string name = name_of(image.name);
    if (name.empty()) continue;
    if (auto rgba = igz->Decode(image)) {
      FlipVertically(*rgba);  // stored upside down
      use(name, ToSquareImage(std::move(*rgba)));
      ++count;
    }
  }
  return count;
}

// The menu's key for one of its symbols ("element:fire", "ui:gold"); empty for other pictures.
std::string ArtKey(const std::string& path) {
  const std::string lower = Lower(path);
  if (lower == kGoldPicture) return "ui:gold";
  if (!lower.starts_with(kElementPath) || !lower.ends_with(kElementSuffix)) return {};
  const std::string element = lower.substr(
      kElementPath.size(), lower.size() - kElementPath.size() - kElementSuffix.size());
  for (std::string_view known : kElements) {
    if (element == known) return "element:" + element;
  }
  return {};
}

// ".../519_12288_triggersnappy.png/0x3a50f90c.png.igb.tex.igz" -> 519, 12288; false for a file
// whose folder isn't named <id>_<variant>[_<name>].png (square_highlight.png).
bool CardFigure(std::string_view path, uint16_t* id, uint16_t* variant) {
  const size_t end = path.rfind('/');
  if (end == std::string_view::npos || end == 0) return false;
  const size_t begin = path.rfind('/', end - 1) + 1;  // 0 if there is no other '/'
  const char* p = path.data() + begin;
  const char* last = path.data() + end;
  unsigned a = 0, b = 0;
  auto r = std::from_chars(p, last, a);
  if (r.ec != std::errc() || r.ptr == last || *r.ptr != '_') return false;
  const char* q = r.ptr + 1;
  r = std::from_chars(q, last, b);
  if (r.ec != std::errc() || r.ptr == q || r.ptr == last || (*r.ptr != '_' && *r.ptr != '.') ||
      a > 0xFFFF || b > 0xFFFF) {
    return false;
  }
  *id = uint16_t(a);
  *variant = uint16_t(b);
  for (const VariantFix& fix : kVariantFixes) {
    if (fix.id == *id && fix.named == *variant) *variant = fix.variant;
  }
  return true;
}

// Reads every card of a collection archive.
int ReadCards(const std::filesystem::path& archive_path) {
  auto archive = alchemy::Iga11Archive::Open(archive_path);
  if (!archive) return 0;
  auto* a = archive.get();
  int count = 0;
  for (size_t index = 0; index < archive->size(); ++index) {
    uint16_t id = 0, variant = 0;
    if (!CardFigure(archive->name(index), &id, &variant)) continue;
    auto igz = alchemy::Igz8File::Parse(
        [a, index](uint64_t offset, size_t size, uint8_t* out) {
          return a->Read(index, offset, size, out);
        },
        archive->length(index));
    if (!igz) continue;
    const auto images = igz->Images();
    if (images.empty()) continue;
    auto rgba = igz->Decode(images[0]);
    if (!rgba) continue;
    FlipVertically(*rgba);  // stored upside down
    auto image = ToSquareImage(std::move(*rgba));
    std::lock_guard<std::mutex> lock(g_mutex);
    g_cards[id].push_back(Card{variant, std::move(image)});
    ++count;
  }
  return count;
}

// Reads the villain vault's captured-villain pictures (kVillainArchive) into g_art.
int ReadVillains(const std::filesystem::path& archive_path) {
  auto archive = alchemy::Iga11Archive::Open(archive_path);
  if (!archive) return 0;
  auto* a = archive.get();
  int count = 0;
  for (size_t index = 0; index < archive->size(); ++index) {
    const std::string name = Lower(archive->name(index));
    const size_t at = name.find(kVillainCaptured);
    if (at == std::string::npos) continue;
    const size_t digits = at + kVillainCaptured.size();
    if (name.size() < digits + 4 || !std::isdigit((unsigned char)name[digits])) continue;
    const int number = std::stoi(name.substr(digits, 4));
    if (number < 1001 || number > 1099) continue;
    auto igz = alchemy::Igz8File::Parse(
        [a, index](uint64_t offset, size_t size, uint8_t* out) {
          return a->Read(index, offset, size, out);
        },
        archive->length(index));
    if (!igz) continue;
    const auto images = igz->Images();
    if (images.empty()) continue;
    auto rgba = igz->Decode(images[0]);
    if (!rgba) continue;
    FlipVertically(*rgba);  // stored upside down like the cards
    auto image = ToSquareImage(std::move(*rgba));
    std::lock_guard<std::mutex> lock(g_mutex);
    g_art["villain:" + std::to_string(number - 1000)] = std::move(image);
    ++count;
  }
  return count;
}

void Load(std::filesystem::path rom) {
  auto start = Clock::now();
  int cards = 0;
  for (const char* name : kCardArchives) cards += ReadCards(rom / kMiscDir / name);
  const int villains = ReadVillains(rom / kMiscDir / kVillainArchive);
  REXLOG_INFO("Toy pictures: {} villains in traps", villains);
  const int symbols = ReadScenePictures(
      rom / kPermanentDir / kArtArchive, ArtKey,
      [](const std::string& key, std::shared_ptr<const FigureImage> image) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_art[key] = std::move(image);
      });
  REXLOG_INFO("Toy pictures: {} collection cards and {} symbols in {} ms", cards, symbols,
              std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
  start = Clock::now();
  std::error_code ec;
  int portraits = 0, minis = 0, archives = 0;
  for (const auto& entry : std::filesystem::directory_iterator(rom / kCharacterDir, ec)) {
    const std::string file = entry.path().filename().string();
    if (entry.path().extension().string() != ".bld" || file.empty() ||
        !std::isdigit((unsigned char)file[0])) {
      continue;
    }
    // NNN_<model>[_<deco>].bld, NNN of any length.
    size_t digits = 0;
    while (digits < file.size() && std::isdigit((unsigned char)file[digits])) ++digits;
    if (digits == file.size() || file[digits] != '_') continue;
    const uint16_t id = uint16_t(std::stoi(file.substr(0, digits)));
    const std::string stem = Lower(entry.path().stem().string());
    const bool mini = Has(stem, "_mini_");
    // The deco: what follows the model name, e.g. "_alt", "_legendary", "_dark", "_nitro".
    std::string deco;
    for (const char* suffix : {"legendary", "alt2014", "alt", "red", "dark", "nitro", "jade",
                               "enchanted", "springtime", "easter", "jolly", "repose2014"}) {
      if (stem.ends_with(std::string("_") + suffix)) {
        deco = suffix;
        break;
      }
    }
    ++archives;
    ReadScenePictures(
        entry.path(), PortraitName,
        [&](const std::string& name, std::shared_ptr<const FigureImage> image) {
          std::lock_guard<std::mutex> lock(g_mutex);
          if (name.starts_with(kMiniPrefix)) {
            g_minis[name] = std::move(image);
            ++minis;
          } else if (!mini && !name.starts_with(kVillainPrefix)) {
            g_portraits[id].push_back(Portrait{deco, name, std::move(image)});
            ++portraits;
          }
        });
  }
  REXLOG_INFO("Toy portraits: {} portraits and {} Minis from {} archives in {} ms", portraits,
              minis, archives,
              std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
}

std::shared_ptr<const FigureImage> Mini(const std::string& name) {
  auto it = g_minis.find(name);
  return it == g_minis.end() ? nullptr : it->second;
}

// The card of the figure's own variant, else the nearest of its id; nullptr if its id has none.
std::shared_ptr<const FigureImage> CardFor(uint16_t id, uint16_t variant) {
  if (id >= kSwapBottoms && id < kSwapTops) id += kSwapTops - kSwapBottoms;  // a bottom: its top
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_cards.find(id);
  if (it == g_cards.end()) return nullptr;
  const Card* best = nullptr;
  int best_score = -1;
  for (const Card& card : it->second) {
    if (card.variant == variant) return card.image;
    int score = 0;
    if ((card.variant >> kYearShift) == (variant >> kYearShift)) score += 8;
    if ((card.variant & kAlternateDeco) == (variant & kAlternateDeco)) score += 4;
    if ((card.variant & kSeries2) == (variant & kSeries2)) score += 2;
    if ((card.variant & kLightCore) == (variant & kLightCore)) score += 1;
    if (score > best_score) {
      best = &card;
      best_score = score;
    }
  }
  return best ? best->image : nullptr;
}

std::shared_ptr<const FigureImage> Lookup(uint16_t id, uint16_t variant) {
  if (auto card = CardFor(id, variant)) return card;
  for (const MiniPicture& m : kMinis) {
    if (m.id != id) continue;
    std::lock_guard<std::mutex> lock(g_mutex);
    if ((variant & kAlternateDeco) && m.alternate) {
      if (auto image = Mini(m.alternate)) return image;
    }
    return Mini(m.name);
  }
  uint16_t base = id;
  // What the figure's deco wants: "legendary", "dark" or another alternate ("alt").
  std::string deco;
  if (id >= kLegendaryIds && id < kLegendaryIdsEnd) {
    base = id - kLegendaryIds;
    deco = "legendary";
  } else if (id == kDarkSpyro) {
    base = kSpyro;
    deco = "dark";
  } else if (variant & kAlternateDeco) {
    deco = (variant & kDecoIdMask) == kLegendaryDeco ? "legendary" : "alt";
  }
  const int year = std::min<int>(variant >> kYearShift, kThisGame);
  const std::string_view prefix = kPrefixes[year];
  const bool series2 = variant & kSeries2, lightcore = variant & kLightCore;
  const bool eons_elite = series2 && year == kThisGame && (variant & kDecoIdMask) == kEonsEliteDeco;
  const bool repose = series2 && year == kThisGame && !eons_elite;
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_portraits.find(base);
  if (it == g_portraits.end()) return nullptr;
  const Portrait* best = nullptr;
  int best_score = -1;
  for (const Portrait& p : it->second) {
    const bool p_legendary = p.deco == "legendary" || Has(p.name, "legendary");
    const bool p_dark = p.deco == "dark" || Has(p.name, "_dark");
    const bool p_alt = !p_legendary && (!p.deco.empty() || p_dark);
    int score = 0;
    if (p.name.starts_with(prefix)) score += 16;
    if (deco == "legendary" ? p_legendary
        : deco == "dark"    ? p_dark
        : deco == "alt"     ? p_alt
                            : !p_legendary && !p_alt) {
      score += 8;
    }
    if (eons_elite == Has(p.name, "eonselite") && repose == Has(p.name, "repose") &&
        (series2 && !eons_elite && !repose) == Has(p.name, "series2")) {
      score += 4;
    }
    if (Has(p.name, "lightcore") == lightcore) score += 2;
    if (score > best_score) {
      best = &p;
      best_score = score;
    }
  }
  return best ? best->image : nullptr;
}

}  // namespace

void InstallToyPortraits(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir).detach();
  skylanders::SetFigureImageProvider(
      [](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
  skylanders::SetArtImageProvider([](const std::string& key) -> std::shared_ptr<const FigureImage> {
    if (auto hat = HatImage(key)) return hat;  // "hat:<number>", "stat:<name>" (toy_hats.cpp)
    if (key.starts_with("upgrade:")) return UpgradeImage(key);  // toy_upgrades.cpp
    if (auto pad = ControllerPictureImage(key)) return pad;    // controller_picture.cpp
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_art.find(key);
    return it == g_art.end() ? nullptr : it->second;
  });
}

}  // namespace trapteamrecomp
