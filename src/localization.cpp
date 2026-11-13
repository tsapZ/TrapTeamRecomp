#include "localization.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Defined in the SDK (xam_user.cpp); both XGetLanguage and ExGetXConfigSetting read this.
REXCVAR_DECLARE(uint32_t, user_language);
// Defined in the SDK (xam_locale.cpp): the console's country (XC_COUNTRY ids), which XGetLocale reads.
REXCVAR_DECLARE(uint32_t, user_country);

REXCVAR_DEFINE_STRING(language, "auto", "Game",
                      "Game text/voice language: auto (the Windows display language if the disc offers it, "
                      "else the disc's own), or one the disc offers: english, german, french, dutch, italian, "
                      "spanish, brazilian, mexican, swedish, norwegian, danish, finnish (each disc offers two "
                      "to four of them). A raw Xbox 360 language ID (a number) also works, for experiments.");

namespace trapteamrecomp {

namespace {

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Trap Team offers the languages of its disc's SKU (rom\alchemy.xml, <Tfb skuName>; the owner's European disc
// says EU1) and picks one of them from the console's language and locale (sub_821D29B8, found 2026-11-13 by its
// culture strings at 0x820EBE44.. after the language setting's Swedish stayed English on that disc):
//   NA      French -> fr-CA, else en-US
//   EU1     German -> de-DE, else en-GB
//   EU2     French -> fr-FR, the locale NL -> nl-NL, else fr-BE
//   EU3     Spanish -> es-ES, else it-IT
//   LATIN   Spanish -> es-MX, Portuguese -> pt-BR, else en-US
//   NORDIC  the locale NO -> nb-NO, DK -> da-DK, FI -> fi-FI, else sv-SE
// The disc holds text files for all twelve (item\3d_font_msg_<language>.bld), but another SKU's languages don't
// come up: the game started with the SKU NORDIC on an EU1 disc stayed on a black screen (2026-11-13). So the
// setting offers what the disc's SKU offers, each set the way the console would ask for it: the language, and
// for those picked by the locale the country behind that locale (SDK xam_locale.cpp: NL 74 -> 23, SE 90 -> 32,
// NO 75 -> 25, DK 25 -> 10, FI 32 -> 11).
struct Offer {
  const char* name;   // the language setting's value
  uint32_t language;  // XGetLanguage (XC_LANGUAGE)
  uint32_t country;   // XC_COUNTRY for XGetLocale; 0 leaves the country alone
};
struct Sku {
  const char* name;
  std::vector<Offer> offers;  // the SKU's default first
};

const std::vector<Sku>& Skus() {
  static const std::vector<Sku> kSkus = {
      {"NA", {{"english", 1, 0}, {"french", 4, 0}}},
      {"EU1", {{"english", 1, 0}, {"german", 3, 0}}},
      {"EU2", {{"french", 4, 0}, {"dutch", 1, 74}}},
      {"EU3", {{"italian", 6, 0}, {"spanish", 5, 0}}},
      {"LATIN", {{"english", 1, 0}, {"mexican", 5, 0}, {"brazilian", 9, 0}}},
      {"NORDIC", {{"swedish", 1, 90}, {"norwegian", 1, 75}, {"danish", 1, 25}, {"finnish", 1, 32}}},
  };
  return kSkus;
}

// A disc without a readable SKU (none so far): the five Xbox 360 language ids, as before.
const Sku& FallbackSku() {
  static const Sku kFallback = {
      "", {{"english", 1, 0}, {"german", 3, 0}, {"french", 4, 0}, {"spanish", 5, 0}, {"italian", 6, 0}}};
  return kFallback;
}

std::filesystem::path g_disc_root;

// The disc's SKU, read once from alchemy.xml's skuName = "...".
const Sku& DiscSku() {
  static const Sku* sku = [] {
    std::ifstream in(g_disc_root / "alchemy.xml", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const size_t key = text.find("skuName");
    const size_t open = key == std::string::npos ? key : text.find('"', key);
    const size_t close = open == std::string::npos ? open : text.find('"', open + 1);
    const std::string name = close == std::string::npos ? "" : text.substr(open + 1, close - open - 1);
    for (const Sku& s : Skus()) {
      if (name == s.name) {
        REXLOG_INFO("Language: the disc's SKU is {}", name);
        return &s;
      }
    }
    REXLOG_WARN("Language: no known SKU in the disc's alchemy.xml ('{}'); offering the five languages", name);
    return &FallbackSku();
  }();
  return *sku;
}

const Offer* Offered(const std::string& name) {
  for (const Offer& offer : DiscSku().offers) {
    if (name == offer.name) return &offer;
  }
  return nullptr;
}

// The Windows display language as a language setting's name ("" for one Trap Team has no text in).
std::string SystemLanguage() {
#ifdef _WIN32
  const LANGID id = GetUserDefaultUILanguage();
  switch (PRIMARYLANGID(id)) {
    case LANG_ENGLISH: return "english";
    case LANG_GERMAN: return "german";
    case LANG_FRENCH: return "french";
    case LANG_ITALIAN: return "italian";
    case LANG_DUTCH: return "dutch";
    case LANG_SWEDISH: return "swedish";
    case LANG_NORWEGIAN: return "norwegian";
    case LANG_DANISH: return "danish";
    case LANG_FINNISH: return "finnish";
    case LANG_PORTUGUESE: return "brazilian";
    // Spanish: Spain's on an EU3 disc, Mexico's on a LATIN one.
    case LANG_SPANISH: return Offered("mexican") ? "mexican" : "spanish";
    default: break;
  }
#endif
  return "";
}

}  // namespace

void SetDiscRoot(const std::filesystem::path& root) { g_disc_root = root; }

const std::vector<std::string>& DiscLanguages() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> list;
    for (const Offer& offer : DiscSku().offers) list.push_back(offer.name);
    return list;
  }();
  return names;
}

