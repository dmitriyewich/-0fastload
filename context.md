# Context

## Правила работы (обязательно)

- После каждого заметного изменения кода/логики/сборки обновлять `.claude/work.md` до ответа пользователю.
- Без явной команды пользователя не делать `git push`.
- После существенных правок делать проверочную сборку `Release|Win32` (или `Release|x86` из `.sln`).
- Не копировать `.asi` в папки игры автоматически.
- Единственное валидное расположение артефакта сборки: `build\Release\!0fastload.asi`.

## Назначение проекта

- `!0fastload` — нативный `ASI` для **GTA San Andreas 1.0 US** и **SA:MP** (определение по `samp.dll` и аргументам `-c/-h/-p`).
- Ускоряет старт: патчи цепочки `gGameState`, copyright/load screen, fade, splash, loading bar, загрузочное аудио.
- **portablegta**: если загружен `portablegta.asi` / `PortableGTA.asi`, после успешных патчей выполняется однократный вызов через `0x744FB0` (инициализация userfiles после хука portablegta).

## Целевая платформа

- `GTA San Andreas 1.0 US`.
- Только `Win32` (x86).

## Важные файлы

- Код: `C:\Games\CODEX\!0fastload\source\main.cpp`
- Версия PE: `C:\Games\CODEX\!0fastload\source\!0fastload.rc`
- Проект: `C:\Games\CODEX\!0fastload\!0fastload.vcxproj`
- Solution: `C:\Games\CODEX\!0fastload\!0fastload.sln`
- Legacy Lua (заглушка в корне): `C:\Games\CODEX\!0fastload\!0fastload.lua`
- MoonLoader-путь (из merge с GitHub): `C:\Games\CODEX\!0fastload\lua\!0fastload.lua`
- Журнал: `C:\Games\CODEX\!0fastload\.claude\work.md`
- Контекст: `C:\Games\CODEX\!0fastload\context.md`
- Референсы: `C:\Games\CODEX\!0fastload\reference\`

## Сборка

- MSBuild (пример пути):  
  `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`
- Из `.sln` (платформа `x86`):  
  `MSBuild.exe !0fastload.sln /p:Configuration=Release /p:Platform=x86`
- Из `.vcxproj` напрямую:  
  `MSBuild.exe !0fastload.vcxproj /p:Configuration=Release /p:Platform=Win32`
- **OutDir** в `.vcxproj`: `build\Release\` → артефакт `build\Release\!0fastload.asi`.

## Текущий профиль `Release|Win32`

- Компиляция: `Optimization=MinSpace` (`/O1`), `FavorSizeOrSpeed=Size` (`/Os`), `SDLCheck=false`, `BufferSecurityCheck=false`, `ExceptionHandling=false`, `RuntimeTypeInfo=false`, `DebugInformationFormat=None`.
- Линковка: **CRT-less** — `IgnoreAllDefaultLibraries=true`, `EntryPointSymbol=DllMain@12`, `AdditionalDependencies=kernel32.lib`; в `main.cpp` — минимальные shims `memcpy`/`memset` (`#pragma function`), `_fltused`, локальные `TinyMemcpy` / `TinyMemset` для `PatchWrite`.
- `WholeProgramOptimization=false`, `LinkTimeCodeGeneration=Default` (без `/GL` из-за совместимости с кастомным CRT-подобным линковочным набором).
- `EnableCOMDATFolding`, `OptimizeReferences` включены.
- Ресурс `VERSIONINFO` подключён через `source\!0fastload.rc`.

## Ключевые адреса (US 1.0)

- `gGameState`: `0xC8D4C0` (целевое значение для fastload: `5`).
- Отключение сброса состояния: NOP `0x747483` (6 байт).
- Copyright / load pipeline: `0x748CF6`, `0x748AA8`, `0x748C23`, `0x748C2B`, вызов `0x748C9A` → `SimulateCopyrightScreen`.
- Splash / fade: `0x590AE4` → `0x590C9E`, хук `0x590ADE` → `IncreaseDisplayedSplash`, операнды на `g_previousDisplayedSplash`, `0x590DA6` → `g_loadscreenTime`, NOP bar `0x5905B4`, load screen `0x590D9F`.
- SA:MP: `0x74805A`, `0x53BC78`, `0x561AF6`, NOP `0x748CBD` (2 байта).
- portablegta: вызов через `0x744FB0`.

## Антивирус

- Runtime `VirtualProtect` + запись в код провоцируют эвристики; Lua-патчер отключён; `VERSIONINFO` ослабляет часть FP. Для публичных сборок — **code signing** и обращения в vendor false-positive.

## Репозиторий

- GitHub: [https://github.com/dmitriyewich/-0fastload](https://github.com/dmitriyewich/-0fastload)
