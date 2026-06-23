# Contributing to AudioFilterLib

Thank you for your interest in contributing! This guide will help you get started.

## Code of Conduct

Be respectful and inclusive. We welcome contributions from all backgrounds and experience levels.

## Getting Started

### Prerequisites
- C++17 compiler (g++, clang, MSVC)
- CMake 3.20+
- libsndfile
- Google Test (for unit tests)

### Development Setup

```bash
git clone https://github.com/yourusername/AudioFilterLib.git
cd AudioFilterLib
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON -DENABLE_EXAMPLES=ON ..
cmake --build .
ctest --verbose
```

## Contribution Workflow

1. **Fork** the repository
2. **Create a branch** for your feature: `git checkout -b feature/my-feature`
3. **Make changes** and commit with clear messages: `git commit -m "Add new feature"`
4. **Add tests** for new functionality
5. **Run tests locally**: `ctest --verbose`
6. **Push** to your fork: `git push origin feature/my-feature`
7. **Open a Pull Request** with a clear description

## Code Style

### C++ Guidelines

**Naming:**
- Classes: `PascalCase` (e.g., `BiquadFilter`)
- Functions: `camelCase` (e.g., `processFrame()`)
- Member variables: `m_snake_case` (e.g., `m_sample_rate`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_CHANNELS`)

**Formatting:**
- Use 4 spaces for indentation (no tabs)
- 100-character line limit
- Brace style: Opening brace on same line (K&R style)

```cpp
class MyFilter : public FilterBase {
public:
    void process(float* buffer, size_t num_samples) override;

private:
    float m_coefficient = 0.0f;
};
```

**Documentation:**
- Document public methods with Doxygen comments
- Explain non-obvious algorithm choices
- Include usage examples

```cpp
/**
 * @brief Brief description
 *
 * Longer description with details about behavior,
 * performance characteristics, and usage.
 *
 * @param param1 Description of parameter
 * @return Description of return value
 *
 * @throw std::invalid_argument if input invalid
 *
 * @example
 * ```cpp
 * filter.configure(48000, 1);
 * float output = filter.process(input);
 * ```
 */
float process(float input_sample) override;
```

## Testing Requirements

All contributions must include tests:

- **Unit tests** for individual components
- **Integration tests** for filter chains
- **Edge case tests** for boundary conditions

```cpp
TEST(MyFeature, BasicFunctionality) {
    MyFilter filter;
    filter.configure(48000, 1);
    
    float output = filter.process(0.5f);
    
    EXPECT_FLOAT_EQ(output, expected_value);
}
```

Run tests: `ctest --verbose`

## Performance Requirements

- IIR filters: < 1 µs per sample at 48 kHz
- FIR filters: should not exceed 10 µs per 100 taps
- No dynamic allocation in hot path
- No locks in real-time processing code

Benchmark changes: `./examples/benchmark`

## Documentation

Update relevant documentation when making changes:

- **API changes**: Update `README.md` and inline Doxygen comments
- **Algorithm changes**: Add explanation to `OPTIMIZATION.md` or `docs/THEORY.md`
- **Build changes**: Update `QUICK_START.md`

## Commit Messages

Use clear, descriptive commit messages:

```
Add Kaiser window FIR filter design

- Implement designKaiserLowpass() with parametric beta
- Add Bessel I0 approximation for window computation
- Include unit tests for Kaiser window accuracy
- Measure performance: 11.8M samples/sec for 127 taps
```

Format:
- First line: 50 chars, imperative mood ("Add", not "Added")
- Blank line
- Details: explain *why*, not just *what*
- Reference issues: "Fixes #123"

## Pull Request Guidelines

**PR Title:** Clear and descriptive
- ✅ "Add Chebyshev Type II filter support"
- ❌ "fix stuff"

**PR Description:**
- Explain what and why
- Link related issues
- List breaking changes
- Include performance impact (if relevant)

**Example PR:**
```markdown
## Description
Implements Chebyshev Type II filter design for steeper rolloff with
passband ripple control.

## Changes
- Add chebyshevType2Poles() algorithm
- Implement designChebyshevHighpass()
- Add 15+ unit tests
- Update THEORY.md with pole placement math

## Performance
- Slightly slower than Type I (elliptical pole layout)
- ~0.06 cycles/sample vs 0.05 for Type I

Fixes #42
```

## Review Process

1. Automated checks run (tests, linting)
2. Maintainers review code
3. Feedback and iteration
4. Approval and merge

## Areas for Contribution

### High Priority
- [ ] SIMD vectorization (SSE/AVX)
- [ ] GPU acceleration (CUDA/HIP)
- [ ] Elliptic filter design
- [ ] Adaptive filtering (LMS/RLS)

### Medium Priority
- [ ] More window functions (Bartlett, Hann, etc.)
- [ ] Multi-rate filtering (decimation, interpolation)
- [ ] Filter visualization tools
- [ ] Python bindings

### Documentation
- [ ] Tutorial articles
- [ ] More example programs
- [ ] Video walkthroughs
- [ ] Performance case studies

## Reporting Issues

Report bugs via GitHub Issues with:

**Title:** Clear bug description
**Description:**
- Expected behavior
- Actual behavior
- Steps to reproduce
- System info (OS, compiler, versions)
- Minimal reproducible example

**Example:**
```markdown
## Bug: Butterworth filter unstable at high orders

### Expected
4th order lowpass at 10 kHz should be stable

### Actual
Filter produces NaN output

### Reproduce
```cpp
auto biquads = designer.designButterworthLowpass(48000, 10000, 8);
// ... process
```

### System
- OS: Ubuntu 22.04
- Compiler: g++-11
- CMake: 3.25
```

## License

By contributing, you agree your code will be licensed under the MIT License.

## Questions?

- Open a GitHub Discussion
- Check existing issues
- See README.md for examples

Thank you for contributing! 🎉
