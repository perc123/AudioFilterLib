# AudioFilterLib Quick Start

## 1. Clone & Initialize

```bash
# Create new repo
mkdir AudioFilterLib && cd AudioFilterLib
git init

# Copy files from this setup (README, CMakeLists.txt, .gitignore)
# Then run setup script:
bash SETUP.sh
```

## 2. Install Dependencies

### Option A: System Package Manager

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake git libsndfile1-dev
# Optional for testing:
sudo apt install libgtest-dev
```

**macOS:**
```bash
brew install cmake libsndfile
# Optional:
brew install googletest
```

**Windows (MSVC + vcpkg):**
```bash
# Use vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install libsndfile:x64-windows gtest:x64-windows
```

### Option B: vcpkg (Cross-Platform)

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh  # macOS/Linux
# Windows: .\bootstrap-vcpkg.bat

# Install libraries
./vcpkg install libsndfile gtest

# Generate toolchain file
# cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake ..
```

## 3. Build

```bash
# Create build directory
mkdir build && cd build

# Configure (Release build with optimizations)
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_TESTS=ON \
      -DENABLE_EXAMPLES=ON \
      -DENABLE_SIMD=ON \
      ..

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --verbose
```

## 4. Verify Installation

```bash
# Check that library built successfully
ls -la lib*/  # or bin/ on Windows
```

## 5. Development Workflow

### Edit Code
- Headers: `include/*.h`
- Implementation: `src/*.cpp`
- Tests: `tests/filter_tests.cpp`
- Examples: `examples/wav_filter_example.cpp`

### Rebuild After Changes
```bash
cd build
cmake --build . -j$(nproc)
ctest --verbose
```

### Clean Build
```bash
rm -rf build/
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON -DENABLE_EXAMPLES=ON ..
cmake --build .
```

## 6. IDE Setup

### VS Code
```bash
# Install extensions:
# - C/C++ (Microsoft)
# - CMake Tools (Microsoft)
# - CMake (twxs)

# Open folder in VS Code
code .

# Select compiler in CMake Tools
# Configure and build via UI
```

### CLion / IntelliJ
- Open as CMake project
- Select C++ 17 compiler
- CMake should auto-configure

### Visual Studio 2019+
- Open folder directly (File → Open Folder)
- VS will detect CMakeLists.txt
- Configure via CMakeSettings.json

## 7. Project Structure Reference

```
AudioFilterLib/
├── CMakeLists.txt              # Root build config
├── README.md                    # Full documentation
├── QUICK_START.md              # This file
├── SETUP.sh                    # Setup script
├── LICENSE                     # MIT license
├── vcpkg.json                  # Dependency manifest
├── .gitignore                  # Git ignore rules
│
├── include/                    # Public headers
│   ├── filter_base.h          # Abstract filter interface
│   ├── biquad_filter.h        # Biquad implementation
│   ├── iir_designer.h         # IIR design algorithms
│   ├── fir_designer.h         # FIR design algorithms
│   └── audio_buffer.h         # Real-time ring buffer
│
├── src/                        # Implementation
│   ├── filter_base.cpp
│   ├── biquad_filter.cpp
│   ├── iir_designer.cpp
│   ├── fir_designer.cpp
│   └── audio_buffer.cpp
│
├── tests/                      # Unit tests
│   ├── CMakeLists.txt
│   └── filter_tests.cpp
│
├── examples/                   # Example programs
│   ├── CMakeLists.txt
│   └── wav_filter_example.cpp
│
├── docs/                       # Documentation
│   ├── DESIGN.md              # Architecture details
│   ├── THEORY.md              # DSP theory reference
│   └── BENCHMARKS.md          # Performance results
│
└── build/                      # Build artifacts (created at build time)
    ├── CMakeCache.txt
    ├── lib/                   # Built library
    ├── bin/                   # Built executables
    └── Testing/               # Test results
```

## 8. CMake Configuration Options

```bash
# Full configuration with all options
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_TESTS=ON \
      -DENABLE_EXAMPLES=ON \
      -DENABLE_SIMD=ON \
      -DENABLE_LTO=ON \
      -DBUILD_SHARED_LIBS=OFF \
      ..
```

| Option | Default | Purpose |
|--------|---------|---------|
| `CMAKE_BUILD_TYPE` | Release | Debug or Release build |
| `ENABLE_TESTS` | ON | Build unit tests |
| `ENABLE_EXAMPLES` | ON | Build example programs |
| `ENABLE_SIMD` | ON | SIMD optimizations (SSE/AVX) |
| `ENABLE_LTO` | ON | Link-time optimization |
| `BUILD_SHARED_LIBS` | OFF | Static or shared library |

## 9. Troubleshooting

### CMake doesn't find libsndfile
```bash
# Ubuntu: install dev package
sudo apt install libsndfile1-dev

# Or specify pkg-config path
cmake -DCMAKE_PREFIX_PATH=/usr/lib/pkgconfig ..

# Or use vcpkg
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake ..
```

### Compiler not found
```bash
# Check available compilers
cmake --version
which g++
which clang++

# Specify compiler explicitly
cmake -DCMAKE_CXX_COMPILER=g++-11 ..
```

### Tests not building
```bash
# Install GTest
apt install libgtest-dev    # Ubuntu
brew install googletest     # macOS

# Or disable tests
cmake -DENABLE_TESTS=OFF ..
```

### SIMD compilation errors
```bash
# Some older compilers don't support -march=native
# Disable SIMD optimizations
cmake -DENABLE_SIMD=OFF ..
```

## 10. Next Steps

After successful build:
1. **Read the docs**: `README.md` and `docs/` folder
2. **Run examples**: `./examples/wav_filter_example`
3. **Run tests**: `ctest --verbose`
4. **Start Phase 2**: Implement filter abstractions

Ready? Go to **Phase 2: DSP Math & Filter Abstractions**
