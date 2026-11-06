// Signal-processing components: repeator, thresholder, redirector, pulsor, memory cell, fast latch.
#include <cstdlib>
#include <algorithm>
#include <format>
#include "components.h"
#include "latchor.h"
#include "fusor_sum.h"  // PlayN (JD)
#include "dividor.h"  // 2026-11-05 (JD)
#include "routor.h"  // PlayB (JD t4343u): the Redirector became the Routor
#include "randor.h"  // 2026-10-31 (JD): Randor  // 2026-10-31: cLatchor (was cFastLatch)
#include "lithokraft/content/repeator.h"  // Cortana 2026-10-15 2215
#include "lithokraft/content/signal.h"
#include "lithokraft/content/pulse_rules.h"  // 2026-11-01 PlayK

namespace lithokraft::components {
namespace {

/// Back in -> front out, one-way. Re-boosts any input to a full 64 after a 2 / 4 / 8 / 16 / 32 / 64 tick delay
/// (2026-11-01 PlayM JD; was 4 / 8 / 16 / 32, before that 2 / 4 / 8 / 16).
/// Cortana 2026-10-15 2215 (JD D4): was 1..8; encoding + lazy legacy migration in content/repeator.h.
class cRepeator final : public cCompBehavior {
 public:
  int delayTicks(CellState s, int64_t, uint64_t) const override { return repeator::delayTicks(s); }  // 2026-11-04 (JD t4306u): its setting
  bool queuedOutput() const override { return false; }  // own 2..64 delay line in tick()
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    const int f = stFacing(st), idx = repeator::delayIndex(st), delay = repeator::kDelays[idx];
    // 2026-11-05 (JD #5): side lock - a powered Repeator whose OUTPUT faces our left or right
    // freezes current out + colour and ignores back-input until the lock drops (Minecraft-style;
    // only Repeators lock Repeators). muxSides/muxOn filled for Repeator role in signal.cpp.
    const int L = dirLeft(f), R = dirRight(f);
    const bool locked = ((in.muxSides & (1u << L)) != 0 || (in.muxSides & (1u << R)) != 0) && in.muxOn;
    uint64_t line = loadLine(m);  // bit i = input i ticks ago
    if (locked) {
      // Freeze out+colour; keep delay line feeding the frozen bit so unlock resumes after delay.
      const int out = (stOn(st) || m.out > 0) ? kMaxSignal : 0;
      const int col = repeator::colourOf(st);
      line = (line << 1) | (out > 0 ? 1u : 0u);
      storeLine(m, line);
      return {out, 0, repeator::withColour(repeator::encode(out, f, idx, out > 0), col)};
    }
    // PlayN (JD 11-05 Randor chain): a NEW non-zero input value is a fresh edge: one 0 bit in the line, so the
    // output dips for one tick and rises again (downstream clocks see a rising edge). LL_REP_FRESH=0 kill.
    static const bool kFresh = [] { const char* e = std::getenv("LL_REP_FRESH"); return !(e && e[0] == '0'); }();
    const int bv = std::max(in.back(f), int(in.switchIn));
    const bool fresh = kFresh && bv > 0 && m.lastIn > 0 && bv != m.lastIn;
    m.lastIn = uint8_t(std::min(bv, 255));
    line = (line << 1) | ((bv > 0 && !fresh) ? 1u : 0u);  // 2026-10-31: Switch direct
    storeLine(m, line);
    const int out = ((line >> (delay - 1)) & 1) ? kMaxSignal : 0;
    const int bch = in.ch[size_t(dirBack(f))] ? in.ch[size_t(dirBack(f))] : in.chainCh;  // 1 red, 2 blue, 0 none (keeps the last colour)
    const int col = bch ? bch : repeator::colourOf(st);  // 3 = purple (Switch-fed, passed down the chain)
    return {out, 0, repeator::withColour(repeator::encode(out, f, idx, out > 0), col)};
  }
  std::optional<CellState> use(CellState st, ComponentMemory& m) const override {  // cycle 2 -> 4 -> ... -> 64 -> 2
    // 2026-10-31 JD double-pulse fix: the line still holds bits that already passed the OLD
    // tap; read at a longer tap they would fire a second time. Keep only bits still in flight
    // (below the old tap) and, when the tap shrinks, emit any in-flight bits past the new tap next tick.
    const int idx = repeator::delayIndex(st), nidx = repeator::nextSetting(idx);
    const int oldD = repeator::kDelays[idx], newD = repeator::kDelays[nidx];
    uint64_t line = loadLine(m);
    line &= lowMask(oldD - 1);
    if (newD < oldD) { const uint64_t keep = lowMask(newD - 1); if (line & ~keep) line = (line & keep) | (uint64_t(1) << (newD - 2)); }
    storeLine(m, line);
    return repeator::withColour(repeator::encode(stValue(st), stFacing(st), nidx, stOn(st)), repeator::colourOf(st));
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | delay {} ticks (setting {}/{}) | out {} | facing {}", d.name, repeator::delayTicks(s), repeator::delayIndex(s) + 1,
                       repeator::kSettings, stValue(s), facingName(stFacing(s)));
  }

