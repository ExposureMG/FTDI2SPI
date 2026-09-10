# What is this

FTDI2SPI used in J-Runner with Extras for the xFlasher.

## How to compile

Visual Studio:

FTDI2SPI -> Properties -> C/C++ -> Preprocessor -> Preprocessor Definitions -> add "_CRT_SECURE_NO_WARNINGS"

Build -> Build Solution

CMake selects **D2XX on Windows** and **libftdi on Linux, macOS, and other non-Windows targets**.

Windows:

x86-64

`cmake -G "Visual Studio 18 2026" -A x64 -B build -S .`
`cmake --build build`

x86-32

`cmake -G "Visual Studio 18 2026" -A win32 -B build -S .`
`cmake --build build`

Linux / macOS / other non-Windows platforms:

Install the native `libftdi1` development package (including its libusb dependency)
and `pkg-config`, then run:

```sh
cmake -S . -B build
cmake --build build
```

CMake discovers libftdi through `pkg-config`; no Windows D2XX headers or binaries
are used. If installed in a custom prefix, set `PKG_CONFIG_PATH` to the directory
containing `libftdi1.pc`. Both the SPI/NAND and ISD2100 paths use libftdi.
The `extern/libftdi` and `extern/libusb` submodules are not needed for this build.
