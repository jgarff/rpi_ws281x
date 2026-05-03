#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_DIR="${1:-${REPO_ROOT}/dist}"

PYTHON_BIN="${PYTHON_BIN:-python3}"
RPI_WS281X_PY_REPO="${RPI_WS281X_PY_REPO:-https://github.com/rpi-ws281x/rpi-ws281x-python.git}"
RPI_WS281X_PY_REF="${RPI_WS281X_PY_REF:-${RPI_WS281X_PY_BRANCH:-v5.0.0}}"
GITHUB_TOKEN="${GITHUB_TOKEN:-}"
RPI_WS281X_PY_TOKEN="${RPI_WS281X_PY_TOKEN:-${GITHUB_TOKEN}}"
PIP_VERSION="${PIP_VERSION:-25.3}"
SETUPTOOLS_VERSION="${SETUPTOOLS_VERSION:-80.9.0}"
WHEEL_VERSION="${WHEEL_VERSION:-0.45.1}"

OS_NAME="$(uname -s)"
ARCH_NAME="$(uname -m)"

if [ "${OS_NAME}" != "Linux" ]; then
    echo "rpi_ws281x wheel build requires Linux (detected ${OS_NAME})."
    exit 1
fi

if [ "${ARCH_NAME}" != "aarch64" ] && [ "${ARCH_NAME}" != "arm64" ]; then
    echo "rpi_ws281x wheel build requires aarch64/arm64 (detected ${ARCH_NAME})."
    exit 1
fi

if ! command -v git >/dev/null 2>&1; then
    echo "git not found; cannot build rpi_ws281x wheel."
    exit 1
fi

if ! command -v "${PYTHON_BIN}" >/dev/null 2>&1; then
    echo "${PYTHON_BIN} not found; cannot build rpi_ws281x wheel."
    exit 1
fi

with_token() {
    local url="$1"
    local token="$2"

    if [ -z "${token}" ]; then
        echo "${url}"
        return 0
    fi

    if [[ "${url}" == https://github.com/* ]]; then
        echo "https://x-access-token:${token}@github.com/${url#https://github.com/}"
        return 0
    fi

    echo "${url}"
}

clone_repo() {
    local url="$1"
    local ref="$2"
    local dest="$3"
    local token="$4"

    local clone_url
    clone_url="$(with_token "${url}" "${token}")"
    if GIT_TERMINAL_PROMPT=0 git clone --depth 1 --branch "${ref}" "${clone_url}" "${dest}"; then
        return 0
    fi

    echo "Git clone failed; attempting tarball fallback..."

    if [[ "${url}" != https://github.com/* ]]; then
        echo "Tarball fallback only supports github.com URLs."
        exit 1
    fi

    local repo_path="${url#https://github.com/}"
    repo_path="${repo_path%.git}"
    local tar_url
    if [[ "${ref}" =~ ^[0-9a-f]{7,}$ ]]; then
        tar_url="https://github.com/${repo_path}/archive/${ref}.tar.gz"
    elif [[ "${ref}" == v* ]]; then
        tar_url="https://github.com/${repo_path}/archive/refs/tags/${ref}.tar.gz"
    else
        tar_url="https://github.com/${repo_path}/archive/refs/heads/${ref}.tar.gz"
    fi
    local tar_path="${TMP_DIR}/repo.tar.gz"

    if command -v curl >/dev/null 2>&1; then
        if [ -n "${token}" ]; then
            curl -fsSL -H "Authorization: token ${token}" -o "${tar_path}" "${tar_url}"
        else
            curl -fsSL -o "${tar_path}" "${tar_url}"
        fi
    elif command -v wget >/dev/null 2>&1; then
        if [ -n "${token}" ]; then
            wget --header="Authorization: token ${token}" -O "${tar_path}" "${tar_url}"
        else
            wget -O "${tar_path}" "${tar_url}"
        fi
    else
        echo "curl or wget is required for tarball fallback."
        exit 1
    fi

    local root_dir
    root_dir="$(tar -tzf "${tar_path}" | head -1 | cut -d/ -f1)"
    tar -xzf "${tar_path}" -C "${TMP_DIR}"
    mv "${TMP_DIR}/${root_dir}" "${dest}"
}

mkdir -p "${OUT_DIR}"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

"${PYTHON_BIN}" -m venv "${TMP_DIR}/venv"
source "${TMP_DIR}/venv/bin/activate"

python -m ensurepip --upgrade >/dev/null 2>&1 || true
python -m pip install --upgrade "pip==${PIP_VERSION}" "setuptools==${SETUPTOOLS_VERSION}" "wheel==${WHEEL_VERSION}"

clone_repo "${RPI_WS281X_PY_REPO}" "${RPI_WS281X_PY_REF}" "${TMP_DIR}/rpi_ws281x_python" "${RPI_WS281X_PY_TOKEN}"

LIB_DIR="${TMP_DIR}/rpi_ws281x_python/library/lib"
rm -rf "${LIB_DIR}"
mkdir -p "${LIB_DIR}"
cp -R "${REPO_ROOT}/." "${LIB_DIR}/"
rm -rf "${LIB_DIR}/.git"

cd "${TMP_DIR}/rpi_ws281x_python/library"
python -m pip wheel . -w "${OUT_DIR}" --no-deps

deactivate

echo "Built rpi_ws281x wheel(s) into ${OUT_DIR}"
