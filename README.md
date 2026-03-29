# !0fastload

Нативный `ASI`-плагин для `GTA San Andreas 1.0 US`, который применяет fast-load патчи сразу при запуске игры.

## Содержимое

- `!0fastload.asi` - релизная сборка
- `source/main.cpp` - исходный код плагина
- `!0fastload.vcxproj` - проект Visual Studio
- `!0fastload.sln` - solution Visual Studio

## Сборка

Требования:
- Visual Studio 2022 с установленным C++ toolset
- целевая платформа `Win32`

Команда сборки:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' '.\!0fastload.vcxproj' '/t:Build' '/p:Configuration=Release;Platform=Win32'
```

Выходной файл:

```text
build\Release\Win32\!0fastload.asi
```

## Примечания

- Целевая версия игры: `GTA San Andreas 1.0 US`
- Формат плагина: `ASI`
- Архитектура: `x86 / Win32`
- Часть патчей сохраняет совместимость с `sampfuncs.asi`
