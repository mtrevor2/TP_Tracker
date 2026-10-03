# Reported keysanity crash and subsequent load failure

Evidence reviewed: dusklight-20260923-173346.log, the two supplied error screenshots, Dusklight v2.0.1 source (422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7), and the current TPTracker save code.

## What the supplied log establishes

The log lists only Randomizer 1.0.4, Cosmetics, and Luau Support. TPTracker is absent. Lines 782–784 report `no seed_hash found for file 0`, `seed_hash not found!`, and the Randomizer's save-loaded callback failure. Randomizer session.cpp returns MOD_ERROR when its saved seed_hash blob is unavailable or empty. This explains the repeatable reload error even after uninstalling TPTracker. It does not establish why the metadata is missing.

The supplied log is a later session, not a crash dump from the original laptop failure. It does not show a keysanity exception or establish corruption of the actual game save. The initial crash and any possible earlier involvement by TPTracker cannot be conclusively ruled out from this evidence alone.

## Tracker save audit

TPTracker stores its journal through the host SaveService using its own ModContext. SaveService indexes blobs by the calling mod's metadata ID, so normal tracker journal writes do not address Randomizer's seed_hash namespace. Tracker seed discovery reads the Randomizer sidecar and seed files. Its card backup routine only copies the live card and sidecar to a separate backup directory; it does not overwrite the source. No tracker write to the Randomizer seed_hash or game key counters was found in these paths.

No speculative change to the game save or Randomizer metadata has been made. Removing the seed-hash check would allow a save to run with an unidentified seed and is not a recovery method.

## Recovery evidence needed

Preserve the affected user's entire Dusklight data directory before changing anything. Obtain a save export including mod data (.dusksave), the matching Randomizer seed folder, and any original crash log/dump. The Randomizer mod-data sidecar is named dev.twilitrealm.randomizer.json alongside the card's mod data. Compare slot 0 against an intact backup of the same save and seed. Do not invent a hash or substitute another seed.

If TPTracker created backups in the affected installation, check mod_data/com.mikey022.tp_randomizer_tracker/card-backups. Backups are not guaranteed to exist, and the newest backup may already contain the missing metadata. Recovery should restore a verified matching game save and its mod data together. Reinstalling application files alone does not restore missing save metadata.

## Oct 3, 2026: report after tracker updates/window close/data relocation

This is a separate report from the logged September incident above. The new evidence is a user's account of repeated load failures after updating/reverting TPTracker, closing Dusklight with the window X, and relocating game data. A seed spoiler log was subsequently supplied for logic checks, but no failure log, error text, affected card, sidecars, or crash dump was provided for this report. A spoiler log cannot establish save corruption.

The current code review found:

- TPTracker's only save mutation is `SaveService::set_blob` under `com.mikey022.tp_randomizer_tracker`. Host `current_blobs` uses the caller's metadata ID; Randomizer's `seed_hash` belongs to `dev.twilitrealm.randomizer`. `set_blob` changes an in-memory namespace; the host writes sidecars after a game-save event with a temporary file and atomic replacement.
- `loadSelectedSeed` opens Randomizer's sidecar/log/seed files for reading. `backupSelectedCard` copies the card and matching sidecars to a new backup directory, refuses overwrite and never writes to the source card. New diagnostics report unreadable seed paths without rewriting or repairing them.
- The inspected host's normal game-loop exit calls mod shutdown. TPTracker closes its UI, removes hooks/observers and waits for its isolated logic worker; its shutdown does not write the card or change Randomizer seed data. The host's GCI implementation also uses atomic file writes. Atomic individual files do not establish transaction safety across every game/sidecar write or every older host build.
- Randomizer's save-loaded callback fails when its selected slot lacks `seed_hash`; seed activation also depends on a matching seed directory. Moving only the game card or changing the data root without those companion files can therefore cause a load failure that reinstalling TPTracker will not repair. This is a plausible explanation, not a diagnosis of this user's failure.

No evidence currently establishes that TPTracker corrupted this save, and this review cannot exclude an unobserved crash, host/version issue, incomplete save, permissions problem, or missing/mismatched metadata. No speculative game-save repair was implemented. Normal window-X closure after a completed in-game save should not require a special tracker exit sequence; it does not save unfinished game progress automatically.

For diagnosis, obtain the Dusklight version, exact error and log from the first failed reload, a `.dusksave` export including mod data, the matching seed folder, and the old/new data paths. Preserve both original data directories and available backups before recovery. Restore only matching save/sidecar/seed data; never fabricate a seed hash or use another seed's metadata.
