#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OTB_SOURCE_DIR="${OTB_SOURCE_DIR:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
OTB_WORK_DIR="${OTB_WORK_DIR:-${HOME}/otb-macos}"
OTB_CONDA_PREFIX="${OTB_CONDA_PREFIX:-${OTB_WORK_DIR}/otb_env}"
OTB_SUPERBUILD_DIR="${OTB_SUPERBUILD_DIR:-${OTB_WORK_DIR}/build_superbuild}"
OTB_SUPERBUILD_INSTALL_DIR="${OTB_SUPERBUILD_INSTALL_DIR:-${OTB_WORK_DIR}/otb_superbuild_install}"
CMAKE_GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
OTB_ENABLE_FEATURES_EXTRACTION="${OTB_ENABLE_FEATURES_EXTRACTION:-OFF}"
OTB_ENABLE_SAR="${OTB_ENABLE_SAR:-ON}"

if [[ "${CONDA_PREFIX:-}" != "${OTB_CONDA_PREFIX}" ]]; then
  # shellcheck disable=SC1091
  source "${SCRIPT_DIR}/activate_otb_conda_env.sh"
fi

mkdir -p "${OTB_SUPERBUILD_DIR}" "${OTB_SUPERBUILD_INSTALL_DIR}"

cmake_args=(
  -S "${OTB_SOURCE_DIR}/SuperBuild"
  -B "${OTB_SUPERBUILD_DIR}"
  -G "${CMAKE_GENERATOR}"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="${OTB_SUPERBUILD_INSTALL_DIR}"
  -DBUILD_TESTING=OFF
  -DBUILD_EXAMPLES=OFF
  -DOTB_WRAP_PYTHON=OFF
)

if [[ -f "${OTB_CONDA_PREFIX}/ssl/cacert.pem" ]]; then
  cmake_args+=(-DCMAKE_TLS_CAINFO="${OTB_CONDA_PREFIX}/ssl/cacert.pem")
fi

if [[ "${OTB_ENABLE_SAR}" == "ON" ]]; then
  cmake_args+=(-DOTB_BUILD_SAR=ON)
fi

if [[ "${OTB_ENABLE_FEATURES_EXTRACTION}" == "ON" ]]; then
  cmake_args+=(-DOTB_BUILD_FeaturesExtraction=ON)
fi

cmake "${cmake_args[@]}"

cat <<EOF

SuperBuild configured.

Build directory:
  ${OTB_SUPERBUILD_DIR}

Install directory:
  ${OTB_SUPERBUILD_INSTALL_DIR}

Next step:
  cmake --build "${OTB_SUPERBUILD_DIR}" --target OTB_DEPENDS -j2

EOF
