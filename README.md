# Side-Chain

A free, open-source sidechain plugin that lets your main signal respond to a separate sidechain input. Built for beginners and new mixing engineers.

## Why
Sidechaining keeps vocals clear over busy mixes, tightens bass against kick, and creates pumping effects. The dedicated tools for this are paid, and open-source compressors are hard for newcomers. Side-Chain is simple, free, and open.

## Features
- Main input responds to an external sidechain input
- [Add your controls, e.g. Amount, Attack, Release, Mix]
- Simple, beginner-friendly interface

## Download
Get the latest build from [Releases](../../releases). [Format: VST3 / Windows]

## Build from source
```powershell
cmake -B build
cmake --build build --config Release
```
Requires CMake, a C++ compiler, and [JUCE].

## Usage
1. Load Side-Chain on the track you want to affect.
2. Route another track (e.g. kick) into its sidechain input in your DAW.
3. Adjust [controls] to taste.

## Contributing
Issues and pull requests are welcome.

## License
GPL-3.0. See [LICENSE](LICENSE).