 private:
  static uint64_t lowMask(int bits) { return bits >= 64 ? ~uint64_t(0) : ((uint64_t(1) << bits) - 1u); }
  // 64-bit delay line in the saved ComponentMemory bytes (see repeator.h).
  static uint64_t loadLine(const ComponentMemory& m) {
    return uint64_t(m.history) | (uint64_t(m.timer) << 8) | (uint64_t(m.historyHi) << 16) | (uint64_t(m.prevIn) << 32) |
           (uint64_t(m.prevSide) << 40) | (uint64_t(m.prevClk) << 48) | (uint64_t(uint8_t(m.channel ^ 1u)) << 56);  // channel defaults to 1 (and old saves hold 1): stored XOR 1 so 1 = empty
  }
  static void storeLine(ComponentMemory& m, uint64_t line) {
    m.history = uint8_t(line); m.timer = uint8_t(line >> 8); m.historyHi = uint16_t(line >> 16);
    m.prevIn = uint8_t(line >> 32); m.prevSide = uint8_t(line >> 40); m.prevClk = uint8_t(line >> 48); m.channel = uint8_t(uint8_t(line >> 56) ^ 1u);
  }
};

/// Readout shared by components whose value is their output strength.
std::string describeOutput(const BlockDef& d, CellState s) {
  return std::format("{} | out {} | facing {}", d.name, stValue(s), facingName(stFacing(s)));
}

/// Back = input, left = LOW reference, right = HIGH reference; output = input when it passes.
/// One reference: threshold (in >= ref). Both: window comparator (refs sorted). None: pass-through.
/// 2026-10-25 (JD): subtract mode (aux bit 0; compare = 0, so existing cells / saves are unchanged):
/// out = max(0, back - max(left, right)), 0..64. Using the block toggles compare <-> subtract (the
/// same use-cycles-a-setting convention as the Repeator delay and the Redirector front).
/// 2026-10-25 (JD roadmap 7, comparator-clock port): read mode (aux bit 1, toggled by sneak + use;
/// off = 0, so existing cells / saves are unchanged). In read mode the back input is the stronger of
/// the back signal and the LEVEL of the block directly behind (ComponentInputs::level): a Timer
/// Buffer's count, a Catchor's caught value while its input holds the match, an Input Buffer's
/// wafer count. Minecraft's comparator reading a container, in both compare and subtract mode.
class cThresholdor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 1; }  // 2026-11-04 (JD t4304u)
  static constexpr int kSubtract = 1;  // aux bit 0
  static constexpr int kRead = 2;      // aux bit 1
  static bool subtract(CellState s) { return (stAux(s) & kSubtract) != 0; }
  static bool readMode(CellState s) { return (stAux(s) & kRead) != 0; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory&) const override {
    const int f = stFacing(st), keep = stAux(st) & kRead;
    int back = in.back(f);
    if (readMode(st)) back = std::max(back, in.level[dirBack(f)]);
    if (subtract(st)) {
      const int v = std::min(back, kMaxSignal);
      const int side = std::min(std::max(in.left(f), in.right(f)), kMaxSignal);
      const int out = std::max(0, v - side);
      return {out, 0, makeState(out, f, kSubtract | keep, out > 0)};
    }
    // SW-H2: pass-through outputs copy the input value — clamp it here (and at
    // the sim's sampling point) so a disk-loaded 65..127 wire value can never
    // propagate into an output. In-range values are untouched.
    const int v = std::min(back, kMaxSignal), lo = in.left(f), hi = in.right(f);
    const bool pass = (lo > 0 && hi > 0) ? (v >= std::min(lo, hi) && v <= std::max(lo, hi)) : v >= std::max(lo, hi);
    const int out = (pass && v > 0) ? v : 0;
    // 2026-10-25 (JD, CloudAtlas readout top): in compare mode the state value carries the BACK
    // input (what the 2-digit readout shows: e.g. the container level in read mode); the output
    // is mem.out (emitToward) and "on" = out > 0, so the signal behaviour is unchanged.
    return {out, 0, makeState(v, f, keep, out > 0)};
  }
  std::optional<CellState> use(CellState st, ComponentMemory&) const override {
    return CellState(makeState(stValue(st), stFacing(st), uint8_t(stAux(st) ^ kSubtract), stOn(st)));
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory&) const override {  // roadmap 7: read mode
    return CellState(makeState(stValue(st), stFacing(st), uint8_t(stAux(st) ^ kRead), stOn(st)));
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    if (subtract(s))
      return std::format("{} | mode subtract{} | out {} | facing {}", d.name, readMode(s) ? " + read behind" : "",
                         stValue(s), facingName(stFacing(s)));
    return std::format("{} | mode compare{} | in {} | {} | facing {}", d.name, readMode(s) ? " + read behind" : "",
                       stValue(s), stOn(s) ? "passing" : "blocked", facingName(stFacing(s)));
  }
};

