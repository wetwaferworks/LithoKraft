# Codec Stubs Implementation Report — 2026-10-11

Basic decode-pipeline stubs for the four approved codecs (JD: stubs now, builder fleshes out).
No third-party decoders vendored, per instruction.

## What landed

New files (LF, `litholattice` namespace):
- `engine/src/audio/include/litholattice/audio/codecs.h` — `AudioCodec` enum,
  `sniffAudioCodec()`, `AudioFormatInfo`, `IAudioDecoder` interface,
  `makeAudioDecoder()` factory.
- `engine/src/audio/codecs.cpp` — the four decoders (WAV full, FLAC/Opus/Vorbis
  container-parse + decode stubbed). Registered in `src/audio/CMakeLists.txt`.
- 9 new self-test checks in `src/audio/audio_selftest.cpp` (CRLF preserved).

The seam: `IAudioDecoder::decode()` matches `AudioStreamer::DecodeFn` exactly
(interleaved stereo float, short write = end-of-stream), so a decoder plugs
straight into the existing async worker via `AudioStreamer::play()`. Decoders
borrow the source buffer (zero copy); decode never runs on the audio thread.

## Per-codec status

| Codec | Sniff | Header parse | Decode | Tests |
|---|---|---|---|---|
| WAV | RIFF/WAVE magic | fmt/data chunks, PCM 1 + IEEE-float 3 | **Full**: 8/16/24/32-bit PCM, 32/64-bit float, mono→stereo | exact-sample checks, EOS, 3 clean error paths |
| FLAC | `fLaC` marker | STREAMINFO fully parsed (rate/ch/bps/total) | **Stubbed** (TODO) | header values verified, decode() reports not-implemented |
| Opus | OggS + OpusHead | OpusHead parsed (channels, pre-skip, rate) | **Stubbed** (TODO) | header values verified, decode() reports not-implemented |
| Vorbis | OggS + vorbis ident | identification header parsed (channels, rate) | **Stubbed** (TODO) | header values verified, decode() reports not-implemented |

All 9 codec checks pass; `AUDIO SELFTEST OK`. Full build clean, no new warnings.
The only other failures in the run are pre-existing / unrelated to this work:
the two grandfathered ones (mesher golden hash, block-names narrowing), plus
one `net: reliable delivers every byte...` check that fails consistently (3/3
runs). That test exercises the net module's `ReliableChannel`, which a sibling
agent is actively implementing right now (`src/net/channels.cpp` modified
minutes before these runs) — it is not touched by, and cannot be caused by,
this audio-only change. Flagged for the parent to route.

## Builder notes

### WAV — done, minor extensions possible
Fully decodable today. Rejects cleanly: non-PCM/float format tags, >stereo,
truncated files. **Builder may add:** `WAVE_FORMAT_EXTENSIBLE` (0xFFFE) for
surround layouts; `fact` chunk handling for compressed-in-WAV (not needed).
Resampling to the engine rate is the game/mixer's job, not the decoder's.

### FLAC — vendor dr_flac
- **Library:** [dr_flac](https://github.com/mackron/dr_libs) — public domain /
  MIT-0, single header, no dependencies. Safest license, smallest footprint.
- **What's stubbed:** frame decoding (LPC prediction, Rice residual coding).
  The STREAMINFO parse already yields everything dr_flac needs as a sanity
  check (rate/channels/bps/total samples).
- **Pitfalls:** honor the seektable metadata block for seeking (don't binary
  search); channel assignment for >2ch files (FLAC has a defined mapping —
  downmix deliberately, don't just take ch 0/1); sample rates up to 655350 Hz
  need resampling to the engine rate.

### Opus — link libopus
- **Library:** [libopus](https://opus-codec.org) (BSD 3-clause, attribution
  required) + [opusfile](https://github.com/xiph/opusfile) for the Ogg layer,
  or a minimal Ogg pager over the existing `firstOggPacket` helper.
- **What's stubbed:** packet decode. OpusHead parsing (channels, pre-skip)
  is done.
- **Pitfalls:** **pre-skip is mandatory** — drop that many leading decoded
  samples or every stream starts with a click; Opus always decodes at 48 kHz,
  resample if the engine rate differs; channel mapping family 1 (surround)
  needs the Vorbis-style channel order, not naive L/R; read OpusTags for
  duration/loop points.

### Vorbis — vendor stb_vorbis (or link libvorbisfile)
- **Library:** [stb_vorbis](https://github.com/nothings/stb) — public domain /
  MIT, single header, simplest path. For full features (chained streams,
  callbacks), libvorbisfile (BSD 3-clause).
- **What's stubbed:** packet decode (floor/residue/inverse MDCT). The
  identification header parse (channels, rate, bitrates) is done.
- **Pitfalls:** chained Ogg Vorbis streams (multiple logical bitstreams back
  to back — handle or reject explicitly); floor1 inverse coupling on stereo;
  sample-rate conversion to the engine rate.

### License guidance
Prefer public-domain/MIT-0 (dr_flac, stb_vorbis) where possible; BSD
(libopus, libvorbisfile) is fine with attribution. **Do not pull in GPL
decoders** — it would taint the engine's licensing.

## Open questions
- Who owns resampling: decoder output at file rate vs engine-rate conversion
  inside the streaming worker (recommendation: a small resampler stage in the
  worker, one place, all codecs benefit).
- Streaming from disk vs memory: current decoders borrow a memory buffer;
  large music files may want a file-backed source with prefetch.
- Game-side: which codecs ship in the base game data (WAV for SFX, Opus for
  music is the conventional split).
