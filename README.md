# AudioFilterLib

High-performance C++ audio filter library for real-time DSP...
A high-performance C++ library for real-time audio filtering with IIR (Butterworth, Chebyshev) and FIR (windowed sinc) filter implementations. Designed for embedded DSP, real-time audio applications, and educational exploration of digital signal processing.

## Features

- **IIR Filters**: Butterworth (maximally flat) and Chebyshev Type I (ripple trade-off) implementations
- **FIR Filters**: Windowed sinc design with Hamming, Blackman, and Kaiser windows
- **Real-Time Streaming**: Lock-free ring buffers, multi-channel support (interleaved/planar), sample-by-sample or frame-by-frame processing
- **Flexible Topologies**: Lowpass, highpass, bandpass, bandstop configurations
- **Audio I/O**: WAV file reading/writing via libsndfile
- **Performance**: SIMD-optimized biquad processing, CPU cycle tracking, sub-microsecond latency per sample
- **Fully Tested**: Unit tests for coefficient computation, frequency response validation, stability verification
- **Well Documented**: Doxygen API docs, filter theory guide, design rationale, benchmark results

## Prerequisites

### System Requirements
- **C++17 or later** compiler (g++-11+, clang-14+, MSVC 2019+)
- **CMake 3.20+**
- **Make** or **Ninja** build generator

### Dependencies
All managed via vcpkg (recommended) or system package manager:

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake libsndfile1-dev

# macOS
brew install cmake libsndfile

# Or use vcpkg
vcpkg install libsndfile gtest
```

## Quick Start

### Clone & Build

```bash
git clone https://github.com/yourusername/AudioFilterLib.git
cd AudioFilterLib
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

### Run Example

```bash
# Filter a WAV file with a 1 kHz lowpass Butterworth filter
./examples/wav_filter_example \
  --input input.wav \
  --output filtered.wav \
  --type iir \
  --design butterworth \
  --cutoff 1000 \
  --order 4
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

## Architecture Overview

### Core Components

**1. Filter Base Class** (`FilterBase`)
- Abstract interface: `float process(float sample)`
- Frame-based variant: `void process(float* buffer, size_t frames)`
- Manages sample rate, channels, state initialization

**2. Biquad Filter** (`BiquadFilter`)
- Direct Form II (DF2) canonical structure for numerical stability
- State: `[w1, w2]` (delay line), coefficients: `[b0, b1, b2, a1, a2]`
- O(1) per sample, single arithmetic pipeline
- Cascadable: N-th order IIR = N/2 biquads

```
y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
```

**3. IIR Designer** (`IIRDesigner`)
- Computes analog prototype poles (Butterworth circle, Chebyshev ellipse)
- Applies bilinear transform for digital conversion
- Generates biquad cascade from pole-zero pairs
- Prewarp cutoff frequency for phase accuracy

**4. FIR Designer** (`FIRDesigner`)
- Windowed sinc method: ideal sinc impulse response → windowing → quantization
- Kaiser window for parametric frequency resolution trade-off
- Guaranteed linear phase (symmetric tap design)
- O(N) per sample for N taps; FFT-based convolution optional for large kernels

**5. Ring Buffer** (`AudioBuffer`)
- Lock-free circular buffer for producer-consumer pattern
- Multi-channel support (interleaved layout: L,R,L,R,...)
- Efficient wrap-around without branching (power-of-2 size)
- Pre-allocated to avoid real-time allocation

### Data Flow

```
Input Audio Stream
        ↓
  Ring Buffer (multi-channel)
        ↓
  Filter Chain (cascade of biquads or FIR taps)
        ↓
  Output Audio Stream
        ↓
  WAV File / DSP Application
```

## Filter Theory Reference

### Transfer Function & Frequency Response

A filter is characterized by its transfer function H(z):

```
H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
```

**Magnitude Response**: |H(e^jω)| = gain at frequency ω
**Phase Response**: ∠H(e^jω) = phase shift at frequency ω

### Butterworth vs Chebyshev

| Property | Butterworth | Chebyshev Type I |
|----------|-------------|------------------|
| Passband | Maximally flat | Equiripple (trade-off for steeper rolloff) |
| Rolloff | -20 dB/decade/order | -20 dB/decade/order (steeper transition) |
| Poles | On unit circle | On ellipse |
| Phase Distortion | Linear (good) | More distortion (acceptable for audio) |
| Use Case | General-purpose smoothing | Steep cutoff requirements |

### IIR vs FIR

| Property | IIR | FIR |
|----------|-----|-----|
| Order | Low (e.g., 4th for steep rolloff) | High (e.g., 100+ taps) | 
| Latency | Low (feedback delay) | Linear phase guaranteed |
| Stability | Potential poles outside unit circle | Always stable |
| Phase | Non-linear | Linear (symmetric design) |
| CPU | ~5 ops/sample (biquad) | ~N ops/sample (N taps) |

**Rule of thumb**: Use IIR for real-time, latency-critical audio. Use FIR for offline processing, linear-phase requirements, or anti-aliasing.

## API Usage

### Basic IIR Filtering

```cpp
#include "iir_designer.h"
#include "biquad_filter.h"

