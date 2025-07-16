# Wavy — Build Instructions

This document outlines how to build, configure, and customize the **Wavy** project using its `Makefile` wrapper over CMake. It describes all supported build targets, custom flags, dependency setup, and runtime options.

> [!WARNING]
> 
> It is **HIGHLY** recommended **NOT** to attempt to install 
> Wavy and its architectural components globally. 
> 
> The provided `Makefile` and `CMake` build system **DO NOT** support it for a reason. 
> 
> Only attempt to do so if you know what you are doing. 
> 

## Overview

Wavy is modular and split into six major components:

- **Owner**: Transcodes given audio file (if needed) and converts into HLS-compatible segments and dispatches to server.
- **Server**: HTTPS-based storage and segment distribution service.
- **Client**: Pulls and fetches HLS playlists and segments and has **playback** integration.

> [!NOTE]
> 
> *Dispatcher* is integrated within the **Owner** binary itself!
> 
> Similarly,
> *Fetcher* and *Audio Backend* are integrated with **Client** component.
> 
> They are important architectural components that play a vital role in Wavy, but 
> moving forward, they CANNOT be compiled separarely as a target.
> 
> If it is really needed to compile them separarely, take a look at [examples](https://github.com/Oinkognito/Wavy/blob/main/examples/README.md).
> 

These components can be built individually or collectively using the Makefile.

## Default Build

To build everything (all modules, release configuration):

```sh
make
# or
make all
```

This performs:
- Dependency setup (if not already done)
- CMake configuration into `./build/`
- Compilation of all binaries

## Build Targets

| Target           | Description                                                 |
|------------------|-------------------------------------------------------------|
| `all`            | Builds **all** components                                   |
| `owner`          | Builds `wavy_owner`                                         |
| `server`         | Builds `wavy_server`                                        |
| `client`         | Builds `wavy_client`                                         |
| `init`           | Downloads and installs third-party headers (`miniaudio`, `toml++`) |

## Extra CMake Flags

You can customize the build using `EXTRA_CMAKE_FLAGS`:

```sh
make <target> EXTRA_CMAKE_FLAGS="..."
```

### Supported Flags

| Flag                              | Description                                                                  |
|-----------------------------------|------------------------------------------------------------------------------|
| `-DBUILD_EXAMPLES=ON`             | Enables building of example programs                                         |
| `-DUSE_MOLD=ON`                   | Enables [mold](https://github.com/rui314/mold) linker                        |
| `-DBUILD_NINJA=ON`                | Generates a Ninja backend instead of Makefiles                               |
| `-DBUILD_UI=ON`                   | Enables optional graphical components (if available)                         |
| `-DAUTOGEN_HEADER=ON`             | Automatically generates internal headers                                     | 
| `-DCOMPILE_REPORT=ON`             | Outputs build reports and diagnostic metadata                                |
| `-DNO_FFMPEG=ON`                  | **Disables FFmpeg support**, useful for minimal server build                 |
| `-DNO_TBB=ON`                     | **Disables Intel oneTBB** (parallelism), useful for servers                  |
| `-DBUILD_FETCHER_PLUGINS=ON`      | Enables building for ALL `TSFetcher` plugins (deps must be handled manually) |
| `-DBUILD_AUDIO_BACKEND_PLUGINS=ON`| Enables building for ALL `TSFetcher` plugins (deps must be handled manually) |

> [!NOTE]
> 
> It is key to note that out of the above flags, the below are by default turned **OFF**:
> 
> 1. `-DBUILD_FETCHER_PLUGINS`
> 2. `-DBUILD_AUDIO_BACKEND_PLUGINS`
> 3. `-DNO_FFMPEG`
> 4. `-DNO_TBB`
> 


### Minimal Server Build (No FFmpeg / No oneTBB)

You can compile a slimmed-down server version like so:

```sh
make server EXTRA_CMAKE_FLAGS="-DNO_FFMPEG=ON -DNO_TBB=ON"
```

This excludes all audio/video encoding/decoding dependencies.

## Runtime Convenience Targets

| Target         | Description                                               |
|----------------|-----------------------------------------------------------|
| `run-server`   | Executes the compiled server binary                       |

Since the binaries `wavy_owner` and `wavy_client` have a LOT of parameters required, this approach is not really convenient.

There will be a section to explain the parameters for the above binaries instead for further clarity.

## Code Quality and Maintenance

| Target               | Description                                                           |
|----------------------|-----------------------------------------------------------------------|
| `format`             | Applies `clang-format` to all source files                            |
| `tidy`               | Runs `clang-tidy` checks using `compile_commands.json`                |
| `prepend-license-src`| Prepends license headers (ignores external headers)                   |


## Cleaning and Reset

| Target     | Description                                              |
|------------|----------------------------------------------------------|
| `clean`    | Deletes the `build/` directory                           |
| `cleanup`  | Removes stray `.ts` and `.m3u8` files from the root path |


## SSL Certificate Generation

For local encrypted development, generate self-signed SSL certificates:

```sh
make server-cert
# or silently:
make server-cert-gen
```

Both generate `server.key` and `server.crt` for TLS-enabled LAN usage.

## Third-party Dependencies

These are resolved by cloning the repository **recursively**:

```bash 
git clone --recursive https://github.com/Oinkognito/Wavy
```

Currently, the only third-party dependency we are using is:

- [`toml.hpp`](https://github.com/marzer/tomlplusplus) — TOML parsing

## Build Artifacts

- All binaries are generated inside the `./build/` directory.
- A `compile_commands.json` file is exported for IDEs and static analyzers.
- Linkers used: `ld`, or optionally `mold` via `-DUSE_MOLD=ON`.

## Additional Notes

- The Makefile is **modular** and supports targeted compilation to reduce build time.
- Every module can be independently configured using flags to skip heavy dependencies.
- `clang-tidy` and `clang-format` are integrated to support CI/static analysis pipelines.
- Use Ninja with `-DBUILD_NINJA=ON` for faster parallel builds if `ninja` is installed.

## Questions or Contributions?

Please refer to the official repository at [Oinkognito/wavy](https://github.com/oinkognito/wavy) or raise an issue if you encounter build problems.

This project is under active development so breaking changes are to be expected!
