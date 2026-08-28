# Third-party notices

This file describes material distributed in the Windows package. The package-specific `DEPENDENCIES.json` and `package-manifest.json` record the exact deployed files, versions, sizes, and SHA-256 hashes.

## Qt

- Version: 6.11.2 release-tested baseline.
- Copyright: The Qt Company Ltd. and other Qt contributors.
- Licence used for this distribution: LGPL-3.0-only. Qt also offers alternative commercial and GPL terms.
- Source/provenance: maintainer-supplied Qt MSVC 2022 x64 kit; the package contains only the runtime closure selected by that kit's `windeployqt.exe`.
- Distribution form: dynamically linked DLLs and runtime plugins.
- Components requested by ClipCutter: Qt Core, Gui, Widgets, Multimedia, and MultimediaWidgets. Deployment can include Qt-owned platform, multimedia, image-format, TLS, network-information, styles, and audio plugins needed by those components.
- Full Qt licence text in the package: `licenses/Qt/LICENSE.txt`.
- Machine-readable full component licence/attribution records: `licenses/Qt/SBOM/qtbase-6.11.2.spdx.json`, `licenses/Qt/SBOM/qtmultimedia-6.11.2.spdx.json`, and `licenses/Qt/SBOM/qtsvg-6.11.2.spdx.json`.
- Source availability/compliance: release maintainers must retain the exact Qt kit provenance and satisfy the LGPL relinking/source-offer obligations applicable to the published binaries.

Qt runtime binaries and plugins incorporate third-party components under compatible licences. The copied Qt SPDX records are the exhaustive component-level attribution for the deployed Qt Base, Multimedia, and SVG modules, including image codecs, rendering code, and other embedded code. Before publication, compare the staged files with those records and update this notice if the selected kit changes its deployed closure.

## FFmpeg libraries supplied by Qt Multimedia

- Deployed DLL product version: 7.1.5 (`avcodec-61.dll`, `avformat-61.dll`, `avutil-59.dll`, `swresample-5.dll`, and `swscale-8.dll`).
- Copyright: FFmpeg developers and authors of incorporated components.
- Licence reported by the Qt 6.11.2 Multimedia SPDX record: LGPL-2.1-or-later, BSD-3-Clause, BSD-2-Clause, BSD-Source-Code, ISC, MIT, and MPL-2.0 components.
- Source/provenance: dynamically linked FFmpeg runtime from the maintainer-supplied Qt 6.11.2 MSVC 2022 x64 kit, deployed by `windeployqt`. The Qt kit's SPDX attribution identifies its upstream FFmpeg source component as 7.1.3; the shipped DLL file metadata reports 7.1.5. Both facts are retained rather than guessed away.
- Distribution form: dynamically linked libraries loaded by `multimedia/ffmpegmediaplugin.dll`.
- Full licence and component attribution: `licenses/Qt/SBOM/qtmultimedia-6.11.2.spdx.json`.
- Nested components identified by that SPDX record include Signalsmith Stretch (MIT), libjpeg (IJG), zlib (Zlib), and Boost material (BSL-1.0).

## FFmpeg command-line programs

- Version: FFmpeg 8.0.1 and ffprobe 8.0.1 release-tested baseline.
- Copyright: FFmpeg developers and the respective authors of enabled libraries.
- Licence: GPL-3.0-or-later for the release-tested full Windows build. A differently configured build can have different licence obligations and must not be substituted without updating the dependency manifest and this notice.
- Source/provenance: maintainer-supplied Windows x64 binary distribution. Official packaging requires trusted source provenance and a verified archive SHA-256 in `packaging/dependencies.json`.
- Distribution form: bundled `ffmpeg.exe` and `ffprobe.exe` executables.
- Full licence text in the package: `licenses/FFmpeg/LICENSE.txt`.
- Exact executable hashes and reported versions: package `DEPENDENCIES.json`.

## ClipCutter SVG icons

The SVG files under `src/ClipCutter/Icons` are first-party assets. These assets are covered by ClipCutter's MIT licence at `licenses/ClipCutter/LICENSE.txt`.

## ClipCutter

- Copyright: 2023 jimbab and contributors.
- Licence: MIT.
- Full licence text in the package: `licenses/ClipCutter/LICENSE.txt`.

## Non-bundled Windows prerequisites

The Microsoft Visual C++ 2015–2022 x64 runtime and Windows graphics/compiler system components are prerequisites, not package contents. Packaging passes `--no-compiler-runtime`, `--no-system-d3d-compiler`, and `--no-system-dxc-compiler`; no Visual Studio redistributable installer, D3D compiler, DXC, or DXIL binary is shipped.
