// The game's controller picture for the settings menu's controls page, as SuperChargers' and Swap
// Force's (SuperChargersRecomp src/game/controller_picture.cpp, SwapForceRecomp
// src/game/game_art.cpp, CleanController).
//
// level/Title.arc (the title screen's archive, with the Controls screen) has
// ".../ui_main/sprites/controls/sky2/controls_360.png/0xbef27b70.png.igb.tex.igz", found 2026-10-22
// by listing the texture names of every archive on the disc: 955x331 DXT5, stored upside down like
// the portraits, a photo of the white pad with light blue callout lines from its buttons to the
// screen's labels and a column of A/B/X/Y symbols at the right. The lines are about
// (148, 202, 230), three pixels wide, with a dark drop shadow up to four pixels to their right or
// below them. To clean it: the line pixels (IsCallout below), two pixels around them and five in
// the shadow's direction are painted over from their neighbours, layer by layer from the outside
// in; then only the opaque part joined to the guide button is kept (the symbol column goes), with
// two pixels of its soft edge where the photo has one (not where a line was painted over: those
// would leave grey stubs where the lines left the pad), and the picture is cut to kCrop. The
// buttons' places were measured on the picture (the face buttons' colours, the dark grey of the
// sticks and the d-pad, the guide button's ring) and are given in the full picture's pixels.

#include "controller_picture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga11_archive.h"
#include "game/alchemy/igz8_file.h"
#include "game/game_text.h"
#include "game/xenos_dxt.h"

namespace trapteamrecomp {
namespace {

using alchemy::Iga11Archive;
using skylanders::FigureImage;

constexpr char kArtKey[] = "controller:xbox360";
constexpr char kArchive[] = "level/Title.arc";
constexpr std::string_view kTexture = "/controls/sky2/controls_360.png/";
constexpr int kWidth = 955, kHeight = 331;
// The pad's box in the picture, with a few pixels to spare.
constexpr int kCropX = 282, kCropY = 38, kCropW = 386, kCropH = 244;
constexpr int kLineGrow = 2;     // pixels painted over around each line pixel
constexpr int kShadow = 5;       // and to the right of and below it
constexpr int kFillPasses = 64;  // enough for the widest line
constexpr int kSeedX = 475, kSeedY = 162;  // the guide button

std::mutex g_mutex;
std::shared_ptr<const FigureImage> g_picture;

// The light blue lines; the X button's blue is darker (r below 80) and kept out besides.
bool IsCallout(const uint8_t* p) {
  const int r = p[0], g = p[1], b = p[2];
  return p[3] > 20 && b - r > 35 && g - r > 25 && b > 170 && r > 100;
}

// Grows `mask` by one pixel (4-neighbourhood) `n` times.
void Grow(std::vector<uint8_t>& mask, int w, int h, int n) {
  std::vector<uint8_t> next(mask.size());
  for (int k = 0; k < n; ++k) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        next[i] = mask[i] || (x > 0 && mask[i - 1]) || (x + 1 < w && mask[i + 1]) || (y > 0 && mask[i - w]) ||
                  (y + 1 < h && mask[i + w]);
      }
    }
    mask.swap(next);
  }
}

