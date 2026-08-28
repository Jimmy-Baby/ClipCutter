# ClipCutter
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/Jimmy-Baby/ClipCutter/blob/main/LICENSE)

<img width="2560" height="1392" alt="ClipCutter_QPgiauHzso" src="https://github.com/user-attachments/assets/4d8738fb-fc6e-4416-b84a-8cbcc953a69c" />

## Description

ClipCutter simplifies the management of video game clips.
It is written in C++ using the Qt framework.
It is designed to tackle the challenge of dealing with a large number of videos at once.

## Installation

### Download
Download the latest package from [Releases](https://github.com/Jimmy-Baby/ClipCutter/releases/latest).

### Extract
Extract the files into a folder. This is where ClipCutter will live.

Right-click `ClipCutter.exe`, then select **Send to > Desktop (create shortcut)** to create a desktop shortcut.

## Usage

Here's how you can use it:

1. Open the clips you want to cut using any of the options shown in the 'File' menu.
2. Set the start/end points for each clip to reduce their length. Optionally, you can rename them and create common keywords using the keywords manager to add prefixes to clips.
3. Remove clips you do not want to keep by checking the **Skip?** box next to them in the video list.
4. Choose your output settings, then click **Process Clips**. Processed clips are written to `ClipCutterOutput` in the original clip directory.

### Bulk workflow

- Imports append to the queue. Duplicate source paths are skipped consistently for file dialogs, folders, and drag/drop.
- Dropped local video files and folders will work. Folder recursion is off by default; unsupported files and non-local URLs are ignored with a status summary.
- **Beside each source** writes to a configurable child directory of every source directory (`ClipCutterOutput` by default). **Fixed directory** writes the whole batch to one selected directory.
- The destination and selected-row output path remain visible while editing. Export shows the complete rendered batch before work starts.
- Queue filtering searches source/output names, prefixes, status, and keep/skip state without changing the source model.
- The **Batch** menu applies keep/skip, prefix, template, profile, full-range reset, removal, and queue-clear operations at model level.

### Naming-template syntax

Templates render a base filename only. The selected output profile owns the extension.

| Token | Value |
| --- | --- |
| `{original}` | Source filename without its extension |
| `{prefix}` | Row prefix/keyword, or empty text |
| `{index}` | One-based queue index |
| `{index:03}` | One-based queue index padded to three digits; widths 1–64 use the same zero-prefixed form |
| `{date}` | Current local date as `yyyy-MM-dd` |
| `{profile}` | Stable output-profile ID |
| `{segment}` | One-based segment number; reserved now for forward-compatible sessions |

Literal text is copied exactly. Braces cannot be escaped. Unknown tokens, unmatched braces, malformed padding, empty rendered names, reserved Windows device names, and invalid filename characters are rejected. Rendering is deterministic and case-insensitive duplicate base names are reported before export. Default: `{prefix}{original}`.

## Building on Windows

### Prerequisites

- Windows 10 or later.
- CMake 3.22 or later and Ninja available on `PATH`.
- Visual Studio 2022 or Build Tools for Visual Studio 2022 with **Desktop development with C++** and a Windows SDK. Run commands from an x64 Native Tools or Developer PowerShell so MSVC is available.
- Qt 6.5 or later with Core, Gui, Widgets, Multimedia, MultimediaWidgets, and Test. The release-tested version is recorded in `packaging/dependencies.json`.
- FFmpeg and ffprobe for running the application. Neither binary is needed for pure unit tests.

Qt is not stored at a fixed repository path. Let CMake find it through `CMAKE_PREFIX_PATH` or `Qt6_DIR`. For example, replace the example path below with the root of your compiler-matched Qt kit:

```powershell
$env:CMAKE_PREFIX_PATH = "C:\path\to\Qt\6.11.2\msvc2022_64"
```

The equivalent one-command form is `cmake --preset debug -DCMAKE_PREFIX_PATH=C:\path\to\Qt\6.11.2\msvc2022_64`. `-DQt6_DIR=C:\path\to\Qt\6.11.2\msvc2022_64\lib\cmake\Qt6` also works.

### Configure, build, and test

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake --preset release
cmake --build --preset release
```

CTest is also directly usable from either build directory:

```powershell
Set-Location build\debug
ctest --output-on-failure
```

To run a local build, place `ffmpeg.exe` and `ffprobe.exe` beside the generated `ClipCutter.exe` (or make both available on `PATH`), make the matching Qt `bin` directory available on `PATH`, then start the executable:

```powershell
Copy-Item C:\path\to\ffmpeg.exe build\debug\
Copy-Item C:\path\to\ffprobe.exe build\debug\
$env:Path = "C:\path\to\Qt\6.11.2\msvc2022_64\bin;$env:Path"
.\build\debug\ClipCutter.exe
```

Release archives already include FFmpeg and the required Qt runtime files. Local source builds intentionally do not perform deployment or packaging.

Maintainers build release archives entirely locally. See [docs/RELEASING.md](docs/RELEASING.md); no hosted CI or GitHub Actions workflow is used.

### Export engine

Output behavior is defined centrally by stable profiles:

| ID | UI name | Trim | Container/codecs | Settings |
| --- | --- | --- | --- | --- |
| `fast-copy` | Fast Copy | Keyframe/seek-aligned | Source container, stream copy | No re-encode |
| `accurate-balanced` | Accurate Balanced | Accurate re-encode | MP4, H.264, AAC | CRF 23, medium |
| `accurate-high-quality` | Accurate High Quality | Accurate re-encode | MP4, H.264, AAC | CRF 18, slow |
| `compact` | Compact | Accurate re-encode | MP4, H.264, AAC | CRF 28, fast |

## Contributing

Contributions from the community are welcome. Create a pull request to get involved.

## License

ClipCutter is open-source software licensed under the MIT License. See the [LICENSE](https://github.com/Jimmy-Baby/ClipCutter/blob/main/LICENSE) file for more details.

# Support

[Issue Tracker](https://github.com/Jimmy-Baby/ClipCutter/issues).
