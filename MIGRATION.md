# Repository separation

Copied all 109 tracked TPTracker files from the 0.4.24 source snapshot into this repository's root. Source/assets, tests, developer generators, artwork attribution, research and audit documentation are retained. No saves, seed logs, full game sources, texture packs or build caches were copied into tracked content.

The prior Dusklight repository is preserved. The official TwilitRealm/dusklight repository supplies the external SDK at a pinned revision with identical SDK/header files. The new project does not depend on the old tracker fork. The Randomizer merge tool is fetched externally for package assembly. The latest verified distribution package is in ignored dist/. Future edits belong here.

Validation: configure with DUSKLIGHT_SOURCE_DIR pointing to an existing checkout, build the native package, run CTest and tests/asset_tests.py. CI performs the same work from pinned fetched dependencies for seven platforms.
