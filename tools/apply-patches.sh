#!/bin/bash
#
# Apply stealth patches to submodules
# Run this after cloning or after syncing upstream
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PATCHES_DIR="$ROOT_DIR/patches"

if [ ! -d "$PATCHES_DIR" ]; then
    echo "[x] No patches directory found. Run generate-patches.sh first."
    exit 1
fi

echo "[*] Applying frida-gum patch..."
cd "$ROOT_DIR/subprojects/frida-gum"
if git apply --check "$PATCHES_DIR/frida-gum-stealth.patch" 2>/dev/null; then
    git apply "$PATCHES_DIR/frida-gum-stealth.patch"
    echo "    Applied successfully"
else
    echo "    Attempting 3-way merge..."
    git apply --3way "$PATCHES_DIR/frida-gum-stealth.patch" || {
        echo "[!] Patch failed. Manual resolution needed."
        echo "    cd subprojects/frida-gum"
        echo "    git apply --reject ../../patches/frida-gum-stealth.patch"
        exit 1
    }
fi

echo "[*] Applying frida-core patch..."
cd "$ROOT_DIR/subprojects/frida-core"
if git apply --check "$PATCHES_DIR/frida-core-stealth.patch" 2>/dev/null; then
    git apply "$PATCHES_DIR/frida-core-stealth.patch"
    echo "    Applied successfully"
else
    echo "    Attempting 3-way merge..."
    git apply --3way "$PATCHES_DIR/frida-core-stealth.patch" || {
        echo "[!] Patch failed. Manual resolution needed."
        echo "    cd subprojects/frida-core"
        echo "    git apply --reject ../../patches/frida-core-stealth.patch"
        exit 1
    }
fi

echo "[*] All patches applied."
