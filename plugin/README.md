# AudioFilterLib Demo Plugin

A small JUCE `AudioProcessor` that wraps `audiofilter::IIRDesigner` /
`audiofilter::BiquadFilter` in a loadable VST3/AU plugin (plus a Standalone
build). This exists as a learning exercise for JUCE — it isn't part of the
core `audiofilter` library and is off by default.

## Building

Requires the `extern/JUCE` submodule (JUCE 8.0.15, pinned):

```bash
git submodule update --init --recursive   # only needed once, or after a fresh clone

mkdir build && cd build
cmake -DENABLE_JUCE_DEMO=ON ..
cmake --build . --target audiofilter_plugin_VST3 -j$(nproc)     # VST3 only
# or:
cmake --build . --target audiofilter_plugin_Standalone -j$(nproc)  # runnable app, no DAW needed
# or just:
cmake --build . --target audiofilter_plugin -j$(nproc)          # every FORMATS target (VST3, AU, Standalone)
```

On macOS, building the `AU` format needs a full Xcode install (not just the
Command Line Tools) for the Audio Unit SDK headers; `VST3` and `Standalone`
don't.

Build products land under `build/plugin/audiofilter_plugin_artefacts/`.
JUCE's CMake support also copies the VST3/AU into the usual system plugin
folders on macOS automatically — restart your DAW (or rescan plugins) to
pick it up.

## What it exposes

| Parameter | Type | Notes |
|---|---|---|
| Filter Type | choice | Lowpass / Highpass / Bandpass / Bandstop |
| Design Method | choice | Butterworth / Chebyshev I |
| Frequency | float, 20 Hz–20 kHz | Cutoff (LP/HP) or center (BP/BS) |
| Bandwidth | float, 10 Hz–10 kHz | Only used for Bandpass/Bandstop |
| Order | choice | 2 / 4 / 6 / 8 |
| Ripple | float, 0.1–3.0 dB | Only used for Chebyshev I |

All parameters are hosted via `juce::AudioProcessorValueTreeState`, so
host automation, undo, and session save/load work without any extra
plumbing in the processor.

## How it wires into `audiofilter`

`PluginProcessor` keeps one independent cascade of `audiofilter::BiquadFilter`
stages per audio channel (each biquad owns its own delay-line state, so
channels can't share one). On a parameter (or sample-rate) change, it calls
the matching `IIRDesigner::design*` method to get a fresh cascade, then
`processBlock()` runs each channel's buffer through
`BiquadFilter::processFrame()` for every stage in the cascade — a direct
mapping from JUCE's per-channel `float*` buffers onto AudioFilterLib's
existing frame-processing API.

## Known limitations (left as an exercise)

- **Filter rebuild isn't real-time-safe.** `rebuildFilters()` is called from
  `processBlock()` and allocates (`std::vector`, one `std::unique_ptr` per
  biquad via `IIRDesigner`). A production plugin would move the redesign to
  a background thread and hand the audio thread a new chain through a
  lock-free swap (e.g. `juce::AbstractFifo`, or a pair of chains plus an
  atomic "active" index) instead of allocating on the audio thread.
- **No parameter smoothing.** Turning the Frequency/Order knob rebuilds the
  filter outright, which can click at audio rates. Real plugins usually
  smooth the underlying coefficients (or crossfade old/new chains) instead
  of swapping cold.
- **No true stereo linking beyond independent per-channel state** — both
  channels use identical coefficients (as they should), but there's no
  mid/side or channel-linked processing option.
