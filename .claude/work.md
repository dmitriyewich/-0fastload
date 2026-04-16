## 2026-04-16

- Fixed portablegta compatibility in `source/main.cpp` by detecting `portablegta.asi` and forcing one call through SA userfiles getter (`0x744FB0`) after fast-load patches are applied.
- Reduced script AV risk by disabling legacy memory-patching logic in `!0fastload.lua`; runtime patching is now delegated to `!0fastload.asi` only.
- Verification attempt: build command for `Release|Win32` could not be executed in this environment because `msbuild`/`dotnet` are not installed in PATH.
- Added project `context.md` based on the requested template and aligned artifact path to `build\Release\!0fastload.asi`.
- Switched `Release|Win32` in `!0fastload.vcxproj` to an OpenSaa-like aggressive profile (`/GL`, max speed, SDL/GS/RTTI/exceptions tuned for release). During verification, removed `/Zl` + custom CRT startup overrides because project uses SEH (`__try/__except`) and requires CRT symbols.
- Added `source\!0fastload.rc` with VERSIONINFO metadata and included it in the project to improve binary reputation for AV heuristics.
- Verification build passed with explicit MSBuild path: `!0fastload.sln` built successfully for `Release|x86`; artifact produced at `build\Release\!0fastload.asi`.
- Performed size-focused refactor in `source/main.cpp`: removed redundant SEH wrappers in fixed-address splash callbacks and replaced repeated pointer-operand writes with a compact address loop.
- Rebuilt `Release|x86` and verified size change: `!0fastload.asi` reduced from `89600` bytes to `88576` bytes with unchanged startup behavior.
- Removed debug logging branch (`FASTLOAD_DEBUG_LOG`) and compacted helper logic in `source/main.cpp` (write/nop pipeline + shorter token parsing/runtime detection) to reduce source bloat while preserving behavior.
- Tuned `Release|Win32` profile for binary size (`/O1`, `FavorSizeOrSpeed=Size`, `/LTCG` non-incremental) and rebuilt successfully.
- Verified final artifact size: `build\Release\!0fastload.asi` is now `87552` bytes.
- Implemented CRT-less profile attempt aligned with OpenSaa style: removed remaining SEH wrappers, added custom minimal runtime shims (`_fltused`, local `memcpy/memset`), switched linker to `NODEFAULTLIB` with `DllMain@12` entrypoint and `kernel32.lib` only.
- Resolved `/GL` incompatibility for custom runtime symbols by disabling WPO/LTCG in this CRT-less profile (`WholeProgramOptimization=false`, `LinkTimeCodeGeneration=Default`).
- Verification build passed for `Release|x86`; resulting artifact size dropped to `build\Release\!0fastload.asi = 6144` bytes.
- Cloned reference repo `Whitetigerswt/gtasa_crashfix` into `reference\gtasa_crashfix` via `git clone` (`gh` was not available in PATH).
- Implemented fastloader-style **ShowRaster** hook at `0x619440`: verify `E9`, rewrite rel32 to `ShowRaster_Prox` (naked x86 stub), skip raster redraw when `gGameState` low byte `!= 9` after first draw (`g_showRasterDidDraw`). Wired into `ApplySinglePlayerPatches` (covers SA:MP path via `ApplySampPatches`). Rebuilt `Release|x86` successfully.
- Removed **ShowRaster** hook (`0x619440`) from `source/main.cpp` per user request; verified `Release|x86` rebuild.
