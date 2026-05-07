#!/usr/bin/env bash
set -euo pipefail

OTB_INSTALL_DIR="${1:-${OTB_INSTALL_DIR:-}}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This helper is only for macOS." >&2
  exit 1
fi

if [[ -z "${OTB_INSTALL_DIR}" ]]; then
  echo "Usage: $0 /path/to/otb_install" >&2
  exit 2
fi

TINYXML_LIB="${OTB_INSTALL_DIR}/lib/libtinyxml.dylib"

if [[ ! -f "${TINYXML_LIB}" ]]; then
  echo "Missing ${TINYXML_LIB}" >&2
  exit 1
fi

install_name_tool -id @rpath/libtinyxml.dylib "${TINYXML_LIB}"

while IFS= read -r f; do
  install_name_tool -change libtinyxml.dylib @rpath/libtinyxml.dylib "${f}"
done < <(
  find "${OTB_INSTALL_DIR}/lib" -type f \( -name "*.dylib" -o -name "*.so" \) | while IFS= read -r f; do
    if otool -L "${f}" 2>/dev/null | grep -q $'^\tlibtinyxml\\.dylib '; then
      echo "${f}"
    fi
  done
)

APPDIR="${OTB_INSTALL_DIR}/lib/otb/applications"
if [[ -d "${APPDIR}" ]]; then
  find "${APPDIR}" -maxdepth 1 -type f -name 'otbapp_*.so' | while IFS= read -r f; do
    ln -sf "$(basename "${f}")" "${f%.so}.dylib"
  done
fi

echo "macOS install-name fix applied to ${OTB_INSTALL_DIR}"