const char* LanguageLabel(const std::string& name) {
  if (name == "english") return "English";
  if (name == "german") return "Deutsch";
  if (name == "french") return "Fran\xC3\xA7" "ais";
  if (name == "dutch") return "Nederlands";
  if (name == "italian") return "Italiano";
  if (name == "spanish") return "Espa\xC3\xB1ol";
  if (name == "mexican") return "Espa\xC3\xB1ol (M\xC3\xA9xico)";
  if (name == "brazilian") return "Portugu\xC3\xAAs (Brasil)";
  if (name == "swedish") return "Svenska";
  if (name == "norwegian") return "Norsk";
  if (name == "danish") return "Dansk";
  if (name == "finnish") return "Suomi";
  return "?";
}

void ApplyLanguageSetting() {
  // A direct --user_language / user_language=<n> override (raw, for experimentation) always
  // wins: don't clobber it with whatever `language` defaults to (a real bug this had until now --
  // it unconditionally overwrote user_language, silently discarding a direct override).
  if (rex::cvar::HasNonDefaultValue("user_language")) {
    REXLOG_INFO("Language: user_language already set directly ({}); leaving it alone",
               REXCVAR_GET(user_language));
    return;
  }

  std::string setting = Lower(REXCVAR_GET(language));
  uint32_t numeric_id = 0;
  if (auto [ptr, ec] = std::from_chars(setting.data(), setting.data() + setting.size(), numeric_id);
      ec == std::errc() && ptr == setting.data() + setting.size()) {
    REXCVAR_SET(user_language, numeric_id);
    REXLOG_INFO("Language: raw id {}", numeric_id);
    return;
  }
  if (setting != "auto" && !Offered(setting)) {
    // A settings file from another disc, or a language this disc's SKU doesn't offer.
    REXLOG_WARN("Language: the disc doesn't offer {}; using auto", setting);
    setting = "auto";
  }
  if (setting == "auto") {
    setting = SystemLanguage();
    if (!Offered(setting)) setting = DiscSku().offers.front().name;
    REXLOG_INFO("Language: auto, from the Windows display language and the disc");
  }
  const Offer& offer = *Offered(setting);
  REXCVAR_SET(user_language, offer.language);
  // A country set directly (--user_country) stays.
  if (offer.country && !rex::cvar::HasNonDefaultValue("user_country")) {
    REXCVAR_SET(user_country, offer.country);
  }
  REXLOG_INFO("Language: {} (language {}, country {})", setting, offer.language, REXCVAR_GET(user_country));
}

}  // namespace trapteamrecomp
