// Strength of the game's motion blur, as in Swap Force (SwapForceRecomp's post_effects_hook.cpp;
// Cemu's Wii U packs offer the same: motion blur off).
//
// The renderer sets it up each frame as Swap Force's does: sub_82304028 stores the motion blur
// exposure time into the motion blur attr (+144 of the object at +388 of its this): the level's
// _motionBlurExposureTime (environment +648, 0.35 in the first level), or another one when its
// override is off or on a camera cut. The hook at that store (config/default.toml, 0x8230427C,
// f13 = the value) scales it, so 0 is the game's own "no blur"; the pass still runs, so its colour
// grading stays.
//
// Found 2026-10-22 by comparing the instructions with Swap Force's sub_82BA3390 (the same attr
// offset +144); the field from the environment's reflection table (0x8211A5D0, offsets after it:
// _motionBlurCameraStrength +644, _motionBlurExposureTime +648). Bloom has no counterpart here yet
// (#5): Trap Team's bloom is per object, and the values copied for it don't change the picture.

#include <algorithm>

#include <rex/cvar.h>
#include <rex/ppc/context.h>

// Off by default as in Swap Force (the owner's choice, 2026-10-19): the blur is tuned for 30 FPS
// and mostly smears at higher frame rates.
REXCVAR_DEFINE_DOUBLE(motion_blur_strength, 0.0, "Graphics",
                      "Motion blur: 0 = off (default), 1 = the game's, up to 2");

// 0x8230427C: f13 = the exposure time stored into the motion blur attr next.
void MotionBlurStrengthHook(PPCRegister& f13) {
  const double scale = std::clamp(REXCVAR_GET(motion_blur_strength), 0.0, 2.0);
  if (scale == 1.0) return;
  f13.f64 = double(float(f13.f64 * scale));
}
