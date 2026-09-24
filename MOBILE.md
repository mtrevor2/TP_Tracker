# Mobile builds

The platform workflow cross-compiles Android ARM64 (API 28+) and iOS ARM64 (iOS 16+) with the official Dusklight SDK stubs. Desktop tests execute on their native runners; mobile binaries are compile/package checks until tested on a device.

Android uses lib/android-aarch64/mod.so, the host's shared C++ runtime, and the same resources as desktop. It requires a compatible Android Dusklight host. No touchscreen-specific tracker shortcuts are added; use the mod menu and GUI controls.

iOS uses lib/ios-arm64/mod.so. This is an integration artifact, not a normally installable iOS mod: Dusklight's supports_native_installs() explicitly returns false on iOS. The native library and resources must be bundled and signed with the Dusklight application. The SDK's bundled-mod install path places resources in mods/<mod-id> and the native module in Frameworks/<mod-id>.so. App signing, bundling/prepatching, and device testing must be performed as part of the iOS host build. Merely importing the combined .dusk cannot enable native loading on iOS.
