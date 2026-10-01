# Contributing to Plugin Analyser

Thank you for your interest in contributing to Plugin Analyser! 🎉

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/your-username/plugin-analyser.git
   cd plugin-analyser
   ```
3. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

### Prerequisites

- CMake 3.22 or higher
- C++17 compatible compiler (Clang, GCC, or MSVC)
- JUCE 8.0+ (will be automatically fetched if not available locally)

### Building

```bash
# Using Makefile (recommended)
make build-release

# Or using CMake directly
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Using Local JUCE

By default CMake downloads the pinned JUCE release. To build against a local JUCE checkout instead:

```bash
cmake -S . -B build -G Ninja -DPLUGIN_ANALYSER_JUCE_SOURCE_DIR=/path/to/JUCE
```

## Code Style

We use `clang-format` for code formatting. Please ensure your code is formatted before submitting:

```bash
make format          # Format all code
make check-format    # Check formatting without changes
```

### Pre-commit Hooks

Install pre-commit hooks to automatically format code:

```bash
make install
# or
pre-commit install
```

## Making Changes

1. **Write clear, focused commits**
   - Each commit should represent a logical change
   - Write descriptive commit messages

2. **Follow the existing code style**
   - Use the same naming conventions
   - Match the existing code structure
   - Add comments for complex logic

3. **Test your changes**
   - Build the project successfully
   - Test the GUI application if you modified UI code
   - Test the CLI tool if you modified core functionality

## Submitting Changes

1. **Push your branch** to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```

2. **Create a Pull Request** on GitHub
   - Provide a clear description of your changes
   - Reference any related issues
   - Include screenshots if UI changes were made

3. **Ensure CI passes**
   - All GitHub Actions checks must pass
   - Fix any linting or formatting issues

## Pull Request Guidelines

- **Keep PRs focused**: One feature or bug fix per PR
- **Write clear descriptions**: Explain what and why
- **Update documentation**: If you add features, update the README
- **Add tests if applicable**: Help maintain code quality

## Code Review Process

- Maintainers will review your PR
- Address any feedback or requested changes
- Once approved, your PR will be merged

## Reporting Issues

Found a bug or have a feature request? Please open an issue with:

- **Clear description** of the problem or feature
- **Steps to reproduce** (for bugs)
- **Expected vs actual behavior**
- **Environment details** (OS, compiler version, etc.)
- **Screenshots** if applicable

## Development Areas

We welcome contributions in these areas:

- 🐛 Bug fixes
- ✨ New features
- 📚 Documentation improvements
- 🎨 UI/UX enhancements
- ⚡ Performance optimizations
- 🧪 Test coverage
- 🔧 Build system improvements

## Questions?

Feel free to open an issue for questions or discussions about the project.

Thank you for contributing! 🙏
