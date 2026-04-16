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

1. Соберите плагин (см. ниже) или возьмите готовый `!0fastload.asi` из релиза на GitHub.
2. Положите `!0fastload.asi` рядом с `gta_sa.exe` (или в папку, которую сканирует ваш ASI-loader).

Готовый файл после сборки лежит здесь: `build\Release\!0fastload.asi`.

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

Репозиторий на GitHub хранит только основной код и профиль сборки:

| Путь | Назначение |
|------|------------|
| `source/main.cpp` | логика fastload-патчей и `DllMain` |
| `source/!0fastload.rc` | `VERSIONINFO` ресурсы PE |
| `!0fastload.vcxproj` | проект Visual Studio |
| `!0fastload.sln` | solution Visual Studio |
| `README.md` | текущая документация |

## Примечания

- **SA-MP** определяется по наличию `samp.dll` или аргументам командной строки `-c`, `-h`, `-p`.
- Плагин только ускоряет старт игры; автосохранение и автозагрузка сейвов **не** делаются.

### Если антивирус ругается

Некоторые антивирусы могут помечать любые **ASI**-моды как подозрительные. Это нормально для таких файлов. Что можно сделать:

- скачивайте `!0fastload.asi` только из [официального репозитория](https://github.com/dmitriyewich/-0fastload) или из **Releases**;
- добавьте папку с игрой или сам файл `!0fastload.asi` в **исключения** антивируса;
- если файл уже удалили или отправили в карантин — восстановите его и разрешите использование.

Исходники и релизы: [https://github.com/dmitriyewich/-0fastload](https://github.com/dmitriyewich/-0fastload).
