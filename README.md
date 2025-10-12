# IDA Patcher

Binary patcher for IDA Pro 9.2+ that applies hotfixes in memory.

## Installation

Download the latest release from [releases](https://github.com/Mewski/ida-patcher/releases) and copy the plugin to your IDA `plugins` directory:
- **Windows**: `ida-patcher.dll`
- **Linux**: `ida-patcher.so`
- **macOS**: `ida-patcher.dylib`

Or build from source:

**Windows:**
```bash
git clone --recursive https://github.com/Mewski/ida-patcher.git
cd ida-patcher
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**macOS (universal binary):**
```bash
git clone --recursive https://github.com/Mewski/ida-patcher.git
cd ida-patcher
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build
```

**Linux:**
```bash
git clone --recursive https://github.com/Mewski/ida-patcher.git
cd ida-patcher
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

Create `ida-patcher.json` in your IDA Pro `plugins` directory:

```json
[
  {
    "name": "example_patch",
    "enabled": false,
    "modules": ["ida.dll", "libida.so", "libida.dylib"],
    "search": "DE AD ?? ?? BE EF",
    "replace": "DE AD ?? ?? C0 DE"
  }
]
```

Patches apply automatically when IDA Pro starts. Check the output window for status messages.

## Pattern Syntax

- `48 8B` - Match/write exact bytes
- `??` - Match any byte (search) or preserve original (replace)
- `A?` or `?B` - Match/preserve individual nibbles

## Examples

Check [`examples/`](examples/) for patch configuration examples.

## References

[mrexodia/idapatch](https://github.com/mrexodia/idapatch)

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
