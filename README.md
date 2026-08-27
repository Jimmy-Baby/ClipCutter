# ClipCutter
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/Jimmy-Baby/ClipCutter/blob/main/LICENSE)

![image](https://github.com/user-attachments/assets/108e997f-cf9e-4fe6-a518-30d14dbcb2ae)

## Description

ClipCutter simplifies the management of video game clips.
It is written in C++ using the Qt framework.
It is designed to tackle the challenge of dealing with a large number of videos at once.

## Installation

### Download
Download the latest [release](https://github.com/Jimmy-Baby/ClipCutter/releases/download/v2.1.1/ClipCutter.v2.1.1.zip)

### Extract
Extract the files into a folder. This is where ClipCutter will live.

Right-click `ClipCutter.exe`, then select **Send to > Desktop (create shortcut)** to create a desktop shortcut.

## Usage

Here's how you can use it:

1. Open the clips you want to cut using any of the options shown in the 'File' menu.
2. Set the start/end points for each clip to reduce their length. Optionally, you can rename them and create common keywords using the keywords manager to add prefixes to clips.
3. Remove clips you do not want to keep by checking the **Skip?** box next to them in the video list.
4. Choose your output settings, then click **Process Clips**. Processed clips are written to `ClipCutterOutput` in the original clip directory.

## Building on Windows

### Prerequisites

- Windows 10 or later.
- CMake 3.22 or later and Ninja available on `PATH`.
- Visual Studio 2022 or Build Tools for Visual Studio 2022 with **Desktop development with C++** and a Windows SDK. Run commands from an x64 Native Tools or Developer PowerShell so MSVC is available.
- Qt 6.5 or later with Core, Gui, Widgets, Multimedia, MultimediaWidgets, and Test. Qt 6.11.2 with the 64-bit MSVC 2022 kit is the tested configuration.
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

### CMake targets

- `clipcutter_core`: reusable queue, naming, utility, and FFmpeg implementation.
- `ClipCutter`: `Main.cpp`, the main window/UI form, and Qt resources.
- `ClipCutterTests`: Qt Test regression tests registered with CTest.
- `ClipCutterExportTests`: deterministic export-engine tests using an injected process runner.
- `ClipCutterMediaCorrectnessTests`: probe, profile, path, finalisation, metadata, and verification tests.

CMake generates UIC, MOC, and RCC output inside the selected build directory. Generated headers such as `ui_MainWindow.h` must never be generated in or committed from the source tree.

### Export engine

Exports run sequentially through an event-driven `ExportQueueController`. Each FFmpeg invocation uses a `QProcess` runner with separate program and argument values; the GUI thread never waits for process startup or completion.

The state machine is explicit:

```text
Pending -> Preparing -> Running -> Finalising -> Succeeded
    |          |           |           |
    |          +-----------+           +-> Failed
    |                  |   +--------------> Failed
    |                  +-> Cancelling -> Cancelled
    +-> Cancelled
    +-> Skipped

Failed/Cancelled -> Pending (explicit retry only)
```

Invalid transitions and duplicate process-completion notifications are ignored. An ordinary failure completes that item and advances the queue. Queue completion reports succeeded, failed, skipped, and cancelled counts rather than treating a partial failure as success.

FFmpeg writes to a unique `.clipcutter-<uuid>.part.<media-extension>` file in the destination directory. A zero exit is not sufficient: ffprobe must confirm a non-empty file, expected streams, and duration within 350 ms for accurate profiles or 2500 ms for keyframe-aligned Fast Copy. Only then is the temporary file atomically promoted. Windows replacement uses `ReplaceFileW`, so an existing final file remains intact until the replacement is complete. Failed and cancelled jobs remove their temporary output; stale ClipCutter temporary files are removed only after seven days.

Source inspection also uses asynchronous `QProcess` ffprobe calls. `MediaProbe` parses JSON format/stream data, limits concurrency to two by default, caches results by canonical path/size/modification time, and tags results with the stable clip ID. The model rejects results for removed or replaced clips. Probe failures remain in the queue with diagnostic text; unprobed, failed, unknown-duration, and video-less entries cannot export.

Output behavior is defined centrally by stable profiles:

| ID | UI name | Trim | Container/codecs | Settings |
| --- | --- | --- | --- | --- |
| `fast-copy` | Fast Copy | Keyframe/seek-aligned | Source container, stream copy | No re-encode |
| `accurate-balanced` | Accurate Balanced | Accurate re-encode | MP4, H.264, AAC | CRF 23, medium |
| `accurate-high-quality` | Accurate High Quality | Accurate re-encode | MP4, H.264, AAC | CRF 18, slow |
| `compact` | Compact | Accurate re-encode | MP4, H.264, AAC | CRF 28, fast |

Preflight separates the editable base name, profile-derived extension, final path, and temporary path. It rejects empty/invalid/reserved Windows names, trailing periods/spaces, duplicate batch outputs, missing/unwritable directories, and resolves existing files with Ask, Auto Rename, Skip, or Overwrite before launching FFmpeg. Auto Rename deterministically uses `name (2).ext`, `name (3).ext`, and so on.

`MetadataService` returns structured source-open/read and output-open/write errors and uses Unicode Win32 APIs plus RAII handles. Media export and metadata preservation have separate semantics: a verified, finalised media file is `Succeeded` with a metadata warning when timestamp copying fails. Verification failures are export failures; FFmpeg and ffprobe diagnostics remain available in the item log.

Startup diagnostics asynchronously check both binary versions plus `libx264`, AAC, and MP4 capabilities. Unsupported profiles are disabled with a reason.

Progress is read incrementally from `-progress pipe:1`. The parser retains partial lines between reads and accepts `out_time_us`, `out_time_ms`, and `out_time`. Known segment durations produce clamped item progress and duration-weighted total progress; unknown durations are indeterminate at item level and use an average-duration/job-count fallback for the total. `progress=end` updates progress only—success still requires a normal zero-code process exit.

Run the ordinary tests without FFmpeg:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

To enable all optional integration cases, set both executable paths before CTest. The integration tests skip cleanly when they are unset:

```powershell
$env:CLIPCUTTER_TEST_FFMPEG = "C:\path\to\ffmpeg.exe"
$env:CLIPCUTTER_TEST_FFPROBE = "C:\path\to\ffprobe.exe"
ctest --preset debug
```

## Contributing

Contributions from the community are welcome. Create a pull request to get involved.

## License

ClipCutter is open-source software licensed under the MIT License. See the [LICENSE](https://github.com/Jimmy-Baby/ClipCutter/blob/main/LICENSE) file for more details.

# Support

[Issue Tracker](https://github.com/Jimmy-Baby/ClipCutter/issues).
