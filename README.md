# Jiangtun (江豚) — RP2350 fork

GC automation alternative firmware for Raspberry Pi Pico / Pico 2 / XIAO RP2040 / XIAO RP2350.

This is a fork of [u1f992/jiangtun](https://github.com/u1f992/jiangtun) (v2.0.0-alpha) with RP2350 support added.

- [NX Macro Controller](https://blog.bzl-web.com/entry/2020/01/20/165719)
- [Poke-Controller Modified](https://github.com/Moi-poke/Poke-Controller-Modified)
- [ORCA GC Controller](https://github.com/yatsuna827/Orca-GC-Controller) (experimental)

## Supported boards

| env | Board | Note |
|---|---|---|
| `pico` | Raspberry Pi Pico (RP2040) | |
| `dol-pico` | Raspberry Pi Pico (RP2040) | DOL enabled |
| `xiao-rp2040` | Seeed XIAO RP2040 | |
| `dol-xiao-rp2040` | Seeed XIAO RP2040 | DOL enabled |
| `pico2` | Raspberry Pi Pico 2 (RP2350) | |
| `dol-pico2` | Raspberry Pi Pico 2 (RP2350) | DOL enabled |
| `xiao-rp2350` | Seeed XIAO RP2350 | |
| `dol-xiao-rp2350` | Seeed XIAO RP2350 | DOL enabled |

## Pin assignment

| Signal | Pico / Pico 2 | XIAO RP2040 / RP2350 |
|---|---|---|
| GameCube | `GPIO5` | `D5` |
| Servo | `GPIO6` | `D6` |
| Reset | `GPIO3` | `D10` |

([WHALE](https://github.com/mizuyoukanao/Bluewhale) compatible)

## Button mapping

| NXMC2/PokeCon | GameCube |
| :-----------: | :------: |
|       Y       |    Y     |
|       B       |    B     |
|       A       |    A     |
|       X       |    X     |
|       L       |    L     |
|       R       |    R     |
|      ZL       |  (none)  |
|      ZR       |    Z     |
|       -       |  (none)  |
|       +       |  Start   |
|    L Click    |  (none)  |
|    R Click    |  (none)  |
|     Home      |  Reset   |
|    Capture    |  (none)  |

## Changes from upstream

- Added RP2350 board support (`pico2`, `dol-pico2`, `xiao-rp2350`, `dol-xiao-rp2350`)
- Added `JIANGTUN_CONFIG_BOARD_XIAO_RP2350` pin definitions and LED handling
- Fixed mutex deadlock in `loop1()`: `gamecube.write()` (which blocks ~6ms waiting for console poll) is now called outside the mutex, with rumble state written back separately
- Refactored `platformio.ini`: moved common settings to `[env]` base section

## Development

This directory is a [PlatformIO](https://platformio.org/) project. Clone with `--recursive` as it contains submodules. Note [the path character length limit in Git for Windows](https://arduino-pico.readthedocs.io/en/latest/platformio.html#important-steps-for-windows-users-before-installing).

```
git config --system core.longpaths true
```

## License

GPL v3. See [LICENSE](LICENSE).

## Reference

- [GC自動化ver3アップデート](https://note.com/gamewagashi/n/n026a29d00a85)