/// 2026-10-25 (JD roadmap 7): Timer Buffer, the input buffer for comparator clocks (the hopper of
/// a Minecraft hopper / comparator clock). Holds a count 0..64 and counts it down at the world
/// tick rate (32 TPS): one step every 1 / 2 / 4 / 8 ticks. The front drives the count as an
/// analog signal, and a Thresholdor in read mode behind it reads it as a level (readLevel).
///   loop mode: at 0 it holds one step, then reloads, so it repeats forever; period = (reload + 1)
///              steps (e.g. reload 15, step 2 ticks = 32 ticks = exactly 1 s at 32 TPS).
///   once mode: it stops at 0 until repowered.
/// Power on the back (or a wire on top) holds it at the reload value; counting starts on release,
/// so a pulse restarts it (repowering). State: value = count, facing, aux bit 0 = once (0 = loop,
/// the default), aux bits 1-2 = step index (1 / 2 / 4 / 8 ticks), on = count > 0.
/// Memory (saved in SGNL v3, no format change): timer = reload value (0 = default 16),
/// history = step divider phase, prevIn = powered last tick.
/// Use cycles the step 1 -> 2 -> 4 -> 8 and then flips loop / once (8 settings, like the Repeator's
/// buttons); sneak + use steps the reload value 1..64 (wraps) and reloads.
class cTimerBuffer final : public cCompBehavior {
 public:
  static constexpr int kDefaultReload = 16;
  static constexpr int kSteps[4] = {1, 2, 4, 8};
  static bool once(CellState s) { return (stAux(s) & 1) != 0; }
  static int stepIndex(CellState s) { return (stAux(s) >> 1) & 3; }
  static int reload(const ComponentMemory& m) { return m.timer == 0 ? kDefaultReload : std::min<int>(m.timer, kMaxSignal); }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    const int f = stFacing(st), aux = stAux(st), step = kSteps[stepIndex(st)], r = reload(m);
    const bool powered = in.back(f) > 0 || in.top > 0;
    int count = stValue(st);
    if (powered) { count = r; m.history = 0; }  // held at the reload value; counting starts on release
    else if (count > 0 || !once(st)) {
      if (++m.history >= step) {
        m.history = 0;
        count = count > 0 ? count - 1 : r;  // loop mode: 0 lasts one step, then reload
      }
    }
    m.prevIn = powered;
    const int out = std::min(count, kMaxSignal);
    return {out, 0, makeState(out, f, aux, out > 0)};
  }
  bool quiescent(CellState s, const ComponentMemory& m) const override { return once(s) && stValue(s) == 0 && !m.prevIn; }
  int readLevel(CellState s, const ComponentMemory&) const override { return stValue(s); }
  std::optional<CellState> use(CellState st, ComponentMemory&) const override {
    int idx = stepIndex(st) + 1, aux = stAux(st) & 1;
    if (idx == 4) { idx = 0; aux ^= 1; }
    return makeState(stValue(st), stFacing(st), aux | (idx << 1), stOn(st));
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override {
    const int r = reload(m) % kMaxSignal + 1;
    m.timer = uint8_t(r); m.history = 0;
    return makeState(r, stFacing(st), stAux(st), true);
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | {} | count {} | step {} ticks | facing {}", d.name, once(s) ? "once (repower to restart)" : "loop",
                       stValue(s), kSteps[stepIndex(s)], facingName(stFacing(s)));
  }
};

