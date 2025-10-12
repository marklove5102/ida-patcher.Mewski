# IDA Patcher

Binary patcher for IDA Pro 9.2+ that applies hotfixes in memory.

## Installation

Download the latest release from [releases](https://github.com/Mewski/ida-patcher/releases) and copy `ida-patcher.dll` to your IDA `plugins` directory.

Or build from source:

```bash
git clone --recursive https://github.com/Mewski/ida-patcher.git
cd ida-patcher
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

## Usage

Create `ida-patcher.json` in your IDA Pro `plugins` directory:

```json
[
  {
    "name": "example_patch",
    "enabled": false,
    "modules": ["ida64.exe"],
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

- [mrexodia/idapatch](https://github.com/mrexodia/idapatch)

## License

MIT - see [LICENSE](LICENSE)
