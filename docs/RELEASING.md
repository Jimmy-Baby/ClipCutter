# Local Windows release process

ClipCutter releases are built, tested, deployed, packaged, and verified locally. No GitHub Actions or other hosted CI workflow participates in this process.

## Prerequisites

- Windows 10 or later with the Microsoft Visual C++ 2015–2022 x64 Redistributable, PowerShell 7, Git, CMake 3.22+, Ninja, and an x64 Visual Studio 2022 developer environment.
- The Qt MSVC 2022 x64 kit matching `packaging/dependencies.json`. Qt paths are always supplied explicitly; presets contain no machine-specific path.
- A local x64 FFmpeg directory containing the exact `ffmpeg.exe` and `ffprobe.exe` versions in `packaging/dependencies.json`.
- Full Qt and FFmpeg licence text files supplied explicitly.
- For an official release, trusted Qt and FFmpeg archive files. Populate their source URLs and independently verified SHA-256 hashes in `packaging/dependencies.json`; never copy an unverified hash into the manifest.

Ordinary packaging is offline once these inputs exist locally. The scripts never download Qt, FFmpeg, or another tool.

## Authoritative version and tag

`project(ClipCutter VERSION X.Y.Z)` in `CMakeLists.txt` is the only application-version source. CMake generates `ClipCutterVersion.h`; the executable, `--version`, runtime diagnostics, and About dialog consume it.

Every invocation receives `-Version vX.Y.Z`. The script rejects a mismatch with CMake. An official release also requires:

- `HEAD` is exactly tagged `vX.Y.Z`;
- the tag matches `-Version`;
- the working tree is clean;
- dependency provenance fields and archive hashes are populated and match the supplied archives.

Branch names are never interpreted as versions. Local validation from an untagged or dirty tree warns and produces only a non-official validation package.

## Dependency preparation

Inspect and update `packaging/dependencies.json`. Do not alter tested versions, provenance, hashes, licences, or filenames without checking the actual distribution. Verify local inputs independently:

```powershell
& C:\deps\ffmpeg\bin\ffmpeg.exe -version
& C:\deps\ffmpeg\bin\ffprobe.exe -version
Get-FileHash C:\deps\ffmpeg-archive.7z -Algorithm SHA256
Get-FileHash C:\deps\qt-offline-installer.exe -Algorithm SHA256
```

The FFmpeg and ffprobe first-line semantic versions are validated during packaging. The generated package `DEPENDENCIES.json` records those lines plus SHA-256 hashes of both copied executables. `package-manifest.json` records every other shipped file's size and SHA-256.

Regenerate and review notices after a dependency or asset change:

```powershell
pwsh .\packaging\windows\generate-third-party-notices.ps1 -DependencyManifestPath .\packaging\dependencies.json -OutputPath .\THIRD_PARTY_NOTICES.md
```

The existing SVGs are first-party MIT assets documented in `src/ClipCutter/Icons/README.md`; older assets with unknown provenance were replaced.

## Clean local validation build

Use explicit paths. Start PowerShell 7 with `pwsh`, then paste this hashtable form. It has no fragile backtick line continuations. Do not write `msvc2022\_64`; the directory name contains a plain underscore. The script initializes the installed Visual Studio x64 developer environment automatically.

```powershell
$release = @{
    Version           = 'v3.0.0'
    PreviousTag       = 'v2.1.1'
    QtRoot            = 'C:\Qt\6.11.2\msvc2022_64'
    FfmpegDirectory   = 'C:\deps\ffmpeg\bin'
    QtLicenseFile     = 'C:\Qt\Licenses\LICENSE'
    FfmpegLicenseFile = 'C:\deps\ffmpeg\LICENSE'
    BuildDirectory    = '.\build\windows-package'
    OutputDirectory   = '.\out\release'
    SourceDateEpoch   = 1787839200
}
& .\packaging\windows\build-release.ps1 @release
```

Omit `SourceDateEpoch` from the hashtable to use the `HEAD` commit timestamp. Use a fixed, documented release timestamp for reproducible publication artifacts. ZIP timestamps have two-second resolution.

`find_package(Qt6 6.5 ...)` in CMake means Qt 6.5 is the minimum compatible API version; it does not request installation of Qt 6.5. The release script validates the actual kit version and passes `<QtRoot>\lib\cmake\Qt6` explicitly as `Qt6_DIR`.

For validation without building:

```powershell
& .\packaging\windows\build-release.ps1 @release -ValidateOnly
```

