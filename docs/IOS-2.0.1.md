# TPTracker 0.4.53: SUMEE / iOS Dusklight 2.0.1 integration candidate

This separate iOS compatibility target is intended for SUMEE's developer to
evaluate and bundle. It is **not a user-installable mod for the existing SUMEE
app**, and has not been tested against SUMEE's custom `2.0.1-0-dirty` binary.

It retains TPTracker's 0.4.53 features, builds against the official Dusklight
2.0.1 SDK, and omits an optional cursor-visibility hook absent from that
release's iOS executable. The host controls pointer visibility; controller
bindings and map inspection remain in the build. The normal multiplatform
release is unchanged.

## What players can do inside SUMEE

1. Open [SUMEE's App Store page](https://apps.apple.com/us/app/sumee/id6755858938)
   and tap **Update**, if offered. SUMEE's release notes include changes to its
   bundled Dusklight. There is no documented separate Dusklight updater in
   the sources reviewed. Updating SUMEE does not guarantee TPTracker is included.
2. To request native TPTracker support, use SUMEE's **Settings > Feedback &
   Reports** option (the developer refers to this in App Store responses).
   Suggested message:

   > Please include TPTracker as a bundled native mod in SUMEE's Dusklight.
   > My game displays Dusklight 2.0.1-0-dirty. An iOS arm64 integration
   > candidate is available, but it needs validation against your custom
   > Dusklight build, hook prepatching, and signing as part of SUMEE.

3. Wait for a SUMEE release that explicitly includes TPTracker. After installing
   that release, open Dusklight's **Mods > TPTracker**, enable it, and configure
   the tracker/controller shortcuts. This last step is conditional on SUMEE's
   developer completing the integration; it is not available in the current app
   merely by downloading this package.

**Copying the `.dusk` into Files/mods cannot install native code through the
standard iOS loader.** Dusklight requires native libraries to be bundled and
signed with the app. The `-dirty` version suffix means local source changes
were present; it does not identify those changes or prove compatibility.

## Developer integration

- Base SDK: `TwilitRealm/dusklight` commit
  `422d7bb1b6c8d973cccf8b3d0b226a57ac3cc8c7` (`v2.0.1`).
- CMake option: `-DTPTRACKER_IOS_2_0_1=ON`, with an arm64 iOS toolchain and
  `-DCMAKE_OSX_DEPLOYMENT_TARGET=16.0`.
- Workflow: `.github/workflows/tptracker-ios-2.0.1.yml`.
- Package: `TPTracker-0.4.53-ios-dusklight-2.0.1.dusk` in `dist`.
- Native mod ID: `com.mikey022.tp_randomizer_tracker`.

Build from this repository using the exact SDK above and the workflow's build
options. The package contains `mod.json`, resources, and
`lib/ios-arm64/mod.so`. For an upstream-style app bundle:

**Host build prerequisite:** The official 2.0.1 IPA resolves all 86 executable
imports and all 13 hook declarations in this candidate, but fails the hook
patchability audit for three functions: `should_capture_mouse`,
`dusk::mods::svc::window_dispatch_event`, and `dusk::ui::update`. They are
compiled in `dusk_internal`, which 2.0.1 omits from its patchable-entry compiler
options. The existing official IPA cannot simply be repackaged with this mod.

`compatibility/ios-2.0.1-dusk-internal-hooks.patch` adds the existing compiler
option to that target on iOS. The patch applies cleanly to upstream 2.0.1;
the resulting host has not been rebuilt or device-tested here. SUMEE's developer
must apply/adapt this change to the actual host sources, rebuild the host,
then rerun the audit before bundling and signing.

1. Put the manifest and resources under
   `<App>.app/mods/com.mikey022.tp_randomizer_tracker/`.
2. Put the native library at
   `<App>.app/Frameworks/com.mikey022.tp_randomizer_tracker.so`.
3. Verify every native executable import and service version against the
   actual SUMEE Dusklight host. Adapt the paths/loading mechanism as needed
   for SUMEE's embedded integration.
4. Prepare the hook gateways for TPTracker and all existing bundled mods using
   the host's build-time native-mod pipeline. Upstream uses **symgen 1.3.6**:

   ```text
   symgen prepatch --binary <actual-host-executable> --report prepatch.json <all-bundled-native-mod-libraries>
   ```

   The upstream tool requires a thin arm64 executable with its embedded symbol
   manifest. If SUMEE embeds the game differently, use/adapt SUMEE's own host
   build pipeline; do not patch or replace its host with an official executable.
5. Sign the native libraries and app as part of the normal SUMEE release build.
6. Test on a physical iOS device: launch, enabling/disabling TPTracker, paused
   menu/controller binding capture, A/Cross map inspection, stacked-check tabs,
   map placement, seed/log import, and collection updates in normal/randomizer play.

The minimum iOS version of this candidate is 16.0. An audit against the official
2.0.1 IPA is a reference check only. The actual SUMEE host binary or source is
needed to finish compatibility work for `2.0.1-0-dirty`. The public SUMEE Lite
repository is a different project and does not document the full app's
Dusklight integration. The developer's public Dusklight fork was also checked:
its `ios/external-display` branch at `00d5a645a4` predates upstream 2.0.1 and
does not identify the `2.0.1-0-dirty` sources currently embedded in SUMEE.

`tools/check_ios_compatibility.py` verifies the package's explicit game imports,
compares its native service requirements with its manifest, and invokes
`symgen prepatch --check` without changing the executable:

```text
python tools/check_ios_compatibility.py --package dist/TPTracker-0.4.53-ios-dusklight-2.0.1.dusk --host <actual-host-executable> --symgen <symgen-1.3.6> --report <audit.json>
```

A successful audit verifies symbol availability and hook patchability only.
Service implementations, C++ layouts, SUMEE's loading/signing integration,
and physical-device behavior still need validation. The report deliberately
keeps device testing and SUMEE compatibility marked unverified.

## Sources

- [SUMEE App Store listing, update history and developer support responses](https://apps.apple.com/us/app/sumee/id6755858938)
- [SUMEE Lite source](https://github.com/getzemani63/sumee-lite)
- [Developer's public Dusklight fork](https://github.com/getzemani63/dusklight/tree/ios/external-display)
- [Official Dusklight 2.0.1 release](https://github.com/TwilitRealm/dusklight/releases/tag/v2.0.1)
- [iOS native-mod installation restriction](https://github.com/TwilitRealm/dusklight/blob/v2.0.1/src/dusk/mods/catalog.cpp)
- [iOS mod bundling and prepatching](https://github.com/TwilitRealm/dusklight/blob/v2.0.1/cmake/ModSDK.cmake)
- [symgen 1.3.6 prepatch documentation](https://github.com/encounter/symgen/blob/v1.3.6/README.md#prepatch)
