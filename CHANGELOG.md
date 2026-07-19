# Changelog

## 1.2.0 — 2026-07-20

### Fixed

- The fix no longer silently deactivates after entering/exiting a vehicle.
  On-foot detection now uses the `bInVehicle` ped flag (`CPed+0x46D`) instead
  of the `CPed::m_pVehicle` pointer, which the game can leave set after the
  ped exits a vehicle.
- Removed the swim-state check that read an undocumented `CPed` field
  (`0x53C`) and could permanently disable the fix after certain player
  actions.

## 1.1.0 — 2026-07-05

### Changed

- Raised the minimum aim timestep from 60 FPS units to 50 FPS units.
- Aim-camera detection now checks the queued player weapon mode and scans all
  three `CCam` slots instead of only the active one.

## 1.0.0 — 2026-06-20

- Initial release.
