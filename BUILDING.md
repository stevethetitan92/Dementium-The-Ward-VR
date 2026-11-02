# Building the source

## Requirements

- Windows x64.
- Visual Studio 2022 with Desktop development with C++, the Windows SDK, and MASM support.
- Windows PowerShell 5.1 (included with Windows).

Open **x64 Native Tools Command Prompt for VS 2022**, change to this repository folder, then run:

```cmd
build-source.cmd
```

This expands the generated UI headers, compiles MinHook and the assembly entry shim, then builds `bin\DementiumOpenXR.dll` and `bin\attach.exe`. The helper and bridge are built from the supplied sources. OpenXR is loaded dynamically, so the loader DLL is not needed to compile. Its upstream version and license are documented in `third_party/openxr-loader/`.

## Use a locally built DLL

Close the game. Extract a release package into a separate test folder and keep a backup of that folder. Replace `Mod Files\DementiumOpenXR.dll` and `Mod Files\attach.exe` with your build outputs. Keep the package's assets, OpenXR loader and configuration. Use its SteamVR or Virtual Desktop launcher. Do not copy game binaries or saves into the source repository.

The `launcher/` directory contains the current development launcher scripts. Updating them in a package also requires regenerating its recovery manifest; changing individual files can otherwise trigger integrity recovery. The source build intentionally does not publish or modify a release package automatically.

## Focused offline checks

Run `test-source.cmd` from the same x64 developer prompt. This executes selected self-contained tests and checks the built DLL's initialization export. It does not launch the game or establish headset correctness.

The remaining `tests/` files are retained for inspection and development. Tests referring to `tests/fixtures/`, game archives, extracted assets or an installed game require those inputs separately. Those files are not distributed here. Do not run every historical test indiscriminately against personal saves. Offline authoring tools also require their documented local models, images, fonts and Python dependencies; they are not required to compile the DLL.

## Supported executable assumptions

Native hooks are version/signature-sensitive; see the guards in the source. This snapshot was developed against the Windows x64 PC release. A different game executable must not be treated as supported merely because the project compiles.
