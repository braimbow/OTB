#!/usr/bin/env bash

# Source this file from the OTB repository root:
#   source Packaging/macOS/activate_otb_conda_env.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OTB_SOURCE_DIR="${OTB_SOURCE_DIR:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
OTB_WORK_DIR="${OTB_WORK_DIR:-${HOME}/otb-macos}"
OTB_CONDA_PREFIX="${OTB_CONDA_PREFIX:-${OTB_WORK_DIR}/otb_env}"
CONDA_EXE="${CONDA_EXE:-$(command -v conda || true)}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "activate_otb_conda_env.sh is intended for macOS." >&2
  return 1 2>/dev/null || exit 1
fi

if [[ -z "${CONDA_EXE}" ]]; then
  echo "Could not find conda. Set CONDA_EXE=/path/to/conda first." >&2
  return 1 2>/dev/null || exit 1
fi

export OTB_SOURCE_DIR
export OTB_WORK_DIR
export OTB_CONDA_PREFIX
export CONDA_SUBDIR="${CONDA_SUBDIR:-osx-64}"
export HOME="${OTB_WORK_DIR}/.conda_home"
export XDG_CACHE_HOME="${OTB_WORK_DIR}/.conda_cache"
export CONDA_PKGS_DIRS="${OTB_WORK_DIR}/.conda_pkgs"
export CONDA_ENVS_PATH="${OTB_WORK_DIR}/.conda_envs"

mkdir -p "${HOME}" "${XDG_CACHE_HOME}" "${CONDA_PKGS_DIRS}" "${CONDA_ENVS_PATH}"

eval "$("${CONDA_EXE}" shell.bash hook)"
conda activate "${OTB_CONDA_PREFIX}"

export PATH="${OTB_CONDA_PREFIX}/bin:${PATH}"

if [[ -f "${OTB_CONDA_PREFIX}/ssl/cacert.pem" ]]; then
  export SSL_CERT_FILE="${OTB_CONDA_PREFIX}/ssl/cacert.pem"
  export CURL_CA_BUNDLE="${OTB_CONDA_PREFIX}/ssl/cacert.pem"
  export REQUESTS_CA_BUNDLE="${OTB_CONDA_PREFIX}/ssl/cacert.pem"
fi

if [[ -x "${OTB_CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-clang" ]]; then
  export CC="${OTB_CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-clang"
fi

if [[ -x "${OTB_CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-clang++" ]]; then
  export CXX="${OTB_CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-clang++"
fi

if [[ -x "${OTB_CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-gfortran" ]]; then
  export FC="${OTB_CONDA_PREFIX}/bin/x86_64-apple-darwin13.4.0-gfortran"
fi

echo "OTB macOS Conda environment activated:"
echo "  OTB_SOURCE_DIR=${OTB_SOURCE_DIR}"
echo "  OTB_WORK_DIR=${OTB_WORK_DIR}"
echo "  OTB_CONDA_PREFIX=${OTB_CONDA_PREFIX}"
echo "  CONDA_SUBDIR=${CONDA_SUBDIR}"
