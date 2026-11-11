// The game's frame time above 120 FPS (TrapTeamRecomp #25: at 165 FPS the whole game ran about 1.4 times
// too fast).
//
// sub_82291130 makes the frame time the game's update takes (the global float at 0x82898BCC, written
// there and by its caller sub_821E9788 every frame): the measured time of the frame (+152 of the object
// sub_821CF530 returns) kept between 0.5 and 3.0 times the nominal frame time (+148, 1/60 s; the
// factors are the floats at 0x820EB298 and 0x820EB29C). So every frame counted at least 1/120 s, and
// above 120 FPS the game ran FPS/120 times too fast. Found 2026-11-12: at 168 FPS the float 1/120
// (3C08888A) stood in many objects and on the main thread's stack; a write watch on the global named
// these two functions (sub_821E9788 <- sub_823A32E0 <- sub_82290EE0 <- the per-frame update).
//
// The hook sits on the compare with the lower bound (0x82291174, fcmpu cr6,f0,f13) and lowers it in step
// with the frame rate limit: as the game has it up to 120 FPS, one frame at the limit above that (the
// limit is the most frames a second the game can get). The 0.5 itself is left alone: other code loads
// it too.

#include <algorithm>
#include <atomic>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DECLARE(int32_t, frame_rate_limit);

void FrameTimeFloorHook(PPCRegister& f11, PPCRegister& f13) {
  constexpr int32_t kGameFloorRate = 120;  // the game's own lower bound: half of a 1/60 s frame
  const int32_t limit = REXCVAR_GET(frame_rate_limit);
  static std::atomic<int32_t> logged_limit{-1};
  if (limit <= kGameFloorRate) {
    if (logged_limit.exchange(limit) != limit) {
      REXLOG_INFO("Frame time: the game's own lower bound {:.5f} s (nominal frame {:.5f} s), frame rate limit {}",
                  f13.f64, f11.f64, limit);
    }
    return;
  }
  const double lower = f13.f64 * double(kGameFloorRate) / double(limit);
  if (logged_limit.exchange(limit) != limit) {
    REXLOG_INFO("Frame time: lower bound {:.5f} s instead of the game's {:.5f} s for the frame rate limit {} "
                "(TrapTeamRecomp #25)",
                lower, f13.f64, limit);
  }
  f13.f64 = lower;
}
