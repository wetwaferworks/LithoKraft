// Nemo View: integer ASCII frame builder (see nemo_view.h).
#include "litholattice/render/nemo_view.h"

#include <algorithm>
#include <cctype>

#include "litholattice/crypto/sha256.h"
#include "litholattice/render/nemo_view_depth_bands.h"

namespace litholattice::nemoview {
namespace {

// pal16.v1: the classic 16-colour VGA text palette.
constexpr uint32_t kPalette[kPaletteSize] = {
    0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0xC0C0C0,
    0x808080, 0x0000FF, 0x00FF00, 0x00FFFF, 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF};

constexpr int kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Ramp steps removed per band: near cells keep full density, far cells thin toward '.'.
constexpr int kFog[kDepthBands] = {0, 0, 0, 1, 1, 2, 3, kRampSteps};

constexpr char kHex[] = "0123456789ABCDEF";
constexpr char kB64Url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

}  // namespace

const char* viewModeName(ViewMode m) { return m == ViewMode::PixelsAndDepth ? "pixels_depth" : "objects"; }

bool viewModeFromName(const std::string& name, ViewMode& out) {
  if (name == "objects") { out = ViewMode::Objects; return true; }
  if (name == "pixels_depth") { out = ViewMode::PixelsAndDepth; return true; }
  return false;
}

int depthBand(uint16_t code, int remapMode) {
  if (code == 0xFFFF) return kSkyBand;
  const int m = (remapMode >= 0 && remapMode <= 2) ? remapMode : 0;
  int b = 0;
  for (int k = 0; k < 6; ++k) b += code > kDepthBandEdges[m][k] ? 1 : 0;
  return b;
}

int bayer4(int cx, int cy) { return kBayer[cy & 3][cx & 3]; }

int paletteIndex(int r, int g, int b) {
  int best = 0;
  int bestD = 0x7FFFFFFF;
  for (int i = 0; i < kPaletteSize; ++i) {
    const int pr = int((kPalette[i] >> 16) & 0xFF), pg = int((kPalette[i] >> 8) & 0xFF), pb = int(kPalette[i] & 0xFF);
    const int d = (r - pr) * (r - pr) + (g - pg) * (g - pg) + (b - pb) * (b - pb);
    if (d < bestD) { bestD = d; best = i; }  // strict: the lower index keeps a tie
  }
  return best;
}

uint32_t paletteColour(int index) { return 0xFF000000u | kPalette[index & 15]; }

char objectGlyph(ObjectId id) {
  switch (id) {
    case ObjectId::Wall: return '#';
    case ObjectId::Door: return '|';
    case ObjectId::Key: return 'k';
    case ObjectId::Exit: return 'E';
    case ObjectId::Light: return '*';
    case ObjectId::Pit: return 'o';
    case ObjectId::Entity: return '&';
    case ObjectId::Liquid: return '~';
    default: return 0;
  }
}

int fogSteps(int band) { return kFog[std::clamp(band, 0, kDepthBands - 1)]; }

const std::array<uint8_t, 32>& depthTableSha256() {
  static const std::array<uint8_t, 32> sha = [] {
    uint8_t bytes[3 * 6 * 2];
    int n = 0;
    for (int m = 0; m < 3; ++m)
      for (int k = 0; k < 6; ++k) {
        bytes[n++] = uint8_t(kDepthBandEdges[m][k] & 0xFF);
        bytes[n++] = uint8_t(kDepthBandEdges[m][k] >> 8);
      }
    return crypto::sha256(bytes, sizeof bytes);
  }();
  return sha;
}

std::string depthTableShaPrefix() {
  std::string s;
  for (int i = 0; i < 8; ++i) {
    s += char(std::tolower(kHex[depthTableSha256()[size_t(i)] >> 4]));
    s += char(std::tolower(kHex[depthTableSha256()[size_t(i)] & 15]));
  }
  return s;
}

bool prepare(const SourceView& src, const FrameConfig& cfg, AsciiFrame& out, std::string* err) {
  auto fail = [&](const char* why) {
    if (err) *err = why;
    return false;
  };
  if (!src.pixels || src.width <= 0 || src.height <= 0) return fail("no source frame");
  if (cfg.cols <= 0 || cfg.rows <= 0 || cfg.cols > 512 || cfg.rows > 512) return fail("cols/rows out of range");
  const int cw = src.cropW > 0 ? src.cropW : src.width;
  const int ch = src.cropH > 0 ? src.cropH : src.height;
  if (src.cropX < 0 || src.cropY < 0 || src.cropX + cw > src.width || src.cropY + ch > src.height)
    return fail("crop outside the frame");
  if (cw % cfg.cols != 0 || ch % cfg.rows != 0) return fail("cells must tile the source exactly");
  const size_t n = size_t(cfg.cols) * size_t(cfg.rows);
  out.cols = cfg.cols;
  out.rows = cfg.rows;
  out.mode = cfg.mode;
  out.glyph.assign(n, ' ');
  out.fg.assign(n, 0);
  out.band.assign(n, uint8_t(kSkyBand));
  out.object.assign(n, 0);
  out.sha256.fill(0);
  return true;
}

void buildRows(const SourceView& src, const FrameConfig& cfg, AsciiFrame& out, int rowBegin, int rowEnd) {
  const int cw = (src.cropW > 0 ? src.cropW : src.width) / cfg.cols;
  const int chh = (src.cropH > 0 ? src.cropH : src.height) / cfg.rows;
  const int cellPixels = cw * chh;
  const bool useObjects = cfg.mode == ViewMode::Objects && src.objects != nullptr;
  rowBegin = std::max(rowBegin, 0);
  rowEnd = std::min(rowEnd, cfg.rows);
  for (int cy = rowBegin; cy < rowEnd; ++cy) {
    for (int cx = 0; cx < cfg.cols; ++cx) {
      uint32_t sr = 0, sg = 0, sb = 0;
      uint16_t nearest = 0xFFFF;
      uint16_t counts[16] = {};
      const int px0 = src.cropX + cx * cw, py0 = src.cropY + cy * chh;
      for (int y = py0; y < py0 + chh; ++y) {
        const size_t row = size_t(y) * size_t(src.width);
        for (int x = px0; x < px0 + cw; ++x) {
          const uint32_t p = src.pixels[row + size_t(x)];
          sr += (p >> 16) & 0xFF;
          sg += (p >> 8) & 0xFF;
          sb += p & 0xFF;
          if (src.depth) nearest = std::min(nearest, src.depth[row + size_t(x)]);
          if (useObjects) {
            const uint8_t o = src.objects[row + size_t(x)];
            ++counts[o < 16 ? o : uint8_t(ObjectId::Other)];
          }
        }
      }
      const uint32_t half = uint32_t(cellPixels) / 2u;
      const int r = int((sr + half) / uint32_t(cellPixels));
      const int g = int((sg + half) / uint32_t(cellPixels));
      const int b = int((sb + half) / uint32_t(cellPixels));
      const int band = depthBand(nearest, src.depthRemapMode);
      int object = 0;
      if (useObjects) {
        uint16_t best = 0;
        for (int k = 0; k < 16; ++k)
          if (counts[k] > best) { best = counts[k]; object = k; }  // strict: the lower id keeps a tie
      }
      char glyph = ' ';
      if (band != kSkyBand) {
        const int y8 = std::clamp(luma8(r, g, b) + (bayer4(cx, cy) - 8) * 2, 0, 255);
        int idx = (y8 * kRampSteps) >> 8;
        idx = std::max(1, idx - fogSteps(band));  // anything drawn shows at least '.'
        glyph = kRamp[idx];
        if (useObjects) {
          const char og = objectGlyph(ObjectId(object));
          if (og) glyph = og;
        }
      }
      const size_t i = size_t(cy) * size_t(cfg.cols) + size_t(cx);
      out.glyph[i] = glyph;
      out.fg[i] = uint8_t(paletteIndex(r, g, b));
      out.band[i] = uint8_t(band);
      out.object[i] = uint8_t(object);
    }
  }
}

void finish(AsciiFrame& out) {
  const std::string rec = canonicalRecord(out);
  out.sha256 = crypto::sha256(reinterpret_cast<const uint8_t*>(rec.data()), rec.size());
}

bool build(const SourceView& src, const FrameConfig& cfg, AsciiFrame& out, std::string* err) {
  if (!prepare(src, cfg, out, err)) return false;
  buildRows(src, cfg, out, 0, cfg.rows);
  finish(out);
  return true;
}

namespace {
template <class F>
std::string rowsOf(const AsciiFrame& f, F cellChar) {
  std::string s;
  s.reserve(size_t(f.cols + 1) * size_t(f.rows));
  for (int y = 0; y < f.rows; ++y) {
    if (y) s += '\n';
    for (int x = 0; x < f.cols; ++x) s += cellChar(size_t(y) * size_t(f.cols) + size_t(x));
  }
  return s;
}
}  // namespace

std::string glyphRows(const AsciiFrame& f) { return rowsOf(f, [&](size_t i) { return f.glyph[i]; }); }
std::string fgRows(const AsciiFrame& f) { return rowsOf(f, [&](size_t i) { return kHex[f.fg[i] & 15]; }); }
std::string bandRows(const AsciiFrame& f) { return rowsOf(f, [&](size_t i) { return char('0' + (f.band[i] & 7)); }); }
std::string objectRows(const AsciiFrame& f) { return rowsOf(f, [&](size_t i) { return kB64Url[f.object[i] & 63]; }); }

std::string canonicalRecord(const AsciiFrame& f) {
  std::string s = "nemo_ascii.v1|" + std::to_string(f.cols) + "|" + std::to_string(f.rows) + "|" +
                  viewModeName(f.mode) + "|ramp.v1|pal16.v1|" + depthTableShaPrefix() + "|";
  s += glyphRows(f);
  s += '|';
  s += fgRows(f);
  s += '|';
  s += bandRows(f);
  s += '|';
  s += objectRows(f);
  return s;
}

std::string shaHex(const AsciiFrame& f) {
  std::string s;
  for (uint8_t v : f.sha256) {
    s += char(std::tolower(kHex[v >> 4]));
    s += char(std::tolower(kHex[v & 15]));
  }
  return s;
}

std::string rleEncode(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size();) {
    const char c = text[i];
    size_t j = i + 1;
    if (c != '\n')
      while (j < text.size() && text[j] == c) ++j;
    const size_t run = j - i;
    if (run >= 4) {
      out += c;
      out += std::to_string(run);
    } else {
      out.append(run, c);
    }
    i = j;
  }
  return out;
}

bool rleDecode(const std::string& rle, std::string& out) {
  out.clear();
  for (size_t i = 0; i < rle.size();) {
    const char c = rle[i];
    if (c >= '0' && c <= '9') return false;  // a count with no character before it
    size_t j = i + 1;
    size_t n = 0;
    while (j < rle.size() && rle[j] >= '0' && rle[j] <= '9') {
      n = n * 10 + size_t(rle[j] - '0');
      if (n > 1u << 20) return false;
      ++j;
    }
    if (j == i + 1) {
      out += c;
    } else {
      if (n < 4 || c == '\n') return false;  // not a form rleEncode makes
      out.append(n, c);
    }
    i = j;
  }
  return true;
}

}  // namespace litholattice::nemoview
