#!/bin/bash
#
# Frida Stealth Build Script
# One-click build with anti-detection optimizations
#
# Usage:
#   ./build-stealth.sh [target] [preset]
#
# Targets:
#   android-arm64  (default)
#   android-arm
#   android-x86_64
#   linux-x86_64
#   ios-arm64
#
# Presets:
#   full       - Maximum stealth, all features (default)
#   minimal    - Only rename binaries and change port
#   gadget     - Gadget-only build, no server
#   custom     - Read from stealth.conf file
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- Defaults ---
TARGET="${1:-android-arm64}"
PRESET="${2:-full}"
CONF_FILE="$SCRIPT_DIR/stealth.conf"
BUILD_DIR="$ROOT_DIR/build"

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[*]${NC} $1"; }
warn()  { echo -e "${YELLOW}[!]${NC} $1"; }
error() { echo -e "${RED}[x]${NC} $1"; exit 1; }

# --- Stealth Presets ---
declare -A STEALTH

load_preset_full() {
    STEALTH[memfd_name]="jit-cache"
    STEALTH[thread_js]="Signal Catcher"
    STEALTH[server_name]="media.codec"
    STEALTH[helper_name]="media.extractor"
    STEALTH[gadget_name]="libhwui"
    STEALTH[port]=52173
    STEALTH[thread_main]="HwBinder:1"
    STEALTH[thread_gadget]="RenderThread"
    STEALTH[server_dir]="com.android.providers.media"
    STEALTH[magic]="$(openssl rand -hex 8 2>/dev/null || head -c 8 /dev/urandom | xxd -p)"
}

load_preset_minimal() {
    STEALTH[memfd_name]="jit-cache"
    STEALTH[thread_js]="gum-js-loop"
    STEALTH[server_name]="media_server"
    STEALTH[helper_name]="app_helper"
    STEALTH[gadget_name]="libutils_v2"
    STEALTH[port]=39821
    STEALTH[thread_main]="frida-main-loop"
    STEALTH[thread_gadget]="frida-gadget"
    STEALTH[server_dir]="com.android.media"
    STEALTH[magic]=""
}

load_preset_gadget() {
    STEALTH[memfd_name]="jit-cache"
    STEALTH[thread_js]="FinalizerDaemon"
    STEALTH[server_name]="frida-server"
    STEALTH[helper_name]="frida-helper"
    STEALTH[gadget_name]="libandroid_runtime"
    STEALTH[port]=27042
    STEALTH[thread_main]="Jit thread pool"
    STEALTH[thread_gadget]="ReferenceQueueD"
    STEALTH[server_dir]="re.frida.server"
    STEALTH[magic]=""
}

load_preset_custom() {
    if [ ! -f "$CONF_FILE" ]; then
        error "Custom preset requires $CONF_FILE. Run: cp tools/stealth.conf.example tools/stealth.conf"
    fi
    source "$CONF_FILE"
}

# --- Load Preset ---
info "Loading preset: $PRESET"
case "$PRESET" in
    full)    load_preset_full ;;
    minimal) load_preset_minimal ;;
    gadget)  load_preset_gadget ;;
    custom)  load_preset_custom ;;
    *)       error "Unknown preset: $PRESET. Use: full, minimal, gadget, custom" ;;
esac

# --- Print Configuration ---
echo ""
info "=== Stealth Build Configuration ==="
echo "  Target:        $TARGET"
echo "  Preset:        $PRESET"
echo "  Server name:   ${STEALTH[server_name]}"
echo "  Helper name:   ${STEALTH[helper_name]}"
echo "  Gadget name:   ${STEALTH[gadget_name]}"
echo "  Port:          ${STEALTH[port]}"
echo "  Thread JS:     ${STEALTH[thread_js]}"
echo "  Thread Main:   ${STEALTH[thread_main]}"
echo "  Thread Gadget: ${STEALTH[thread_gadget]}"
echo "  Memfd name:    ${STEALTH[memfd_name]}"
echo "  Server dir:    ${STEALTH[server_dir]}"
echo "  Magic:         ${STEALTH[magic]:-<disabled>}"
echo ""

# --- Apply Patches ---
PATCHES_DIR="$ROOT_DIR/patches"
if [ -d "$PATCHES_DIR" ] && ls "$PATCHES_DIR"/*.patch &>/dev/null; then
    info "Applying stealth patches to submodules..."
    "$SCRIPT_DIR/apply-patches.sh" || error "Patch apply failed. See above."
else
    warn "No patches found in patches/ — assuming submodules already patched"
fi

# --- Build Configure Arguments ---
CONFIGURE_ARGS="--host=$TARGET"

# Gadget-only mode
if [ "$PRESET" = "gadget" ]; then
    CONFIGURE_ARGS="$CONFIGURE_ARGS -Dserver=disabled -Dinject=disabled"
fi

# frida-gum options
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-gum:stealth_memfd_name=${STEALTH[memfd_name]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-gum:stealth_thread_js=${STEALTH[thread_js]}"

# frida-core options
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_server_name=${STEALTH[server_name]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_helper_name=${STEALTH[helper_name]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_gadget_name=${STEALTH[gadget_name]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_port=${STEALTH[port]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_thread_main=${STEALTH[thread_main]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_thread_gadget=${STEALTH[thread_gadget]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_server_dir=${STEALTH[server_dir]}"
CONFIGURE_ARGS="$CONFIGURE_ARGS -Dfrida-core:stealth_magic=${STEALTH[magic]}"

# --- Clean Previous Build ---
if [ -d "$BUILD_DIR" ]; then
    warn "Removing previous build directory..."
    rm -rf "$BUILD_DIR"
fi

# --- Configure ---
info "Configuring build..."
cd "$ROOT_DIR"
./configure $CONFIGURE_ARGS

# --- Build ---
info "Building (this may take 10-30 minutes)..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# --- Report Output ---
echo ""
info "=== Build Complete ==="
echo ""

SERVER_BIN="$BUILD_DIR/subprojects/frida-core/server/${STEALTH[server_name]}"
GADGET_LIB="$BUILD_DIR/subprojects/frida-core/lib/gadget/lib${STEALTH[gadget_name]}.so"
INJECT_BIN="$BUILD_DIR/subprojects/frida-core/inject/frida-inject"

if [ -f "$SERVER_BIN" ]; then
    info "Server:  $SERVER_BIN"
    ls -lh "$SERVER_BIN"
fi
if [ -f "$GADGET_LIB" ]; then
    info "Gadget:  $GADGET_LIB"
    ls -lh "$GADGET_LIB"
fi
if [ -f "$INJECT_BIN" ]; then
    info "Inject:  $INJECT_BIN"
    ls -lh "$INJECT_BIN"
fi

echo ""
info "Done. Use tools/deploy-stealth.sh to push to device."

# --- Save build info for deploy script ---
cat > "$BUILD_DIR/.stealth-info" <<EOF
TARGET=$TARGET
PRESET=$PRESET
SERVER_NAME=${STEALTH[server_name]}
HELPER_NAME=${STEALTH[helper_name]}
GADGET_NAME=${STEALTH[gadget_name]}
PORT=${STEALTH[port]}
MAGIC=${STEALTH[magic]}
EOF
