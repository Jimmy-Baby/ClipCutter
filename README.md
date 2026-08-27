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
- A Windows FFmpeg executable for running the application. FFmpeg is not needed to configure, compile, or run the pure unit tests.

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

To run a local build, place `ffmpeg.exe` beside the generated `ClipCutter.exe`, make the matching Qt `bin` directory available on `PATH`, then start the executable:

```powershell
Copy-Item C:\path\to\ffmpeg.exe build\debug\
$env:Path = "C:\path\to\Qt\6.11.2\msvc2022_64\bin;$env:Path"
.\build\debug\ClipCutter.exe
```

Release archives already include FFmpeg and the required Qt runtime files. Local source builds intentionally do not perform deployment or packaging.

### CMake targets

- `clipcutter_core`: reusable queue, naming, utility, and FFmpeg implementation.
- `ClipCutter`: `Main.cpp`, the main window/UI form, and Qt resources.
- `ClipCutterTests`: Qt Test regression tests registered with CTest.

CMake generates UIC, MOC, and RCC output inside the selected build directory. Generated headers such as `ui_CCMainWindow.h` must never be generated in or committed from the source tree.

## Contributing

Contributions from the community are welcome. Create a pull request to get involved.

## License

ClipCutter is open-source software licensed under the MIT License. See the [LICENSE](https://github.com/Jimmy-Baby/ClipCutter/blob/main/LICENSE) file for more details.

# Support

[Issue Tracker](https://github.com/Jimmy-Baby/ClipCutter/issues).
