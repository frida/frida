#!/bin/bash
#
# Generate patch files from submodule changes
# Run this after making changes to submodules
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PATCHES_DIR="$ROOT_DIR/patches"

mkdir -p "$PATCHES_DIR"

echo "[*] Generating frida-gum patch..."
cd "$ROOT_DIR/subprojects/frida-gum"
git diff > "$PATCHES_DIR/frida-gum-stealth.patch"
echo "    $(wc -l < "$PATCHES_DIR/frida-gum-stealth.patch") lines"

echo "[*] Generating frida-core patch..."
cd "$ROOT_DIR/subprojects/frida-core"
git diff > "$PATCHES_DIR/frida-core-stealth.patch"
echo "    $(wc -l < "$PATCHES_DIR/frida-core-stealth.patch") lines"

echo "[*] Patches saved to patches/"
echo ""
echo "To apply patches on a fresh clone:"
echo "  cd subprojects/frida-gum && git apply ../../patches/frida-gum-stealth.patch"
echo "  cd subprojects/frida-core && git apply ../../patches/frida-core-stealth.patch"
