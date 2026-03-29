# !0fastload

Native ASI plugin for GTA San Andreas 1.0 US that applies fast-load startup patches immediately on launch.

## Contents

- `!0fastload.asi` - release build
- `source/main.cpp` - plugin source
- `!0fastload.vcxproj` - Visual Studio project
- `!0fastload.sln` - Visual Studio solution

## Build

Requirements:
- Visual Studio 2022 with C++ toolset
- Win32 target

Build command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' '.\!0fastload.vcxproj' '/t:Build' '/p:Configuration=Release;Platform=Win32'
```

Output:

```text
build\Release\Win32\!0fastload.asi
```

## Notes

- Target game version: `GTA San Andreas 1.0 US`
- Plugin format: `ASI`
- Architecture: `x86 / Win32`
- Some patches keep compatibility behavior for `sampfuncs.asi`
