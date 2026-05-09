#!/usr/bin/env bash
set -euo pipefail

qt_tag="${QT_TAG:-v6.11.0}"
qt_url="${QT_URL:-https://code.qt.io/qt/qt5.git}"
qt_dir="${QT_DIR:-third_party/qt}"
module_subset="${QT_MODULE_SUBSET:-qtbase}"

if [[ ! -d .git ]]; then
    echo "Run this script from the repository root." >&2
    exit 1
fi

if [[ -d "${qt_dir}/.git" || -f "${qt_dir}/.git" ]]; then
    echo "Using existing Qt checkout at ${qt_dir}."
elif git config -f .gitmodules --get "submodule.${qt_dir}.path" >/dev/null 2>&1; then
    git submodule update --init --checkout "${qt_dir}"
else
    git submodule add "${qt_url}" "${qt_dir}"
fi

(
    cd "${qt_dir}"
    git fetch --tags origin
    git switch --detach "${qt_tag}"
    ./init-repository --module-subset="${module_subset}"
)

echo "Qt is initialized at ${qt_dir} on ${qt_tag} with module subset '${module_subset}'."
