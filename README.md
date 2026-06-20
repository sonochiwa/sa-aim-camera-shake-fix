# SA Aim Camera Shake Fix

`AimCameraShakeFix.asi` is a standalone GTA San Andreas ASI plugin that removes
the high-FPS camera shake/jitter that can happen while aiming weapons on foot.

The fix does not cap FPS. It temporarily raises the GTA SA camera timestep only
while the game processes aim-camera code, then restores the original timestep
immediately.

## Features

- Fixes aim-camera shake at high FPS.
- Does not add ads, telemetry, network access or popups.
- Does not replace game files.
- Small standalone ASI plugin.
- Designed for GTA San Andreas 1.0 US and SA-MP installs based on it.

## Demo

https://www.youtube.com/watch?v=bCkV8ltsS-M

## Installation

1. Install an ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.
2. Download `AimCameraShakeFix.asi` from the GitHub Releases page.
3. Copy `AimCameraShakeFix.asi` to the GTA San Andreas game folder.
4. Start the game and aim with an on-foot weapon at high FPS.

To uninstall, delete `AimCameraShakeFix.asi`.

## Compatibility

Test target:

- GTA San Andreas 1.0 US
- SA-MP clients that use the same `gta_sa.exe`

The plugin checks the game image base before installing hooks. Other executable
versions are not enabled by default.

## Source

The repository contains the source code and documentation. Compiled `.asi`
builds are published separately through GitHub Releases.

## Technical Notes

Hooks:

- `CCamera::Process @ 0x52B730`
- `CCam::Process_AimWeapon @ 0x521500`

Timer globals:

- `CTimer::ms_fTimeStep @ 0xB7CB5C`
- `CTimer::ms_fTimeStepNonClipped @ 0xB7CB58`

## Author

Created by sonochiwa.

If this fix helped you, consider starring the repository and sharing a before /
after video with a link to the original release page.
