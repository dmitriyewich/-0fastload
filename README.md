# !0fastload

`!0fastload` is a native `ASI` plugin for `GTA San Andreas 1.0 US` that applies fast startup patches as early as possible.

The plugin is intended for both:
- single-player
- `SA-MP` startup

It does not depend on `SAMPFUNCS` and does not patch `samp.dll` directly.

## Features

- skips the standard startup/loading screen path
- disables loading audio on startup
- keeps fastload behavior in both single-player and `SA-MP`
- adds a conservative `SA-MP` startup-safe patch set for focus/menu pause behavior

## Compatibility

- game: `GTA San Andreas 1.0 US`
- plugin format: `ASI`
- architecture: `x86 / Win32`

## Installation

Copy `!0fastload.asi` to the game root folder next to `gta_sa.exe`.

## Build

Requirements:
- Visual Studio 2022 with C++ toolset
- target platform `Win32`

Build command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' '.\!0fastload.vcxproj' '/t:Build' '/p:Configuration=Release;Platform=Win32'
```

Output file:

```text
build\Release\Win32\!0fastload.asi
```

## Repository Layout

- `source/main.cpp` - plugin source
- `!0fastload.vcxproj` - Visual Studio project
- `!0fastload.sln` - Visual Studio solution
- `publish/` - publication-ready files

## Notes

- `SA-MP` detection is based on `samp.dll` presence or standard launch arguments.
- The plugin focuses on startup fastload behavior only.
- Autosave/autoload logic is intentionally not included.