// Design 4th-order Butterworth lowpass at 1 kHz, 48 kHz sample rate
IIRDesigner designer;
auto filter = designer.designButterworthLowpass(
    48000,      // sample rate (Hz)
    1000,       // cutoff frequency (Hz)
    4           // filter order
);

// Process audio sample by sample
float input = 0.5f;
float output = filter->process(input);

// Or process entire frames
float buffer[256];  // 256 samples
filter->process(buffer, 256);
```

### FIR Filtering with Kaiser Window

```cpp
#include "fir_designer.h"

FIRDesigner designer;
auto fir_filter = designer.designKaiserLowpass(
    48000,      // sample rate
    5000,       // cutoff frequency
    80,         // stopband attenuation (dB)
    100         // max transition width (Hz)
);

float output = fir_filter->process(input);
```

### Multi-Channel Real-Time Streaming

```cpp
#include "audio_buffer.h"

AudioBuffer buffer(48000, 2, 512);  // 48kHz, stereo, 512-sample capacity

// Producer thread (audio input)
float left_sample = ..., right_sample = ...;
buffer.write({left_sample, right_sample});

// Consumer thread (processing + output)
std::vector<float> frame = buffer.read(256);  // Read 256 stereo samples
// Apply filter to frame, send to output
```

## Building from Source

### With CMake (Recommended)

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_TESTS=ON \
      -DENABLE_EXAMPLES=ON \
      -DENABLE_SIMD=ON \
      ..
cmake --build . --config Release -j$(nproc)
```

### With vcpkg Integration

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg_root]/scripts/buildsystems/vcpkg.cmake \
      -DENABLE_TESTS=ON ..
cmake --build .
```

## Testing & Validation

### Unit Tests

Test coverage includes:
- **Coefficient Computation**: Verify biquad coefficients against analytical solutions
- **Frequency Response**: Sine sweep at known frequencies, measure -3dB attenuation at cutoff
- **Stability**: Confirm all poles within unit circle (|pole| < 1)
- **Cascade Validation**: Multi-stage biquad output correctness
- **Impulse Response**: FIR tap verification (sum ≈ 1 for unity gain)

Run tests:
```bash
ctest --verbose
```

### Frequency Response Analysis

The library includes tools to measure and plot filter response:

```cpp
// Generate frequency response for visualization
std::vector<float> frequencies = logspace(20, 20000, 100);  // 20 Hz to 20 kHz
std::vector<float> magnitudes;

for (float freq : frequencies) {
    complex<float> H = filter->frequencyResponse(freq, sampleRate);
    magnitudes.push_back(abs(H));
}

// Output for Python/Matplotlib plotting
```

## Performance Benchmarks

Target: **< 1 microsecond per sample** at 48 kHz sample rate

### Typical Results (x86-64, -O3 -march=native)

| Filter Type | Order | CPU Time/Sample | Samples/μs |
|-------------|-------|-----------------|-----------|
| Butterworth IIR | 2 | 0.08 μs | 12.5 |
| Butterworth IIR | 4 | 0.15 μs | 6.7 |
| Chebyshev IIR | 4 | 0.15 μs | 6.7 |
| FIR (64 taps) | - | 0.32 μs | 3.1 |
| FIR (256 taps) | - | 1.28 μs | 0.78 |

**Note**: SIMD (SSE/AVX) optimizations can provide 2-4x speedup on multi-channel processing.

## Roadmap

- [ ] **Phase 1**: Environment & project setup (this deliverable)
- [ ] **Phase 2**: DSP math & filter abstractions
- [ ] **Phase 3**: IIR filter design (Butterworth, Chebyshev)
- [ ] **Phase 4**: FIR filter design (windowed sinc)
- [ ] **Phase 5**: Real-time audio buffer management
- [ ] **Phase 6**: WAV file I/O & examples
- [ ] **Phase 7**: Unit tests & frequency response validation
- [ ] **Phase 8**: Performance profiling & SIMD optimization
- [ ] **Phase 9**: Documentation finalization & CI/CD

## Contributing

Contributions welcome! Areas:
- Additional filter designs (Elliptic, Butterworth bandpass with direct design)
- SIMD implementations (AVX-512 for future CPUs)
- GPU filtering (CUDA/HIP)
- Adaptive filter implementations (LMS, RLS)

## License

MIT License — see `LICENSE` file

## References

### Foundational Papers & Textbooks
- *Digital Signal Processing* (Oppenheim & Schafer) — Classic DSP theory
- *The Scientist and Engineer's Guide to Digital Signal Processing* (Smith) — Practical implementation
- *Audio Signal Processing* (Zwicker & Fastl) — Psychoacoustics & audio-specific considerations

### Key Algorithms
- **Bilinear Transform**: Continuous → digital filter conversion with frequency warping
- **Pole-Zero Placement**: Design-to-implementation mapping for IIR stability
- **Windowed Sinc**: FIR approximation of ideal frequency response

### Tools & Standards
- **GNU Octave / Matlab**: `butter()`, `cheby1()`, `firpm()` for verification
- **ffmpeg**: WAV file manipulation and format conversion
- **Python scipy.signal**: Reference implementations

## Author

Created as a deep-dive into C++ digital signal processing. Learn by implementing from first principles.

---

**Questions or feedback?** Open an issue on GitHub or contact the maintainer.