/// Linked (default): strongest of back / left / right / wire on top, out the front, any colour.
/// Using it cycles the front N -> E -> S -> W.
/// Output is always colourless: it drives red and blue Trace alike, so a cRedirector transfers
/// between colours (red in, blue out and vice versa).
/// Split (aux bit 2): red and blue Trace inputs are routed independently. The facing bits are the
/// red lane's front, aux bits 0-1 the blue lane's; either lane can be switched off (state bits 13 /
/// 14). A colourless input (cCatchor) feeds both lanes. Using an adjacent red / blue trace switches
/// to split and cycles that colour's front; using the block then cycles the red front.
class cRedirector final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 4; }  // 2026-11-04 (JD t4304u)  // Multiplexor
  static bool split(CellState s) { return (stAux(s) & 4) != 0; }
  static int blueFront(CellState s) { return stAux(s) & 3; }
  // Lane off flags (split only; bits 13 / 14, free on a cRedirector): an off lane takes no input and
  // drives nothing, so a pixel's unused colour has nowhere to leak through the colourless output.
  static constexpr CellState kRedOff = 1u << 13, kBlueOff = 1u << 14;
  static constexpr CellState kMuxBlue = 1u << 15;
  static CellState keep(CellState st) { return CellState(st & (kRedOff | kBlueOff)); }
  // 2026-11-01 (JD): Multiplexor: no use / wire-use control (Repeator side only). LL_MUX_NOUSE=0: old.
  static bool muxNoUse() { static const bool v = [] { const char* e = std::getenv("LL_MUX_NOUSE"); return !(e && e[0] == '0'); }(); return v; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory&) const override {
    const int f = stFacing(st);
    // 2026-10-31 (JD mux): a Repeator feeding the left or right side selects the colour, level-held:
    // Repeator off = red mode (red inputs pass), on = blue mode (blue inputs pass). Plain Trace or any
    // other component on a side never switches it. Output is colourless out the front, as in linked.
    if (in.muxSides != 0) {
      int red = 0, blue = 0;
      for (int d = 0; d < 4; ++d) {
        if (d == f || (in.muxSides & (1u << d))) continue;
        if (in.ch[d] == 1 || in.ch[d] == 0) red = std::max(red, in.side[d]);
        if (in.ch[d] == 2 || in.ch[d] == 0) blue = std::max(blue, in.side[d]);
      }
      const int out = std::min(in.muxOn ? blue : red, kMaxSignal);
      // 2026-11-04 (CloudAtlas v2 art): bit 15 = blue selected (render only: multiplexor_*_sel_blue).
      return {out, 0, CellState(makeState(out, f, stAux(st), out > 0) | keep(st) | (in.muxOn ? kMuxBlue : 0))};
    }
    if (!split(st)) {
      const int out = std::min(std::max({in.back(f), in.left(f), in.right(f), in.top}), kMaxSignal);  // SW-H2 clamp
      return {out, 0, makeState(out, f, stAux(st), out > 0)};
    }
    const int bf = blueFront(st);
    int red = 0, blue = 0;
    for (int d = 0; d < 4; ++d) {
      // Neither live lane reads a front (its output would feed back through the colourless Trace);
      // a colourless input (cCatchor / component) feeds both lanes.
      if ((d == f && !(st & kRedOff)) || (d == bf && !(st & kBlueOff))) continue;
      if (in.ch[d] == 1 || in.ch[d] == 0) red = std::max(red, in.side[d]);
      if (in.ch[d] == 2 || in.ch[d] == 0) blue = std::max(blue, in.side[d]);
    }
    if (st & kRedOff) red = 0;
    if (st & kBlueOff) blue = 0;
    red = std::min(red, kMaxSignal); blue = std::min(blue, kMaxSignal);  // SW-H2 clamp (pass-through lanes)
    const int v = std::max(red, blue);
    return {red, blue, CellState(makeState(v, f, stAux(st), v > 0) | keep(st))};
  }
  int emitToward(int dir, CellState st, const ComponentMemory& m) const override {
    if (!split(st)) return dir == stFacing(st) ? m.out : 0;
    return std::max(dir == stFacing(st) ? int(m.out) : 0, dir == blueFront(st) ? int(m.out2) : 0);
  }
  std::optional<CellState> use(CellState st, ComponentMemory&) const override {
    if (muxNoUse()) return std::nullopt;
    return CellState(makeState(stValue(st), (stFacing(st) + 1) & 3, stAux(st), stOn(st)) | keep(st));
  }
  // Using an adjacent Trace cycles that colour's lane: N -> E -> S -> W -> off -> N.
  std::optional<CellState> useChannel(CellState st, int channel) const override {
    if (muxNoUse()) return std::nullopt;
    int f = stFacing(st), bf = split(st) ? blueFront(st) : stFacing(st);
    CellState off = split(st) ? keep(st) : 0;
    auto step = [&](int& front, CellState bit) {
      if (off & bit) { off &= CellState(~bit); front = 0; }
      else if (front == 3) off |= bit;
      else ++front;
    };
    if (channel == 1) step(f, kRedOff);
    else if (channel == 2) step(bf, kBlueOff);
    else return std::nullopt;
    return CellState(makeState(stValue(st), f, 4 | bf, stOn(st)) | off);
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    if (!split(s)) return describeOutput(d, s);
    return std::format("{} | split | red Trace -> {} | blue Trace -> {} | out {} (colourless)", d.name, (s & kRedOff) ? "off" : facingName(stFacing(s)),
                       (s & kBlueOff) ? "off" : facingName(blueFront(s)), stValue(s));
  }
};

