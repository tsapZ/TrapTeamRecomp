// Ultrawide: a wider field of view for the game's 3D camera when the window is wider than 16:9.
//
// SwapForceRecomp's hook (its src/hooks/ultrawide_hook.cpp has how the camera was found there):
// every frame the renderer sets up the scene camera, stores the display's aspect ratio (16:9) and
// the field of view, and builds the projection with the perspective builder (r3 = matrix, f1 =
// horizontal FOV, f2 = vertical FOV, f3 = aspect, f4 = near, f5 = far; radians; the horizontal FOV
// when f1 >= 0). The guest frame stays 16:9; with present_letterbox off (the app turns it off while
// this is active, unless set by hand) the presenter stretches it across the wider window, so the
// camera has to see that much more to the sides for the picture to keep its proportions ("Hor+"):
// the hook at the call widens the aspect and, if the call gives a horizontal FOV, that one too,
// keeping the vertical FOV. The HUD is drawn into the same 16:9 frame and would be stretched.
//
// The HUD (ultrawide_hud, on): the UI lays itself out on its screen object's width and height
// (+188 / +192, 1280 x 720 on a 16:9 video mode), set by sub_821DD170 from the display's size.
// The hook at its store (0x821DD2D4) sets the width to the height x the window's aspect (720 x
// 2560 / 1080 = 1707), as GiantsRecomp's does for its UI screen object: the HUD keeps its
// proportions and goes to the window's edges. Found 2026-10-21 from the code loading the 1280 /
// 720 floats (0x820EBA9C), after the renderer's 2D camera (sub_8230F840, the full-screen passes)
// turned out not to be the HUD's (#4). Checked by script in a 2560x1080 window: the HUD (Double
// Trouble's portrait and gauge at the left edge, the name 0.75 as wide as stretched), the
// dialogue box, the autosave notice, the portal prompt and the pause menu (its background covers
// the whole window).
//
// Found 2026-10-21: the builder sub_823B1AF0 in a dump of this game's code by Swap Force's
// (42 of its first 48 instructions the same); the calls with a probe, a hook at each of the
// builder's callers logging its arguments in play: 0x8230F784 and 0x8230F7F0 are the ones that run
// every frame with the display's 16:9 and a horizontal FOV that follows the camera (the vertical
// one passed as -1 degree, unset); the camera builds its projection twice a frame with the same
// arguments, and both are widened. The other callers draw other views.
//
// Movies: the logos and the opening movie are 16:9 Bink movies drawn into the 16:9 frame, which
// the presenter would stretch across the window like the 3D picture. While one plays the
// presenter keeps black bars at the sides instead (present_use_actual_aspect off and
// present_letterbox on just for that time, not saved). BinkOpen's header hook
// (intro_skip_hook.cpp) marks the start, the start of BinkClose (sub_821A5FE8, called only from the
// game's movie close sub_824A30F0, which then clears the movie's handle) the end.

#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

#include "ultrawide_hook.h"

#include <rex/cvar.h>
#include <rex/graphics/video_mode_util.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ui/flags.h>

REXCVAR_DECLARE(std::string, resolution);

REXCVAR_DEFINE_BOOL(ultrawide_fov, true, "Graphics",
                    "On a window wider than 16:9 (resolution), show more to the sides instead of "
                    "black bars: a wider field of view and the picture stretched across the "
                    "window");
REXCVAR_DEFINE_BOOL(ultrawide_hud, true, "Graphics",
                    "With ultrawide_fov on a wider window, the HUD and the menus at the window's "
                    "edges in their own proportions instead of stretched");

namespace {

// True while a movie plays with the presenter's bars turned on for it; the aspect setting it
// changed for that, to put back.
std::atomic<bool> g_movie_bars{false};
std::atomic<bool> g_movie_actual_aspect{true};

constexpr double kGameAspect = 16.0 / 9.0;

// The window's aspect ratio from `resolution`, or 0 if it isn't set.
double WindowAspect() {
  int32_t width = 0, height = 0;
  if (!rex::graphics::video_mode_util::TryParseResolutionPreset(REXCVAR_GET(resolution), width,
                                                                height) ||
      width <= 0 || height <= 0) {
    return 0;
  }
  return double(width) / double(height);
}

}  // namespace

namespace trapteamrecomp {

// Whether the picture should fill a wider window (the presenter then must not letterbox).
bool UltrawideActive() {
  return REXCVAR_GET(ultrawide_fov) && WindowAspect() > kGameAspect * 1.01;
}


void MovieOpened(uint32_t width, uint32_t height) {
  // Only when the picture fills the wider window (the app turned the bars off for ultrawide).
  if (!UltrawideActive() || REXCVAR_GET(present_letterbox) || g_movie_bars.exchange(true)) return;
  // The presenter takes the frame's aspect from the window's size (present_use_actual_aspect),
  // so the 16:9 frame counts as wide as the window: off for the movie, the console's 16:9 video
  // mode then gives the aspect and present_letterbox the bars.
  g_movie_actual_aspect = rex::cvar::Query<bool>("present_use_actual_aspect");
  rex::cvar::SetFlagByName("present_use_actual_aspect", "false");
  rex::cvar::SetFlagByName("present_letterbox", "true");
  REXLOG_INFO("Ultrawide: movie {}x{} with bars at the sides", width, height);
}

void MovieClosed() {
  if (!g_movie_bars.exchange(false)) return;
  rex::cvar::SetFlagByName("present_letterbox", "false");
  rex::cvar::SetFlagByName("present_use_actual_aspect", g_movie_actual_aspect ? "true" : "false");
}

}  // namespace trapteamrecomp

// At 0x8230F784 and 0x8230F7F0 (bl sub_823B1AF0): f1 = horizontal FOV (radians; negative when the
// vertical one is given), f2 = vertical FOV, f3 = aspect.
void UltrawideFovHook(PPCRegister& f1, PPCRegister& f2, PPCRegister& f3) {
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("Ultrawide: camera FOV h {} v {}, aspect {}", f1.f64, f2.f64, f3.f64);
  }
  if (!trapteamrecomp::UltrawideActive() || f3.f64 <= 0) return;
  const double wide = WindowAspect();
  if (wide <= f3.f64) return;
  const double scale = wide / f3.f64;
  if (f1.f64 >= 0) {
    // Same vertical FOV: tan(h'/2) = tan(h/2) * scale.
    f1.f64 = double(float(2.0 * std::atan(std::tan(f1.f64 * 0.5) * scale)));
  }
  f3.f64 = double(float(wide));
}

// At the start of BinkClose (sub_821A5FE8): the movie is over (or skipped).
void BinkCloseHook() {
  trapteamrecomp::MovieClosed();
}

// At 0x821DD2D4 (stfs f31,188(r31)) in sub_821DD170, the UI's screen set-up: f31 = the screen's
// width, f29 its height (1280 x 720 on a 16:9 video mode), about to be stored at +188 / +192.
void UltrawideHudHook(PPCRegister& f29, PPCRegister& f31) {
  static std::atomic<bool> logged{false};
  if (!REXCVAR_GET(ultrawide_hud) || !trapteamrecomp::UltrawideActive() || f29.f64 <= 0) {
    if (!logged.exchange(true)) REXLOG_INFO("Ultrawide: UI screen {} x {}", f31.f64, f29.f64);
    return;
  }
  const double width = double(float(f29.f64 * WindowAspect()));
  if (!logged.exchange(true)) {
    REXLOG_INFO("Ultrawide: UI screen {} x {} -> {} wide", f31.f64, f29.f64, width);
  }
  if (width > f31.f64) f31.f64 = width;
}