std::shared_ptr<FigureImage> Clean(const RgbaImage& in) {
  const int w = in.width, h = in.height;
  if (w != kWidth || h != kHeight || in.pixels.size() < size_t(w) * h * 4) return nullptr;
  // The X button's blue, so its letter's light edges don't count as lines.
  std::vector<uint8_t> x_button(size_t(w) * h);
  for (size_t i = 0; i < x_button.size(); ++i) {
    const uint8_t* p = &in.pixels[i * 4];
    x_button[i] = p[3] > 200 && p[0] < 80 && p[2] - p[0] > 100;
  }
  Grow(x_button, w, h, 6);
  std::vector<uint8_t> line(size_t(w) * h);
  for (size_t i = 0; i < line.size(); ++i) line[i] = !x_button[i] && IsCallout(&in.pixels[i * 4]);
  std::vector<uint8_t> unknown = line;
  Grow(unknown, w, h, kLineGrow);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!line[size_t(y) * w + x]) continue;
      for (int d = 1; d <= kShadow; ++d) {
        if (x + d < w) unknown[size_t(y) * w + x + d] = 1;
        if (y + d < h) unknown[size_t(y + d) * w + x] = 1;
      }
    }
  }
  const std::vector<uint8_t> painted = unknown;
  // Paint over: every pass fills the unknown pixels next to known ones with their average.
  std::vector<float> px(in.pixels.begin(), in.pixels.end());
  for (int pass = 0; pass < kFillPasses; ++pass) {
    std::vector<std::pair<size_t, std::array<float, 4>>> filled;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        if (!unknown[i]) continue;
        std::array<float, 4> sum{};
        int count = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if ((dx || dy) && nx >= 0 && ny >= 0 && nx < w && ny < h && !unknown[size_t(ny) * w + nx]) {
              for (int c = 0; c < 4; ++c) sum[c] += px[(size_t(ny) * w + nx) * 4 + c];
              ++count;
            }
          }
        }
        if (count) {
          for (float& v : sum) v /= float(count);
          filled.emplace_back(i, sum);
        }
      }
    }
    if (filled.empty()) break;
    for (const auto& [i, value] : filled) {
      std::copy(value.begin(), value.end(), &px[i * 4]);
      unknown[i] = 0;
    }
  }
  // Keep the opaque part joined to the guide button, with two pixels of its soft edge.
  std::vector<uint8_t> opaque(size_t(w) * h), core(size_t(w) * h);
  for (size_t i = 0; i < opaque.size(); ++i) opaque[i] = px[i * 4 + 3] > 100;
  std::vector<std::pair<int, int>> stack = {{kSeedX, kSeedY}};
  while (!stack.empty()) {
    const auto [x, y] = stack.back();
    stack.pop_back();
    if (x < 0 || y < 0 || x >= w || y >= h) continue;
    const size_t i = size_t(y) * w + x;
    if (core[i] || !opaque[i]) continue;
    core[i] = 1;
    stack.insert(stack.end(), {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}});
  }
  std::vector<uint8_t> keep = core;
  Grow(keep, w, h, 2);
  auto out = std::make_shared<FigureImage>();
  out->width = kCropW;
  out->height = kCropH;
  out->rgba.resize(size_t(kCropW) * kCropH * 4);
  for (int y = 0; y < kCropH; ++y) {
    for (int x = 0; x < kCropW; ++x) {
      const size_t i = size_t(kCropY + y) * w + kCropX + x;
      uint8_t* o = &out->rgba[(size_t(y) * kCropW + x) * 4];
      for (int c = 0; c < 4; ++c) o[c] = uint8_t(std::clamp(std::lround(px[i * 4 + c]), 0L, 255L));
      if (!keep[i] || (!core[i] && painted[i])) o[3] = 0;
    }
  }
  return out;
}

std::shared_ptr<FigureImage> Read(const std::filesystem::path& rom_dir) {
  auto archive = Iga11Archive::Open(rom_dir / kArchive);
  if (!archive) return nullptr;
  for (size_t i = 0; i < archive->size(); ++i) {
    if (archive->name(i).find(kTexture) == std::string::npos) continue;
    auto igz = ParseEntry(*archive, i);
    if (!igz) return nullptr;
    for (const auto& image : igz->Images()) {
      if (auto rgba = igz->Decode(image)) {
        FlipVertically(*rgba);  // stored upside down
        return Clean(*rgba);
      }
    }
    return nullptr;
  }
  return nullptr;
}

}  // namespace

void InstallControllerPicture(const std::filesystem::path& rom_dir) {
  std::thread([rom_dir] {
    auto picture = Read(rom_dir);
    if (!picture) {
      REXLOG_WARN("Controller picture: not found in {}", (rom_dir / kArchive).string());
      return;
    }
    REXLOG_INFO("Controller picture: {}x{} from the Controls screen", picture->width, picture->height);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_picture = std::move(picture);
  }).detach();
}

std::shared_ptr<const FigureImage> ControllerPictureImage(const std::string& key) {
  if (key != kArtKey) return nullptr;
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_picture;
}

skylanders::ControllerPicture ControllerPictureLayout() {
  // In the full picture's pixels (see the top).
  static const skylanders::ControllerSpot kSpots[] = {
      {"guide", 475, 162, 21}, {"back", 432, 162, 9},  {"start", 516, 162, 9},  {"y", 591, 133, 11},
      {"x", 558, 161, 11},     {"b", 621, 156, 11},    {"a", 588, 185, 11},     {"ls", 358, 172, 25},
      {"rs", 528, 225, 24},    {"up", 414, 198, 9},    {"down", 414, 236, 9},   {"left", 395, 217, 9},
      {"right", 433, 217, 9},  {"lb", 357, 87, 15},    {"rb", 592, 87, 15},
  };
  skylanders::ControllerPicture picture;
  picture.art_key = kArtKey;
  picture.width = kCropW;
  picture.height = kCropH;
  for (skylanders::ControllerSpot spot : kSpots) {
    spot.x -= kCropX;
    spot.y -= kCropY;
    picture.spots.push_back(spot);
  }
  return picture;
}

}  // namespace trapteamrecomp