/// Back = data, left/right = clock. A rising clock edge with data > 0 fires a 1-tick pulse of the
/// data strength that travels the connected wire run without decay.
class cPulsor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 1; }  // 2026-11-04 (JD t4304u)
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    const int f = stFacing(st), data = std::min(in.back(f), kMaxSignal), clock = std::max(in.left(f), in.right(f));  // SW-H2: data is passed through as the pulse strength
    // 2026-11-01 (JD): relay mode. With no clock on either side, a rising edge on the back (any strength,
    // 1 included) is ARMED (prevSide = its value) and re-emitted NEXT tick as a one-tick pulse of the same
    // value (no hold). The output comes from memory, so the same-tick settle cannot cascade a chain in one
    // tick: each Pulsor adds exactly one tick (1, 2, 3 ...). prevIn = last back value; bit 7 = this tick
    // was a relay fire (data <= 64 never sets it). Clocked mode unchanged (fires on the edge tick).
    const bool edge = clock == 0 && m.prevClk == 0 && data > 0 && (m.prevIn & 127) == 0;
    const int pending = m.prevSide <= kMaxSignal ? int(m.prevSide) : 0;
    m.prevSide = uint8_t(edge ? data : 0);
    const bool clockFire = clock > 0 && m.prevClk == 0 && data > 0;
    const bool relay = !clockFire && pending > 0;
    const bool fire = clockFire || relay;
    m.prevClk = uint8_t(clock);
    const bool lastRelay = (m.prevIn & 128) != 0;
    m.prevIn = uint8_t(data | (relay ? 128 : 0));
    m.timer = fire ? kPulsorAnimTicks : (m.timer ? m.timer - 1 : 0);
    // 2026-11-01 (JD, PlayK): fires on one tick, stays high for cPulseRules::holdTicks() ticks
    // (default 4) so it can hold a Multiplexor open for an exact window. The held value is the
    // published m.out; the countdown is the saved anim timer, so the save format is unchanged.
    // PlayB (Elfie F3 / JD t4328u double blink): hold on the Pulsor's OWN last fired value, not m.out
    // (m.out is the delayed published value since the 1-tick delay, so the hold re-fired: on, off, on, on).
    // m.history is unused by the Pulsor (saved byte, no save-format change).
    const bool held = !fire && !lastRelay && m.history > 0 && m.timer > kPulsorAnimTicks - lithokraft::cPulseRules::holdTicks();
    const int out = fire ? (relay ? pending : data) : (held ? int(m.history) : 0);
    m.history = uint8_t(out);
    ComponentStep step{out, 0, makeState(out, f, std::min<int>(m.timer, 7), m.timer > 0), out > 0};
    if (fire) step.sound = SoundCue::PulsorFire;
    return step;
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | {} | facing {}", d.name, stOn(s) ? "FIRING" : "idle", facingName(stFacing(s)));
  }
};

/// Back = write, front = stored value (continuous), sides = clear on a rising edge. 0 = empty.
class cMemor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 8; }  // PlayB (JD t4331u): 8 ticks (was 16)
  bool quiescent(CellState, const ComponentMemory&) const override { return true; }  // pure in inputs + edges
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    const int f = stFacing(st), v = std::min(in.back(f), kMaxSignal), side = std::max(in.left(f), in.right(f));  // SW-H2: v is stored and re-emitted
    int stored = stValue(st);
    if (side > 0 && m.prevSide == 0) stored = 0;
    if (v > 0 && v != m.prevIn) stored = v;  // a pulse or a rising change stores the new value
    m.prevIn = uint8_t(v);
    m.prevSide = uint8_t(side);
    return {stored, 0, makeState(stored, f, 0, stored > 0)};
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    const int v = stValue(s);
    if (!v) return std::format("{} | empty | facing {}", d.name, facingName(stFacing(s)));
    return std::format("{} | stored {} ('{}') | facing {}", d.name, v, sixbitChar(v - 1), facingName(stFacing(s)));
  }
};

