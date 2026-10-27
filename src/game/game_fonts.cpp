// The game's own fonts for the portal menus, read from the owner's game files at start-up, like
// the pictures (nothing of them is in the repository).
//
// The text font is one igBitmapFont, "hdd:/tfb/content/fonts/360/font_001" (its name at +0x08), in
// the scene of permanent/bootstrap.bld (its level.bld, IGZ version 8). The same layout as Swap
// Force's and SuperChargers' fonts (SwapForceRecomp src/game/game_fonts.cpp) with the lists in
// fields of their own: +0x0C the font's size in pixels (113), +0x40 / +0x44 an igCharMetricsList
// each (176 and 18 glyphs, their igCharMetrics pointers inline from +0x18, the count at +0x08); an
// igCharMetrics has +0x08 the u16 code point, +0x0C the advance, +0x10 / +0x14 the cell's offset
// (the cells don't start at the line's top here: 'A' at y 19), +0x18 / +0x1C its size, +0x20 /
// +0x24 its top-left texture coordinates. Every cell has a padding of +0x48 (4.0) pixels around
// the letter on each side, which the advance includes: the game draws each letter's advance
// 8 pixels shorter, twice the padding (measured 2026-10-21 on the title screen's "Starten", the
// five letter distances fit 1.4 times the font's pixels at an advance 7.7 to 8.1 shorter; with the
// stored advance the menus' letters stood visibly wider apart than the game's). +0x30 / +0x34
// (-3, -4 in the title font) look like a horizontal and vertical spacing but are not what makes
// up the difference. The first list's page is the igImage2 "fonts/360/font_001.png" (1024x1024, 16 bits per texel, Xbox 360 tiled with its
// mip levels after it, the first byte the coverage); the second list (18 glyphs on
// "font_001_rgb.png": button symbols and repeats of the first list's) is left out. Its code points
// (& * < = > [ \ ] ^ _ ` { | } ~ and three Greek letters) stand for buttons in the game's texts,
// and the first list has a plain white circle of 84x84 pixels for most of them, the shape the
// symbol is drawn over: those are left out of the first list too, so the menus' own font draws
// them (seen 2026-10-22: every ">" of the settings menu came out as a white dot).
// The title font, the game's bold one for headings, is "font_002" in the scene of
// permanent/global.bld, laid out the same: size 110, padding 4, 158 glyphs on the 1024x664 page
// "fonts/360/font_002.png" (format 0x1CB93819 too). Read 2026-10-21 from this disc; the decoded
// pages show the glyphs upright, German's ä ö ü ß among them.

#include "game_fonts.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <rex/logging.h>

#include "alchemy/iga11_archive.h"
#include "alchemy/igz8_file.h"

namespace trapteamrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

using skylanders::GameFont;
using skylanders::GameFontGlyph;
using skylanders::GameFontPage;

constexpr char kBodyArchive[] = "bootstrap.bld";
constexpr char kBodyPage[] = "fonts/360/font_001.png";
constexpr char kTitleArchive[] = "global.bld";
constexpr char kTitlePage[] = "fonts/360/font_002.png";
constexpr char kScene[] = "level.bld";
// The page's format (its name's hash in the scene's EXID list), 16 bits per texel.
constexpr uint32_t kCoverageFormat = 0x1CB93819;

constexpr uint32_t kFontSizeField = 0x0C;
constexpr uint32_t kPaddingField = 0x48;  // 4.0: around every cell's letter, on each side
constexpr uint32_t kGlyphListField = 0x40;
constexpr uint32_t kSymbolListField = 0x44;  // the button symbols (see the top)
constexpr uint32_t kListCountField = 0x08;
constexpr uint32_t kListItems = 0x18;
constexpr uint32_t kCodepointField = 0x08;
constexpr uint32_t kAdvanceField = 0x0C;
constexpr uint32_t kOffsetXField = 0x10;
constexpr uint32_t kOffsetYField = 0x14;
constexpr uint32_t kWidthField = 0x18;
constexpr uint32_t kHeightField = 0x1C;
constexpr uint32_t kU0Field = 0x20;
constexpr uint32_t kV0Field = 0x24;

float F32(const alchemy::Igz8File& igz, uint64_t object, uint32_t field) {
  const uint32_t bits = igz.Field(object, field);
  float value;
  std::memcpy(&value, &bits, 4);
  return value;
}

