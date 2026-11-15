// Nemo View: the ASCII picture Little Nemo reads, built from the software renderer's own
// buffers (colour, 16-bit depth and the per-pixel object id). Integer math only, so the same
// view gives the same bytes on every machine and for every worker count.
//
// Pipeline per cell (cells tile the source exactly; no float cell sizes):
//   reduce   RGB sum, nearest depth code, majority object id (ties go to the lower id)
//   quantize integer luma Y = (77R + 150G + 29B + 128) >> 8, depth band from a baked table,
//            nearest colour in a fixed 16-colour palette (ties go to the lower index)
//   dither   4x4 Bayer on the cell grid, luma' = clamp(Y + (bayer - 8) * 2)
//   glyph    12-step ramp " .:-=+*o#%@&", thinned by distance (fog per band); in Objects mode
//            walls, doors, keys, lights, pits, entities and liquid get their own characters
//
// Rows are independent: buildRows() writes only its own rows, so callers may split the rows
// across workers with no shared writes and join at their own phase boundary. finish() hashes
// the canonical record after the join. The old render.ascii look (software_ascii) is untouched.
//
// What Nemo may see: only the camera's own depth-tested pixels. The object id is the depth-test
// winner per pixel, so nothing behind a wall can appear. The record carries no position.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace litholattice::nemoview {

inline constexpr int kDepthBands = 8;   ///< 0 touching .. 6 far, 7 sky / nothing drawn
inline constexpr int kSkyBand = 7;
inline constexpr int kRampSteps = 12;
inline constexpr const char* kRamp = " .:-=+*o#%@&";  ///< ramp.v1, space first
inline constexpr int kPaletteSize = 16;
inline constexpr int kMaxObjectId = 63;

/// Object ids written into the software renderer's id buffer (from the block -> object table the
/// game supplies). 0 = nothing drawn (sky). Values above 15 are allowed and count as Other.
enum class ObjectId : uint8_t {
  Sky = 0,
  Floor = 1,
  Wall = 2,
  Pit = 3,
  Light = 4,
  Translucent = 5,
  Key = 6,
  Door = 7,
  Exit = 8,
  Entity = 9,
  Liquid = 10,
  Other = 11,
};

/// Objects (default): object characters for walls, doors, keys and the rest, luma elsewhere.
/// PixelsAndDepth: pixels and depth only (no object ids), kept for the crutch comparison.
enum class ViewMode : uint8_t { Objects = 0, PixelsAndDepth = 1 };

const char* viewModeName(ViewMode m);
bool viewModeFromName(const std::string& name, ViewMode& out);

/// Read-only view of one finished frame. Pixels are 0xAARRGGBB, rows top first.
struct SourceView {
  int width = 0, height = 0;
  const uint32_t* pixels = nullptr;
  const uint16_t* depth = nullptr;    ///< software depth codes (swsimd::depthQ); nullptr = all sky
  const uint8_t* objects = nullptr;   ///< per-pixel object id; nullptr = none
  int depthRemapMode = 0;             ///< LL_SW_DEPTH_REMAP in effect when the frame was drawn (0..2)
  /// Optional crop (the fovea). Zero size = the whole frame.
  int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
};

struct FrameConfig {
  int cols = 80, rows = 45;
  ViewMode mode = ViewMode::Objects;
};

struct AsciiFrame {
  int cols = 0, rows = 0;
  ViewMode mode = ViewMode::Objects;
  uint64_t tick = 0;                  ///< display only; not part of the hashed record
  std::string glyph;                  ///< cols*rows characters, row major, no newlines
  std::vector<uint8_t> fg;            ///< palette index 0..15 per cell
  std::vector<uint8_t> band;          ///< depth band 0..7 per cell
  std::vector<uint8_t> object;        ///< majority object id per cell (0 in PixelsAndDepth mode)
  std::array<uint8_t, 32> sha256{};   ///< SHA-256 of canonicalRecord(); set by finish()
};

// ---- primitives (exposed for tests) ----
inline constexpr int luma8(int r, int g, int b) { return (77 * r + 150 * g + 29 * b + 128) >> 8; }
int depthBand(uint16_t code, int remapMode);
int bayer4(int cx, int cy);                          ///< 0..15
int paletteIndex(int r, int g, int b);               ///< nearest by squared distance, lower index wins ties
uint32_t paletteColour(int index);                   ///< 0xFFRRGGBB
char objectGlyph(ObjectId id);                       ///< 0 when the id has no character of its own
int fogSteps(int band);                              ///< ramp steps removed at this band
const std::array<uint8_t, 32>& depthTableSha256();   ///< SHA-256 of the baked band table
std::string depthTableShaPrefix();                   ///< first 16 hex characters

// ---- building ----
/// Checks the geometry (exact tiling) and sizes `out`. False with `err` set on bad input.
bool prepare(const SourceView& src, const FrameConfig& cfg, AsciiFrame& out, std::string* err = nullptr);
/// Fills rows [rowBegin, rowEnd). Call after prepare(); safe to run on disjoint row ranges at once.
void buildRows(const SourceView& src, const FrameConfig& cfg, AsciiFrame& out, int rowBegin, int rowEnd);
/// Hashes the canonical record into out.sha256. Call once all rows are built.
void finish(AsciiFrame& out);
/// prepare + buildRows(all) + finish.
bool build(const SourceView& src, const FrameConfig& cfg, AsciiFrame& out, std::string* err = nullptr);

// ---- text forms ----
std::string glyphRows(const AsciiFrame& f);    ///< rows joined by '\n'
std::string fgRows(const AsciiFrame& f);       ///< one hex digit per cell
std::string bandRows(const AsciiFrame& f);     ///< one digit 0-7 per cell
std::string objectRows(const AsciiFrame& f);   ///< one base64url character per cell
/// nemo_ascii.v1|cols|rows|mode|ramp.v1|pal16.v1|lut|glyph|fg|depth|cls (all channels, always).
std::string canonicalRecord(const AsciiFrame& f);
std::string shaHex(const AsciiFrame& f);

/// Lossless run-length form: a run of 4 or more of one character becomes <char><count>.
/// Newlines are kept and never run-length coded. Input must contain no digits.
std::string rleEncode(const std::string& text);
bool rleDecode(const std::string& rle, std::string& out);

}  // namespace litholattice::nemoview