/// cCatchor (per-pixel address decoder). Wiring: while unwired it auto-connects to the FIRST wire
/// (red or blue) that touches one of its sides; that side becomes its input and that colour the
/// one it catches. Output: left, right or centre (straight through) relative to the input, never
/// up or down. The tick its input becomes exactly its match value (steady or pulse) it sends the
/// caught value as a one-tick pulse out of the output side (a colour lamp shows it; a cRedirector
/// there takes it as its input). The line itself is never driven, so the signal carries on.
/// State: value = match (1-64, 0 = disarmed), facing = direction of travel (input is behind it),
/// aux bits 0-1 = output (0 centre, 1 left, 2 right, 3 = not wired yet), aux bit 2 = blue input.
/// Use cycles the output (centre -> left -> right); sneak + use steps the match value.
class cCatchor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 1; }  // 2026-11-04 (JD t4304u)
  static bool wired(CellState s) { return (stAux(s) & 3) != 3; }
  // Same inputs and nothing fired last tick: prevIn already equals the match result, out stays 0.
  bool quiescent(CellState s, const ComponentMemory& m) const override { return m.out == 0 && !stOn(s); }
  static int outDir(CellState s) {
    const int f = stFacing(s), m = stAux(s) & 3;
    return m == 1 ? (f + 3) & 3 : m == 2 ? (f + 1) & 3 : f;
  }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    int f = stFacing(st), aux = stAux(st);
    const int match = stValue(st);
    if ((aux & 3) == 3)
      for (int d = 0; d < 4; ++d)
        if (in.ch[d] == 1 || in.ch[d] == 2 || ((in.repSides >> d) & 1)) { f = (d + 2) & 3; aux = in.ch[d] == 2 ? 4 : 0; break; }
    int v = 0;
    if ((aux & 3) != 3) {
      const int inDir = (f + 2) & 3;
      // 2026-11-01 (JD, PlayK): a Repeator whose front faces the input side feeds it directly (any colour).
      if (in.ch[inDir] == ((aux & 4) ? 2 : 1) || ((in.repSides >> inDir) & 1)) v = std::min(in.side[inDir], kMaxSignal);  // SW-H2 clamp
    }
    const bool hit = match > 0 && v == match;
    const bool fire = hit && !m.prevIn;
    m.prevIn = hit;
    const int out = fire ? match : 0;  // the caught value
    return {out, 0, makeState(match, f, aux, fire)};
  }
  int emitToward(int dir, CellState st, const ComponentMemory& m) const override {
    return wired(st) && dir == outDir(st) ? m.out : 0;
  }
  /// 2026-10-25 (JD roadmap 7): readable by a Thresholdor in read mode: the caught (match) value
  /// for as long as its input holds the match (steady, not the one-tick pulse), else 0.
  int readLevel(CellState st, const ComponentMemory& m) const override { return wired(st) && m.prevIn ? stValue(st) : 0; }
  std::optional<CellState> use(CellState st, ComponentMemory& /*m*/) const override {
    // 2026-10-24 (JD 6:40 PM): RMB always cycles the output left -> right -> centre; an unwired
    // Catchor takes the centre output (bound to the colour bit it has).
    const int mode = wired(st) ? ((stAux(st) & 3) + 1) % 3 : 0;
    return makeState(stValue(st), stFacing(st), (stAux(st) & 4) | mode, false);
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory& /*m*/) const override {
    // 2026-10-24 (JD 6:40 PM): sneak / Alt + RMB steps the match number (1..64, wraps), wired or not.
    return makeState(stValue(st) % kMaxSignal + 1, stFacing(st), stAux(st), false);
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    if (!wired(s)) return std::format("{} | catches {} | not wired (touch a Trace to a side, sneak+use to set)", d.name, stValue(s));
    static constexpr const char* kOut[3] = {"centre", "left", "right"};
    return std::format("{} | catches {} on {} | in from {} | out {} ({})", d.name, stValue(s), (stAux(s) & 4) ? "blue" : "red",
                       facingName((stFacing(s) + 2) & 3), kOut[stAux(s) & 3], facingName(outDir(s)));
  }
};
}  // namespace


