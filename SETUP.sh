#!/bin/bash
# AudioFilterLib Repository Setup Script
# Run this in your repo directory after cloning or creating a new repo

set -e

echo "=== Setting up AudioFilterLib Repository Structure ==="

# Create directory structure
mkdir -p src
mkdir -p include
mkdir -p tests
mkdir -p examples
mkdir -p docs
mkdir -p build

# Create placeholder files in directories
touch include/.gitkeep
touch src/.gitkeep
touch tests/.gitkeep
touch examples/.gitkeep
touch docs/.gitkeep

# Create subdirectory CMakeLists for tests
cat > tests/CMakeLists.txt << 'EOF'
# Tests subdirectory

add_executable(audiofilter_tests
    filter_tests.cpp
)

target_link_libraries(audiofilter_tests
    PRIVATE
        audiofilter
        GTest::gtest_main
)

gtest_discover_tests(audiofilter_tests)
EOF

# Create subdirectory CMakeLists for examples
cat > examples/CMakeLists.txt << 'EOF'
# Examples subdirectory

add_executable(wav_filter_example
    wav_filter_example.cpp
)

target_link_libraries(wav_filter_example
    PRIVATE
        audiofilter
)
EOF

echo "✓ Directory structure created"
echo "✓ CMakeLists.txt for tests/ and examples/ generated"

# Initialize git if not already done
if [ ! -d .git ]; then
    git init
    echo "✓ Git repository initialized"
else
    echo "✓ Git repository already exists"
fi

# Create initial git ignore commit
if [ ! -f .git/config ] || ! git config user.name > /dev/null 2>&1; then
    echo ""
    echo "Configure git for first commit:"
    git config user.name "Your Name"
    git config user.email "your.email@example.com"
fi

# Create a basic LICENSE (MIT)
if [ ! -f LICENSE ]; then
    cat > LICENSE << 'EOF'
MIT License

Copyright (c) 2024 AudioFilterLib Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF
    echo "✓ MIT LICENSE created"
fi

# Create vcpkg.json for dependency management
if [ ! -f vcpkg.json ]; then
    cat > vcpkg.json << 'EOF'
{
  "name": "audiofilterlib",
  "version-string": "0.1.0",
  "description": "High-performance C++ audio filter library",
  "dependencies": [
    "libsndfile",
    {
      "name": "gtest",
      "optional": true
    }
  ]
}
EOF
    echo "✓ vcpkg.json manifest created"
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Next steps:"
echo "1. Configure git user info (if not already done):"
echo "   git config user.name 'Your Name'"
echo "   git config user.email 'your@example.com'"
echo ""
echo "2. Create initial commit:"
echo "   git add ."
echo "   git commit -m 'Initial project structure setup'"
echo ""
echo "3. Install dependencies:"
echo "   # Using vcpkg:"
echo "   vcpkg install --triplet=x64-linux (or your target triplet)"
echo ""
echo "   # Or use your system package manager:"
echo "   apt install libsndfile1-dev cmake    # Ubuntu/Debian"
echo "   brew install libsndfile cmake         # macOS"
echo ""
echo "4. Build the project:"
echo "   cd build"
echo "   cmake -DCMAKE_BUILD_TYPE=Release .."
echo "   cmake --build . -j$(nproc)"
echo ""
echo "5. Ready to implement Phase 2!"
echo ""
