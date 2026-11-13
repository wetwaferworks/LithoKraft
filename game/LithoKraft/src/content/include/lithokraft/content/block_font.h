// 3x5 block font (2026-11-12, JD t5295u: Component Test). The glyph table the test world has
// always used for its floor labels (moved here verbatim from test_world.cpp so the Component
// Test world shares it instead of keeping a drifted copy). 15 chars per glyph, top row first,
// '#' = block, '.' = empty.
#pragma once

namespace lithokraft {

/// The test world's 3x5 glyph for `c`, or nullptr (no glyph: the test world leaves a blank).
/// Unchanged set: A-I K-P R-U W-Y, 0-9 and '-'. test_world.cpp reads exactly this.
const char* blockFont3x5(char c);

/// blockFont3x5 plus the four letters its table never had (J, Q, V, Z). Those four are taken
/// verbatim from the other in-tree 3x5 font, cBoltLabWorld::glyph (same 3x5 grid, same
/// top-row-first layout), so labels such as RED VIA / SIGNAL RECEIVOR / SWIVELOR are complete.
/// Uppercase only; nullptr for anything else (a space is the caller's gap).
const char* blockFont3x5Labels(char c);

}  // namespace lithokraft