/// Wireless signal source: trace in, radio out. use() toggles input/beacon mode;
/// sneak+use cycles the radio channel. First trace touching a side binds the input colour.
/// Broadcasts (mem.out = strength, mem.out2 = colour); the sim publishes after the tick.
/// Never drives wire (emitToward = 0).
class cSignalTransmittor final : public cCompBehavior {
 public:
  static bool wired(CellState s) { return (stAux(s) & 3) != 3; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    int f = stFacing(st), aux = stAux(st);
    if ((aux & 3) == 3)
      for (int d = 0; d < 4; ++d)
        if (in.ch[d] == 1 || in.ch[d] == 2) { f = (d + 2) & 3; aux = in.ch[d] == 2 ? 4 : 0; break; }
    int v = 0, col = 0;
    if (m.beacon) {
      v = kMaxSignal;
      col = (aux & 4) ? 2 : 1;
    } else if ((aux & 3) != 3) {
      const int inDir = (f + 2) & 3;
      if (in.ch[inDir] == ((aux & 4) ? 2 : 1)) { v = std::min(in.side[inDir], kMaxSignal); col = in.ch[inDir]; }  // SW-H2: v is broadcast as the strength
    }
    const bool active = v > 0;
    return {v, col, makeState(stValue(st), f, aux, active)};
  }
  int emitToward(int /*dir*/, CellState /*st*/, const ComponentMemory& /*m*/) const override { return 0; }
  std::optional<CellState> use(CellState st, ComponentMemory& m) const override {
    m.beacon = !m.beacon;
    return st;
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override {
    const uint8_t max = radioChannelsUnlocked();
    m.channel = uint8_t(m.channel % max + 1);
    return st;
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    if (!wired(s)) return std::format("{} | not wired (touch a Trace to a side)", d.name);
    return std::format("{} | {} | {}", d.name, (stAux(s) & 4) ? "blue" : "red",
                       stOn(s) ? "broadcasting" : "idle");
  }
};

/// Wireless signal sink: radio in, trace out. The sim fills in.radioIn / in.radioCol from the
/// radio registry (1-tick latency). sneak+use cycles the listen channel. Outputs on the front,
/// on the received colour.
class cSignalReceivor final : public cCompBehavior {
 public:
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    const int v = std::min(in.radioIn, kMaxSignal);  // SW-H2: received strength is passed through to the wire
    const bool active = v > 0;
    return {v, int(in.radioCol), makeState(stValue(st), stFacing(st), stAux(st), active)};
  }
  int emitChannel(int /*dir*/, CellState /*st*/, const ComponentMemory& m) const override {
    return m.out2 ? m.out2 : 0;  // onto the received colour
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override {
    const uint8_t max = radioChannelsUnlocked();
    m.channel = uint8_t(m.channel % max + 1);
    return st;
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | {}", d.name, stOn(s) ? "receiving" : "idle");
  }
};


/// The cDish: a long-range receivor. The sim queries with kDishSensitivity, so it hears
/// far beyond a plain receivor. Listens on its set channel; channel 0 (the Daemon) is
/// monitored when the lore pipeline exists. Single block for v1; multiblock later.
class cDish final : public cCompBehavior {
 public:
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    const int v = std::min(in.radioIn, kMaxSignal);  // SW-H2: received strength is passed through to the wire
    const bool active = v > 0;
    return {v, int(in.radioCol), makeState(stValue(st), stFacing(st), stAux(st), active)};
  }
  int emitChannel(int /*dir*/, CellState /*st*/, const ComponentMemory& m) const override {
    return m.out2 ? m.out2 : 0;
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override {
    const uint8_t max = radioChannelsUnlocked();
    m.channel = uint8_t(m.channel % max + 1);
    return st;
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | {}", d.name, stOn(s) ? "receiving" : "listening");
  }
};


/// The cOverclockor: the beacon equivalent. Tick accelerator (tiers later) + ward mode.
/// Accepts items through its interface; the jammer item enables jamming mode: all radio
/// channels except its own are blocked within kJammerRadius. use toggles the jammer item
/// (v1 placeholder for the item UI); sneak+use cycles the jam channel.
class cOverclockor final : public cCompBehavior {
 public:
  ComponentStep tick(const ComponentInputs& /*in*/, CellState st, ComponentMemory& /*m*/) const override {
    return {0, 0, st};
  }
  std::optional<CellState> use(CellState st, ComponentMemory& m) const override {
    m.jammer = !m.jammer;
    return st;
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override {
    const uint8_t max = radioChannelsUnlocked();
    m.channel = uint8_t(m.channel % max + 1);
    return st;
  }
  std::string describe(const BlockDef& d, CellState /*s*/) const override {
    // Note: describe() can't see memory; the game HUD shows the jam state separately.
    return std::format("{} | use: install/remove jammer item, sneak+use: jam channel", d.name);
  }
};


/// cPortor (lithography port). Defines input or output for a chip face.
/// Simple by design: use toggles input/output. Wire color auto-detected from
/// adjacent Trace (like cCatchor). When the Lithographer prints a chip, each
/// cPortor maps to one face. 6 faces = 6 bits = 64 configurations.
/// State: aux bit 0 = output (1) or input (0), aux bit 2 = blue (like cCatchor).
class cPortor final : public cCompBehavior {
 public:
  bool quiescent(CellState, const ComponentMemory&) const override { return true; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    // cPortor is a passive doorway: input mode reads, output mode passes through.
    // The actual I/O happens at lithography time; in the build area it's just a marker.
    const bool isOutput = (stAux(st) & 1) != 0;
    if (!isOutput) {
      // Input: capture the signal value for the chip to use
      int v = 0;
      for (int d = 0; d < 4; ++d)
        if (in.ch[d] == 1 || in.ch[d] == 2) { v = std::min(in.side[d], kMaxSignal); break; }  // SW-H2 clamp
      m.out = v;
    }
    return {0, 0, st};
  }
  int emitToward(int, CellState, const ComponentMemory&) const override { return 0; }
  std::optional<CellState> use(CellState st, ComponentMemory&) const override {
    // Toggle input/output
    const int aux = stAux(st) ^ 1;
    return makeState(stValue(st), stFacing(st), aux, false);
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    const bool isOutput = (stAux(s) & 1) != 0;
    const bool blue = (stAux(s) & 4) != 0;
    return std::format("{} | {} | {} | use: toggle in/out", d.name,
                       isOutput ? "output" : "input", blue ? "blue" : "red");
  }
};


/// cFusor (circuit breaker). Trips when input signal exceeds its threshold (1-64).
/// When tripped, output drops to 0 until reset. Protects chips from overload.
/// State: value = trip threshold (1-64), aux bit 0 = tripped.
/// Use cycles threshold? No — sneak+use sets threshold (like cCatchor match).
/// Use toggles reset (untrip).
class cFusor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 2; }  // 2026-11-04 (JD t4304u)  // Breakor
  bool quiescent(CellState s, const ComponentMemory& m) const override {
    return (stAux(s) & 1) != 0 && !m.prevSide;  // tripped = quiescent, unless a reset Repeator is on (edge tracking)
  }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override {
    const int threshold = stValue(st) > 0 ? stValue(st) : 64;
    bool tripped = (stAux(st) & 1) != 0;
    // 2026-10-31 (JD auto-reset): input comes from the BACK only (any colour); the left / right sides
    // are reset-only. A Repeator feeding a side resets a tripped fuse on its rising edge (off -> on),
    // like use. Plain Trace on a side does nothing (the sim only reports Repeators in muxSides).
    const int f = stFacing(st);
    const bool rst = in.muxSides != 0 && in.muxOn;
    if (rst && !m.prevSide) tripped = false;
    m.prevSide = rst ? 1 : 0;
    const int v = std::min(in.back(f), kMaxSignal);  // SW-H2: v is passed through unless tripped
    bool newTripped = tripped;
    if (!tripped && v > threshold) newTripped = true;  // overload! trip
    const int out = newTripped ? 0 : v;  // pass through unless tripped
    const int aux = newTripped ? (stAux(st) | 1) : (stAux(st) & ~1);
    return {out, 0, makeState(stValue(st), stFacing(st), aux, false)};
  }
  int emitToward(int dir, CellState st, const ComponentMemory& m) const override {
    return ((stAux(st) & 1) == 0 && dir == stFacing(st)) ? m.out : 0;  // 2026-10-31: front only
  }
  std::optional<CellState> use(CellState st, ComponentMemory&) const override {
    // Reset the fuse (untrip)
    return makeState(stValue(st), stFacing(st), stAux(st) & ~1, false);
  }
  std::optional<CellState> useAlt(CellState st, ComponentMemory&) const override {
    // Set threshold (1-64, wraps)
    return makeState(stValue(st) % 64 + 1, stFacing(st), stAux(st), false);
  }
  std::string describe(const BlockDef& d, CellState s) const override {
    const bool tripped = (stAux(s) & 1) != 0;
    return std::format("{} | trips at {} | {} | use: reset, sneak+use: set threshold",
                       d.name, stValue(s) > 0 ? stValue(s) : 64,
                       tripped ? "TRIPPED" : "ok");
  }
};

