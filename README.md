# !0fastload

`!0fastload` это нативный `ASI`-плагин для `GTA San Andreas 1.0 US`, который применяет fastload-патчи как можно раньше при запуске игры.

Плагин рассчитан на использование:
- в одиночной игре
- при запуске `SA-MP`

Он не зависит от `SAMPFUNCS` и не патчит `samp.dll` напрямую.

## Возможности

- пропускает стандартный путь со стартовыми экранами и загрузочным экраном
- отключает загрузочное аудио при старте
- сохраняет fastload-поведение и в одиночной игре, и в `SA-MP`
- добавляет осторожный набор `SA-MP`-совместимых патчей против focus/menu pause-залипаний на старте

## Совместимость

- игра: `GTA San Andreas 1.0 US`
- формат плагина: `ASI`
- архитектура: `x86 / Win32`

## Установка

Скопируйте `!0fastload.asi` в корневую папку игры рядом с `gta_sa.exe`.

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

## Структура Репозитория

- `source/main.cpp` - исходный код плагина
- `!0fastload.vcxproj` - проект Visual Studio
- `!0fastload.sln` - solution Visual Studio
- `publish/` - готовые файлы для публикации

## Примечания

- `SA-MP` определяется по наличию `samp.dll` или стандартных аргументов запуска.
- Плагин отвечает только за fastload-логику старта.
- Логика автосохранения и автозагрузки специально не включена.
