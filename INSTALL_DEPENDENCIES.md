# Installing Dependencies for Wirekrak

This project uses **vcpkg** to manage all third-party dependencies and **CMake Presets** for reproducible builds.

## Prerequisites

- Windows 10/11
- CMake >= 3.25
- Ninja
- Git
- MinGW-w64 (x64)
- PowerShell or Command Prompt

## 1. Install vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg
bootstrap-vcpkg.bat
```

## 2. Install required libraries

```bash
vcpkg install simdjson spdlog cli11 --triplet x64-mingw-dynamic
```

## 2. Install optional libraries

### 2.1 Experimental examples

```bash
vcpkg install lz4 xxhash --triplet x64-mingw-dynamic
```

### 2.2 Flashstrike benchmarks

```bash
vcpkg install lz4 xxhash prometheus-cpp --triplet x64-mingw-dynamic
```

## 3. Configure environment

Make sure Ninja and MinGW are in your PATH.

Example:

```cmd
set PATH=C:\mingw64\bin;%PATH%
```

## 4. Configure Wirekrak

From the project root:

```bash
cmake --preset ninja-debug
```

## 5. Build

```bash
cmake --build --preset debug
```

## 6. Run tests

```bash
ctest --preset test-debug
```

## Notes

- TLS is handled via Windows **SChannel**, no OpenSSL required.
- All dependencies are header-only or linked via vcpkg.
- Examples are built by default.

Happy hacking 🚀

---

⬅️ [Back to README](README.md#installation)