For an official release, commit all changes, create and check out the exact tag, populate the trusted dependency provenance/hashes, then add:

```powershell
$release.OfficialRelease = $true
$release.QtArchive = 'C:\deps\qt-offline-installer.exe'
$release.FfmpegArchive = 'C:\deps\ffmpeg-release.7z'
& .\packaging\windows\build-release.ps1 @release
```

## Individual commands

The orchestrator performs these operations, but each stage is repository-local and directly runnable:

```powershell
cmake --preset release-package -DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\msvc2022_64
cmake --build --preset release-package
ctest --preset release-package
cmake --install .\build\windows-package --config Release --prefix .\build\windows-package\stage

pwsh .\packaging\windows\package-release.ps1 <explicit package parameters>
pwsh .\packaging\windows\smoke-test.ps1 -PackageDirectory .\build\windows-package\stage -Version v3.0.0 -SmokeExecutable .\build\windows-package\ClipCutterPackageSmoke.exe

pwsh .\packaging\windows\generate-release-notes.ps1 -PreviousTag v2.1.1 -CurrentTag v3.0.0 -OutputPath .\out\release\release-notes-v3.0.0.md
```

Integration tests receive `CLIPCUTTER_TEST_FFMPEG` and `CLIPCUTTER_TEST_FFPROBE` automatically from `-FfmpegDirectory`.

## Automated smoke coverage

Smoke tests run first against staging and again after extracting the final ZIP. They verify:

- `ClipCutter.exe`, requested Qt DLLs, `platforms/qwindows.dll`, and a Qt multimedia plugin;
- `ffmpeg.exe` and `ffprobe.exe` existence/version;
- `ClipCutter --version` and JSON runtime version match `-Version`;
- runtime FFmpeg/ffprobe lookup resolves beside the packaged application;
- loaded Qt modules come from the package and none comes from the developer build tree;
- a deterministic synthetic audio/video sample can be created and probed;
- a short accurate export succeeds through `ExportQueueController` and passes ffprobe verification;
- extracted file sizes and SHA-256 values match `package-manifest.json`.

`ClipCutterPackageSmoke.exe` is copied into the package only while a smoke test runs and is removed in `finally`; it is never archived. Test media is created under the system temporary directory and is never shipped.

## Outputs

For `vX.Y.Z`:

- `out/release/ClipCutter-vX.Y.Z-windows-x64.zip`
- `out/release/ClipCutter-vX.Y.Z-windows-x64.zip.sha256`
- `out/release/release-notes-vX.Y.Z.md`
- staging tree: `build/windows-package/stage`
- extraction verification tree: `build/windows-package/archive-check`

Archive entries are ordinal-sorted, use `/`, contain no absolute source paths, and receive the normalized `SOURCE_DATE_EPOCH` timestamp. .NET's built-in ZIP implementation is used; no locally installed archiver is assumed.

## Manual verification and publication checklist

- Confirm the working tree is clean and `git describe --tags --exact-match HEAD` prints `vX.Y.Z`.
- Run the full official command and preserve its console log as release evidence.
- Compare the archive checksum with the `.sha256` file independently.
- Inspect the extracted package tree and launch `ClipCutter.exe` normally on a clean Windows x64 machine or VM.
- Open About and confirm the version.
- Import representative media, preview audio/video, export each profile, and inspect results.
- Review `DEPENDENCIES.json`, `package-manifest.json`, `THIRD_PARTY_NOTICES.md`, and all package licence files.
- Recheck Qt LGPL obligations, FFmpeg build configuration/licence, source availability, asset provenance, and any newly deployed DLL/plugin.
- Confirm Qt SBOM files match the deployed Qt closure and the Qt-supplied FFmpeg DLL metadata/attribution evidence remains accurate.
- Edit `release-notes-vX.Y.Z.md`; do not infer features from branch names.
- Create the GitHub release manually, paste the edited notes, upload only the ZIP and `.sha256`, and verify the uploaded hashes.
- Do not add a GitHub Actions workflow for this release process.

## Rollback after failure

Do not publish partial output. Keep source and dependency inputs unchanged for diagnosis. The next run safely recreates `build/windows-package`, staging, and extraction-check trees and overwrites only the exact versioned archive/checksum paths. If any artifact was uploaded, remove it from the draft release; if a release was published, mark it withdrawn and publish a corrected, newly versioned release rather than silently replacing a public binary. Delete and recreate a tag only if it has not been published or consumed; otherwise advance the patch version.
