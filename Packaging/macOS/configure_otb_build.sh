#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OTB_SOURCE_DIR="${OTB_SOURCE_DIR:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
OTB_WORK_DIR="${OTB_WORK_DIR:-${HOME}/otb-macos}"
OTB_CONDA_PREFIX="${OTB_CONDA_PREFIX:-${OTB_WORK_DIR}/otb_env}"
OTB_SUPERBUILD_INSTALL_DIR="${OTB_SUPERBUILD_INSTALL_DIR:-${OTB_WORK_DIR}/otb_superbuild_install}"
OTB_BUILD_DIR="${OTB_BUILD_DIR:-${OTB_WORK_DIR}/build_otb}"
OTB_INSTALL_DIR="${OTB_INSTALL_DIR:-${OTB_WORK_DIR}/otb_install}"
OTB_WRAP_PYTHON="${OTB_WRAP_PYTHON:-ON}"
OTB_ENABLE_FEATURES_EXTRACTION="${OTB_ENABLE_FEATURES_EXTRACTION:-OFF}"
OTB_ENABLE_SAR="${OTB_ENABLE_SAR:-ON}"

if [[ "${CONDA_PREFIX:-}" != "${OTB_CONDA_PREFIX}" ]]; then
  # shellcheck disable=SC1091
  source "${SCRIPT_DIR}/activate_otb_conda_env.sh"
fi

mkdir -p "${OTB_BUILD_DIR}" "${OTB_INSTALL_DIR}"

cmake_args=(
  -S "${OTB_SOURCE_DIR}"
  -B "${OTB_BUILD_DIR}"
  -DXDK_INSTALL_PATH="${OTB_SUPERBUILD_INSTALL_DIR}"
  -DCMAKE_PREFIX_PATH="${OTB_SUPERBUILD_INSTALL_DIR}"
  -DCMAKE_INSTALL_PREFIX="${OTB_INSTALL_DIR}"
  -DBUILD_TESTING=ON
  -DOTB_WRAP_PYTHON="${OTB_WRAP_PYTHON}"
  -DOTB_WRAP_QGIS=OFF
)

if [[ "${OTB_ENABLE_SAR}" == "ON" ]]; then
  cmake_args+=(
    -DOTBGroup_SAR=ON
    -DModule_OTBSARCalibration=ON
    -DModule_OTBAppSAR=ON
  )
fi

if [[ "${OTB_ENABLE_FEATURES_EXTRACTION}" == "ON" ]]; then
  cmake_args+=(
    -DOTB_USE_MUPARSER=ON
    -DOTB_USE_MUPARSERX=ON
    -DOTBGroup_FeaturesExtraction=ON
    -DModule_OTBAppFeaturesExtraction=ON
  )
fi

cmake "${cmake_args[@]}"

cat <<EOF

OTB configured.

Build directory:
  ${OTB_BUILD_DIR}

Install directory:
  ${OTB_INSTALL_DIR}

Next steps:
  cmake --build "${OTB_BUILD_DIR}" -j2
  cmake --build "${OTB_BUILD_DIR}" --target install -j2
  bash "${SCRIPT_DIR}/fix_otb_macos_install_names.sh" "${OTB_INSTALL_DIR}"

EOF
