# Dementium: The Ward VR

PC VR mod for Dementium: The Ward, using Direct3D 12 interception, OpenXR and tracked motion controls. Requires a separately purchased copy of the original game.

- [Download the mod](https://github.com/stevethetitan92/Dementium-The-Ward-VR/releases)
- [Report a bug](https://github.com/stevethetitan92/Dementium-The-Ward-VR/issues)
- [Build from source](BUILDING.md)

## Download the source

[Download the complete source snapshot](Dementium-VR-source-2026-11-01.zip?raw=true), then extract it to its own folder. The archive contains the C++ source, launcher scripts, tests, dependency notices and build scripts described below. The folders below are inside that source ZIP.

The independent source build produced both the bridge DLL and launcher helper successfully; all six selected standalone checks passed. This is compilation/offline validation, not an additional gameplay test.

## Source layout

- `src/bridge.cpp`: bridge initialization and graphics interception.
- `src/attach.cpp`: launcher helper and remote initialization reporting.
- `src/xr_session.hpp`, `src/stereo.hpp`: OpenXR and stereo rendering.
- `src/arsenal_runtime.hpp`: weapon interactions and combat integration.
- `src/save_slot_files.hpp` and related save modules: save routing and slot management.
- `launcher/`: game discovery, runtime selection, launch diagnostics and recovery.
- `tests/`: focused C++ regression checks. Some require separately obtained game files or local diagnostic fixtures; see BUILDING.md.
- `tools/`: offline asset-authoring and analysis scripts.
- `include/`, `third_party/`: build dependencies and their notices.

This is a development source snapshot, not a declaration that it is byte-for-byte identical to any previously uploaded release DLL. Publishing this snapshot does not replace the release download. No game executable, game archive, user saves, crash dumps or personal logs are included.

Generated embedded UI pixel headers are stored losslessly in `generated-ui-headers.zip` to keep the source checkout manageable. The build expands them into `src/`. They contain the mod's UI resources, rather than executable code. Runtime meshes, textures, audio and configuration remain in the separately downloaded mod package.

Third-party notices are retained with their respective dependencies. No new blanket license is granted over the original game or third-party assets by publishing this repository.
