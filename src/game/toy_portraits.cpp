// Pictures of the Skylanders for the portal menu: the portraits the game's HUD shows.
//
// Every Skylander has a scene archive of its own, rom/character/NNN_<model>[_<deco>].bld (IGA
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

#include <algorithm>
#include <cctype>
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

std::mutex g_mutex;
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

// Calls `use` for every HUD portrait in the scene of `archive_path`, with its name.
template <typename Use>
int ReadPortraits(const std::filesystem::path& archive_path, Use use) {
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
    const std::string name = PortraitName(image.name);
    if (name.empty()) continue;
    if (auto rgba = igz->Decode(image)) {
      FlipVertically(*rgba);  // stored upside down
      use(name, ToSquareImage(std::move(*rgba)));
      ++count;
    }
  }
  return count;
}

void Load(std::filesystem::path rom) {
  const auto start = Clock::now();
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
    ReadPortraits(entry.path(), [&](const std::string& name, std::shared_ptr<const FigureImage> image) {
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

std::shared_ptr<const FigureImage> Lookup(uint16_t id, uint16_t variant) {
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
}

}  // namespace trapteamrecomp