void registerLogic(const Registrar& add) {
  add("repeator", std::make_unique<cRepeator>());
  add("thresholdor", std::make_unique<cThresholdor>());
  add("redirector", std::make_unique<cRedirector>());
  add("pulsor", std::make_unique<cPulsor>());
  add("memor", std::make_unique<cMemor>());
  add("catchor", std::make_unique<cCatchor>());
  add("signal_transmittor", std::make_unique<cSignalTransmittor>());
  add("signal_receivor", std::make_unique<cSignalReceivor>());
  add("dish", std::make_unique<cDish>());
  add("overclockor", std::make_unique<cOverclockor>());
  add("fast_latch", std::make_unique<cLatchor>());  // block id kept for saves
  add("portor", std::make_unique<cPortor>());
  add("fusor", std::make_unique<cFusor>());
  add("timer_buffer", std::make_unique<cTimerBuffer>());
  add("fusor_sum", std::make_unique<cFusorSum>());  // PlayN (JD)
  add("dividor", std::make_unique<cDividor>());  // 2026-11-05 (JD)
  add("randor", std::make_unique<cRandor>());
  add("routor", std::make_unique<cRoutor>());  // PlayB (JD t4343u): was redirector_select  // 2026-11-04 (JD t4302u)  // 2026-10-31 (JD): random range block  // 2026-10-25 roadmap 7
}

}  // namespace lithokraft::components
