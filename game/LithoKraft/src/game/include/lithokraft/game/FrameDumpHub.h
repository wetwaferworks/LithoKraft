#pragma once
// Showcase --frame-dump: process-wide slot holding the active IFrameSink. Game polls it
// in capturePendingScreenshot (no Game layout change); main installs the encoder.
#include "lithokraft/game/IFrameSink.h"

namespace lithokraft {

class cFrameDumpHub {
 public:
  static cFrameDumpHub& instance();
  void install(IFrameSink* sink) { sink_ = sink; armed_ = false; }
  IFrameSink* sink() const { return sink_; }
  bool armed() const { return armed_; }
  void setArmed(bool a) { armed_ = a; }

 private:
  IFrameSink* sink_ = nullptr;
  bool armed_ = false;
};

}  // namespace lithokraft
