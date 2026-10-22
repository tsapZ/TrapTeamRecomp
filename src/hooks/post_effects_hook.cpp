// Strength of the game's bloom and motion blur, as in Swap Force (SwapForceRecomp's
// post_effects_hook.cpp; Cemu's Wii U packs offer the same).
//
// Bloom: Trap Team keeps it in the level's environment settings, one object per zone (19 in the
// first level, 0x4F0 bytes apart), with _bloomLuminanceCutoff at +140 and the three amounts
// after it: _bloomInnerScale +144, _bloomOuterScale +148, _bloomDiffuseScale +152 (names from
// the reflection table at 0x8211A5D0). sub_8230E670 hands the environment it renders with (r29)
// to the shaders: it copies the four fields into the float4 at +16 of the object at +84 of its
// `this` (r11), the pixel shader constant the scene's draws read (c4019 in the first level's
// capture: 1.01, 0.5, 1.5, 0), each time from the environment afresh. The hook right after those
// stores (config/default.toml, 0x8230EBA4) scales the three amounts and leaves the cutoff, so 0
// takes the glow away and nothing compounds.
//
// Found 2026-10-22: the values copied into the renderer's CBloomData (sub_82304CF0, 2168(r3)) did
// not reach the draws (a RenderDoc capture with that cutoff forced to 0 still had 1.01 in 288
// draws); a scan of guest memory for 1.01, 0.5, 1.5 in a row found the zone objects, and writing
// all of them (cutoff 0, outer 8) visibly brightened the clouds and the floating island. The
// loads of +140...+152 in the generated code then led to this copy.
//
// Motion blur, as Swap Force's option (motion blur off by default there too).
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
// _motionBlurCameraStrength +644, _motionBlurExposureTime +648).

#include <algorithm>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_DOUBLE(bloom_strength, 1.0, "Graphics",
                      "Glow around bright areas: 0 = off, 1 = the game's, up to 3");

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

// 0x8230EBA4: r11 = the shader constants' object; +16 cutoff, +20 inner, +24 outer, +28 diffuse
// scale, just stored from the environment.
void BloomStrengthHook(PPCRegister& r11) {
  const double scale = std::clamp(REXCVAR_GET(bloom_strength), 0.0, 3.0);
  if (scale == 1.0 || !r11.u32) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  for (uint32_t offset : {20u, 24u, 28u}) {
    uint8_t* p = base + r11.u32 + offset;
    rex::memory::store_and_swap<float>(p, float(rex::memory::load_and_swap<float>(p) * scale));
  }
}