// Xbox 360 2D tiling (XGAddress2DTiledOffset): the byte offset of texel (x, y) of a surface `pitch`
// texels wide with 2^log2_bytes bytes per texel.
uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitch, uint32_t log2_bytes) {
  pitch = (pitch + 31) & ~31u;
  const uint32_t macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (log2_bytes + 7);
  const uint32_t micro = ((x & 7) + ((y & 0xE) << 2)) << log2_bytes;
  const uint32_t off = macro + ((micro & ~0xFu) << 1) + (micro & 0xF) + ((y & 1) << 4);
  return ((off & ~0x1FFu) << 3) + ((y & 16) << 7) + ((off & 0x1C0) << 2) +
         (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (off & 0x3F);
}

// From a line's top to the baseline: the bottom of the H's ink.
float Ascent(const GameFont& font) {
  for (const GameFontGlyph& g : font.glyphs) {
    if (g.codepoint != 'H') continue;
    const GameFontPage& page = font.pages[g.page];
    for (int row = g.height - 1; row >= 0; --row) {
      for (int col = 0; col < g.width; ++col) {
        if (page.alpha[size_t(g.y + row) * page.width + g.x + col] >= 128) {
          return g.y_offset + row + 1;
        }
      }
    }
  }
  return font.size * 0.8f;
}

std::shared_ptr<const GameFont> ReadFont(const std::filesystem::path& archive_path,
                                         std::string_view page_name) {
  auto archive = alchemy::Iga11Archive::Open(archive_path);
  if (!archive) return nullptr;
  const auto scene = archive->Find(kScene);
  if (!scene) return nullptr;
  auto* a = archive.get();
  const size_t index = *scene;
  auto igz = alchemy::Igz8File::Parse(
      [a, index](uint64_t offset, size_t size, uint8_t* out) {
        return a->Read(index, offset, size, out);
      },
      archive->length(index));
  if (!igz) return nullptr;
  const auto fonts = igz->Objects("igBitmapFont");
  if (fonts.empty()) return nullptr;
  const uint64_t font_object = fonts.front();

  // The page: decoded from the scene's pixels, the first mip level.
  const auto images = igz->Images();
  const auto image = std::find_if(images.begin(), images.end(),
                                  [page_name](const auto& i) { return i.name == page_name; });
  if (image == images.end() || image->width <= 0 || image->height <= 0) return nullptr;
  const auto raw = igz->Raw(*image);
  const uint32_t rows = (uint32_t(image->height) + 31) & ~31u;
  if (!raw || raw->format_hash != kCoverageFormat || raw->size < uint32_t(image->width) * rows * 2) {
    return nullptr;
  }
  std::vector<uint8_t> pixels(raw->size);
  if (!igz->Read(raw->offset, pixels.size(), pixels.data())) return nullptr;
  auto font = std::make_shared<GameFont>();
  font->size = F32(*igz, font_object, kFontSizeField);
  GameFontPage& page = font->pages.emplace_back();
  page.width = image->width;
  page.height = image->height;
  page.alpha.resize(size_t(page.width) * page.height);
  for (int y = 0; y < page.height; ++y) {
    for (int x = 0; x < page.width; ++x) {
      page.alpha[size_t(y) * page.width + x] =
          pixels[TiledOffset2D(uint32_t(x), uint32_t(y), uint32_t(page.width), 1)];
    }
  }

  // The first glyph list, the one on that page.
  const auto list = igz->Pointer(igz->Field(font_object, kGlyphListField));
  if (!list) return nullptr;
  const float padding = F32(*igz, font_object, kPaddingField);
  const float shorter = std::isfinite(padding) && padding > 0 && padding < 32 ? 2 * padding : 0.0f;
  std::unordered_set<uint32_t> symbols;
  if (const auto symbol_list = igz->Pointer(igz->Field(font_object, kSymbolListField))) {
    const uint32_t symbol_count = igz->Field(*symbol_list, kListCountField);
    for (uint32_t k = 0; k < symbol_count && k < 4096; ++k) {
      if (const auto g = igz->Pointer(igz->Field(*symbol_list, kListItems + 4 * k))) {
        symbols.insert(igz->Field(*g, kCodepointField) >> 16);
      }
    }
  }
  const uint32_t count = igz->Field(*list, kListCountField);
  for (uint32_t k = 0; k < count && k < 4096; ++k) {
    const auto g = igz->Pointer(igz->Field(*list, kListItems + 4 * k));
    if (!g) continue;
    GameFontGlyph glyph;
    glyph.codepoint = igz->Field(*g, kCodepointField) >> 16;
    if (symbols.contains(glyph.codepoint)) continue;
    glyph.advance = F32(*igz, *g, kAdvanceField) - shorter;
    glyph.x_offset = F32(*igz, *g, kOffsetXField);
    glyph.y_offset = F32(*igz, *g, kOffsetYField);
    glyph.x = int(std::lround(F32(*igz, *g, kU0Field) * page.width));
    glyph.y = int(std::lround(F32(*igz, *g, kV0Field) * page.height));
    glyph.width = std::min(int(std::lround(F32(*igz, *g, kWidthField))), page.width - glyph.x);
    glyph.height = std::min(int(std::lround(F32(*igz, *g, kHeightField))), page.height - glyph.y);
    if (glyph.x < 0 || glyph.y < 0) continue;
    font->glyphs.push_back(glyph);
  }
  if (font->glyphs.empty() || font->size <= 0) return nullptr;
  font->ascent = Ascent(*font);
  return font;
}

}  // namespace

skylanders::GameFonts LoadGameFonts(const std::filesystem::path& rom_dir) {
  const auto start = std::chrono::steady_clock::now();
  skylanders::GameFonts fonts;
  fonts.body = ReadFont(rom_dir / "permanent" / kBodyArchive, kBodyPage);
  fonts.title = ReadFont(rom_dir / "permanent" / kTitleArchive, kTitlePage);
  REXLOG_INFO("Game fonts: title {} glyphs, body {} glyphs, in {} ms",
              fonts.title ? fonts.title->glyphs.size() : 0,
              fonts.body ? fonts.body->glyphs.size() : 0,
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start).count());
  return fonts;
}

}  // namespace trapteamrecomp
