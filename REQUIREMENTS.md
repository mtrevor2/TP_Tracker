# TPTracker requirements and installation

This GameBanana v6.5 bundle contains TPTracker 0.4.33.

## Required

- A compatible Dusklight installation with native mod support and the Dusklight Randomizer enabled.
- Your own game data configured in Dusklight.
- A randomizer save and its matching seed settings. Automatic seed selection uses the selected save's randomizer sidecar; save a new seed once, or select its log manually in TPTracker's options.

## Platforms

The single multiplatform `.dusk` includes Windows x64, Linux x64/ARM64, macOS Intel/Apple Silicon, Android ARM64 and iOS ARM64 binaries. Native Steam Deck/SteamOS uses Linux x64; running Windows Dusklight through Proton uses the Windows binary. Use a matching native Dusklight host. Mobile binaries have build validation but still require device testing; iOS native loading requires compatible app signing/bundling.

## Install or update

1. Extract the ZIP and install `TPTracker-0.4.33-multiplatform.dusk` through Dusklight's mod installation workflow, or place it in Dusklight's `mods` directory.
2. Keep only one active TPTracker package; move the older version outside the `mods` directory before enabling the update.
3. Restart Dusklight and enable TPTracker in its Mods menu. Assign a keyboard/controller shortcut in TPTracker's options if desired.

README.md describes the features and changes. Python, a web browser and an internet connection are not required during gameplay. Collection guides are bundled offline.

## Current limits and validation

- Availability uses the seed settings, tracked inventory and vanilla entrance connections. Shuffled entrance routing is not evaluated.
- Unknown state remains UNKNOWN; the private live Progressive Sky Book character count is still unsupported. Some unmapped scripted rewards remain checklist-only.
- Overworld maps, dungeon maps and minimaps are implemented. Controller input, marker alignment and device-specific behavior still require gameplay testing beyond the automated build/model/asset checks.
- Notes and skipped checks are stored with the selected save. Save your game to retain them.
- The tracker does not modify game progression or randomized item placements. It cannot repair a damaged Randomizer save.
- The tracker window uses Dusklight's input focus and cannot detach to another monitor.

Regression coverage for this release includes held/delivered Auru's Memo, desert access, the Forest Temple second-monkey door with available/spent keys, unrelated gate isolation, and the existing cave, warp, notes and marker cases.
