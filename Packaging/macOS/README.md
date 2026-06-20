# macOS Build and Runtime Helpers

This directory contains macOS-specific helper scripts for building and running
OTB from source.

The goal is to keep macOS packaging and runtime fixes separate from OTB feature
branches. These helpers are not sensor-specific and can be used with optical,
SAR, and other OTB modules.

## Scope

The helpers address macOS build and runtime issues such as:

- Conda environment and architecture selection.
- Certificate propagation for CMake `ExternalProject` downloads.
- SuperBuild and OTB configure commands.
- Dynamic library install-name fixes.
- OTB application module `.so` / `.dylib` compatibility symlinks.

They do not implement sensor-specific logic.

## Recommended Branch Layout

Use this branch independently from feature branches:

```bash
develop
  -> macos-integration
```

Feature branches for specific sensors or processing chains can be rebased or
merged separately.

## Create a Conda Environment

The recommended reproducible environment file is:

```text
Packaging/macOS/environment-osx-64.yml
```

For Intel macOS or Apple Silicon under Rosetta/x86_64:

```bash
export OTB_WORK_DIR="$HOME/otb-macos"
export OTB_CONDA_PREFIX="$OTB_WORK_DIR/otb_env"
export CONDA_SUBDIR=osx-64

conda env create -p "$OTB_CONDA_PREFIX" -f Packaging/macOS/environment-osx-64.yml
```

An exact lock-style export of the environment used during development is also
provided:

```text
Packaging/macOS/conda-explicit-osx-64.txt
```

It can be used when the flexible YAML environment is not enough:

```bash
export OTB_WORK_DIR="$HOME/otb-macos"
export OTB_CONDA_PREFIX="$OTB_WORK_DIR/otb_env"
conda create -y -p "$OTB_CONDA_PREFIX" --file Packaging/macOS/conda-explicit-osx-64.txt
```

The explicit file is more reproducible but less portable over time than the YAML
file.

For Intel macOS or Apple Silicon under Rosetta/x86_64:

```bash
export OTB_WORK_DIR="$HOME/otb-macos"
export OTB_CONDA_PREFIX="$OTB_WORK_DIR/otb_env"
export CONDA_SUBDIR=osx-64

conda create -y -p "$OTB_CONDA_PREFIX" -c conda-forge \
  python=3.10 cmake make ninja git curl pkg-config ca-certificates \
  clang_osx-64 clangxx_osx-64 gfortran_osx-64
```

For a quick manual environment, the command above is still sufficient as a
minimal starting point. For native Apple Silicon, omit `CONDA_SUBDIR=osx-64` and
choose compiler packages matching your Conda platform.

## Activate the Environment

From the repository root:

```bash
export OTB_WORK_DIR="$HOME/otb-macos"
export OTB_CONDA_PREFIX="$OTB_WORK_DIR/otb_env"
export CONDA_SUBDIR=osx-64

source Packaging/macOS/activate_otb_conda_env.sh
```

The activation helper configures:

- Conda package/cache paths under `OTB_WORK_DIR`.
- TLS certificate variables used by CMake, curl and Python tooling.
- `CC`, `CXX`, and `FC` when Conda compiler wrappers are available.

## Configure and Build SuperBuild Dependencies

```bash
source Packaging/macOS/activate_otb_conda_env.sh
bash Packaging/macOS/configure_otb_superbuild.sh
cmake --build "$OTB_SUPERBUILD_DIR" --target OTB_DEPENDS -j2
```

Useful variables:

```bash
export OTB_SUPERBUILD_DIR="$OTB_WORK_DIR/build_superbuild"
export OTB_SUPERBUILD_INSTALL_DIR="$OTB_WORK_DIR/otb_superbuild_install"
export OTB_ENABLE_FEATURES_EXTRACTION=ON
```

`OTB_ENABLE_FEATURES_EXTRACTION=ON` enables the FeaturesExtraction/MuParser path
used by applications such as `BandMath`.

## Configure, Build, and Install OTB

```bash
source Packaging/macOS/activate_otb_conda_env.sh
bash Packaging/macOS/configure_otb_build.sh
cmake --build "$OTB_BUILD_DIR" -j2
cmake --build "$OTB_BUILD_DIR" --target install -j2
```

Useful variables:

```bash
export OTB_BUILD_DIR="$OTB_WORK_DIR/build_otb"
export OTB_INSTALL_DIR="$OTB_WORK_DIR/otb_install"
export OTB_WRAP_PYTHON=ON
export OTB_ENABLE_FEATURES_EXTRACTION=ON
export OTB_ENABLE_SAR=ON
```

## Apply macOS Runtime Fixes

After each install:

```bash
bash Packaging/macOS/fix_otb_macos_install_names.sh "$OTB_INSTALL_DIR"
```

This helper:

- fixes `libtinyxml.dylib` install names to use `@rpath`;
- updates installed OTB libraries that still refer to bare `libtinyxml.dylib`;
- adds `.dylib` symlinks for OTB application modules when only `.so` names exist.

## Runtime Check

```bash
source "$OTB_INSTALL_DIR/otbenv.profile"
otbcli_ReadImageInfo --help
otbApplicationLauncherCommandLine --help
```

If FeaturesExtraction was enabled:

```bash
otbcli_BandMath --help
```

If SAR was enabled:

```bash
otbApplicationLauncherCommandLine SARCalibration --help
```

## Notes

- These scripts are not required on Linux or RedHat/SLURM clusters.
- They are intentionally parameterized; avoid committing machine-local absolute
  paths into downstream branches.
- For complex GeoTIFF outputs, avoid `PREDICTOR=3`; GDAL only supports that
  predictor for `Float32` and `Float64`, not complex pixel types.
