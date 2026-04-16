# !0fastload

`!0fastload` — нативный **ASI**-плагин для **GTA San Andreas 1.0 US**, который применяет fastload-патчи при старте процесса.

Плагин рассчитан на использование:

- в одиночной игре;
- при запуске **SA-MP**.

Он **не** зависит от **SAMPFUNCS** и **не** патчит `samp.dll` напрямую.

## Возможности

- пропуск стандартного пути со стартовыми экранами и загрузочным экраном (fade / splash / bar);
- отключение загрузочного аудио при старте;
- симуляция прохождения copyright-цепочки без полного ожидания;
- те же fastload-патчи в одиночной игре и в **SA-MP**;
- в режиме **SA-MP** — дополнительный набор патчей против залипаний focus / меню / паузы на старте;
- совместимость с **portablegta** (после патчей вызывается инициализация userfiles через `0x744FB0`, если загружен `portablegta.asi`).

## Совместимость

- игра: **GTA San Andreas 1.0 US** (жёстко зашитые оффсеты);
- формат: **ASI**;
- архитектура: **x86 (Win32)**.

## Установка

Скопируйте `!0fastload.asi` в корень игры рядом с `gta_sa.exe` (или в каталог, откуда подхватывает ваш ASI-loader). Автоматически копировать артефакт из этого репозитория в папку игры не требуется — сборка кладёт файл только в `build\Release\`.

## Сборка

Требования:

- **Visual Studio 2022** (или Build Tools) с **Desktop development with C++**, toolset **v145**;
- целевая платформа **Win32**.

Сборка solution (удобно, если в `.sln` платформа называется `x86`):

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' '.\!0fastload.sln' /t:Build /p:Configuration=Release /p:Platform=x86
```

Сборка напрямую из проекта:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' '.\!0fastload.vcxproj' /t:Build /p:Configuration=Release /p:Platform=Win32
```

Выходной файл (единственный каноничный артефакт репозитория):

```text
build\Release\!0fastload.asi
```

## Структура репозитория

| Путь | Назначение |
|------|------------|
| `source/main.cpp` | логика патчей и `DllMain` |
| `source/!0fastload.rc` | `VERSIONINFO` для уменьшения ложных срабатываний AV |
| `!0fastload.vcxproj` | проект Visual Studio |
| `!0fastload.sln` | solution |
| `!0fastload.lua` | legacy MoonLoader-скрипт в корне **отключён** (патчинг только через ASI) |
| `lua/!0fastload.lua` | копия для MoonLoader из каталога `moonloader` (при необходимости) |
| `reference/` | референсы (fastloader, imfast, portablegta, gtasa_crashfix и др.) |
| `context.md` | контекст для разработки |
| `.claude/work.md` | рабочий журнал изменений |

## Примечания

- **SA-MP** определяется по наличию `samp.dll` или аргументам командной строки `-c`, `-h`, `-p`.
- Плагин отвечает только за **fastload** на старте; автосохранение и автозагрузка сейвов **не** реализованы.
- Эвристики антивирусов на runtime-патчинг возможны; для публичных релизов полезны **подпись** бинарника и репутация/whitelisting.

Репозиторий на GitHub: [dmitriyewich/-0fastload](https://github.com/dmitriyewich/-0fastload).
