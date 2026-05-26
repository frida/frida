#!/bin/bash
#
# Frida Stealth Deploy Script
# Push compiled binaries to Android device and start server
#
# Usage:
#   ./deploy-stealth.sh [command]
#
# Commands:
#   push      - Push binaries to device (default)
#   start     - Start frida-server on device
#   stop      - Stop frida-server on device
#   status    - Check if server is running
#   verify    - Run detection verification checks
#   clean     - Remove frida from device
#   all       - push + start + verify
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
INFO_FILE="$BUILD_DIR/.stealth-info"

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[*]${NC} $1"; }
warn()  { echo -e "${YELLOW}[!]${NC} $1"; }
error() { echo -e "${RED}[x]${NC} $1"; exit 1; }
pass()  { echo -e "${GREEN}[PASS]${NC} $1"; }
fail()  { echo -e "${RED}[FAIL]${NC} $1"; }

# --- Load Build Info ---
if [ ! -f "$INFO_FILE" ]; then
    error "Build info not found. Run build-stealth.sh first."
fi
source "$INFO_FILE"

DEVICE_DIR="/data/local/tmp"
SERVER_BIN="$BUILD_DIR/subprojects/frida-core/server/$SERVER_NAME"
GADGET_LIB="$BUILD_DIR/subprojects/frida-core/lib/gadget/lib${GADGET_NAME}.so"

COMMAND="${1:-push}"

# --- Check ADB ---
check_adb() {
    if ! command -v adb &>/dev/null; then
        error "adb not found in PATH"
    fi
    if ! adb get-state &>/dev/null; then
        error "No device connected. Check USB/WiFi connection."
    fi
}

# --- Commands ---
do_push() {
    check_adb
    info "Pushing binaries to device..."

    if [ -f "$SERVER_BIN" ]; then
        info "  Server: $SERVER_NAME -> $DEVICE_DIR/$SERVER_NAME"
        adb push "$SERVER_BIN" "$DEVICE_DIR/$SERVER_NAME"
        adb shell chmod 755 "$DEVICE_DIR/$SERVER_NAME"
    fi

    if [ -f "$GADGET_LIB" ]; then
        info "  Gadget: lib${GADGET_NAME}.so -> $DEVICE_DIR/lib${GADGET_NAME}.so"
        adb push "$GADGET_LIB" "$DEVICE_DIR/lib${GADGET_NAME}.so"
    fi

    info "Push complete."
}

do_start() {
    check_adb
    info "Starting server on device (port $PORT)..."

    # Kill existing instance
    adb shell "su -c 'pkill -f $SERVER_NAME'" 2>/dev/null || true
    sleep 0.5

    # Start in background
    adb shell "su -c 'nohup $DEVICE_DIR/$SERVER_NAME &'" &
    sleep 1

    # Verify
    if adb shell "su -c 'pidof $SERVER_NAME'" &>/dev/null; then
        local pid=$(adb shell "su -c 'pidof $SERVER_NAME'" | tr -d '\r')
        info "Server started (PID: $pid, Port: $PORT)"
    else
        error "Failed to start server"
    fi
}

do_stop() {
    check_adb
    info "Stopping server..."
    adb shell "su -c 'pkill -f $SERVER_NAME'" 2>/dev/null || true
    info "Server stopped."
}

do_status() {
    check_adb
    local pid=$(adb shell "su -c 'pidof $SERVER_NAME'" 2>/dev/null | tr -d '\r')
    if [ -n "$pid" ]; then
        info "Server is running (PID: $pid)"
        adb shell "su -c 'cat /proc/$pid/comm'" 2>/dev/null | tr -d '\r'
    else
        warn "Server is not running"
    fi
}

do_verify() {
    check_adb
    local pid=$(adb shell "su -c 'pidof $SERVER_NAME'" 2>/dev/null | tr -d '\r')

    if [ -z "$pid" ]; then
        error "Server not running. Start it first."
    fi

    echo ""
    info "=== Detection Verification (PID: $pid) ==="
    echo ""

    # Test 1: maps string scan
    info "Test 1: /proc/self/maps - frida string"
    local maps_hit=$(adb shell "su -c 'cat /proc/$pid/maps'" 2>/dev/null | grep -ic "frida" || true)
    if [ "$maps_hit" = "0" ]; then
        pass "No 'frida' string in maps"
    else
        fail "Found 'frida' in maps ($maps_hit occurrences)"
    fi

    # Test 2: anonymous executable segments
    info "Test 2: /proc/self/maps - anonymous executable segments"
    local anon_exec=$(adb shell "su -c 'cat /proc/$pid/maps'" 2>/dev/null | grep "r-xp" | grep "00:00 0" | grep -v "\[" | wc -l | tr -d ' ')
    if [ "$anon_exec" = "0" ]; then
        pass "No anonymous executable segments"
    else
        warn "Found $anon_exec anonymous executable segments (may include non-Frida)"
    fi

    # Test 3: memfd segments
    info "Test 3: /proc/self/maps - memfd segments"
    local memfd_count=$(adb shell "su -c 'cat /proc/$pid/maps'" 2>/dev/null | grep -c "memfd:" || true)
    if [ "$memfd_count" -gt "0" ]; then
        pass "Found $memfd_count memfd segments (expected)"
    else
        warn "No memfd segments found"
    fi

    # Test 4: thread names
    info "Test 4: Thread names"
    local thread_hit=$(adb shell "su -c 'ls /proc/$pid/task/*/comm'" 2>/dev/null | xargs -I{} adb shell "su -c 'cat {}'" 2>/dev/null | grep -ic "frida\|gum-js-loop\|gmain" || true)
    if [ "$thread_hit" = "0" ]; then
        pass "No detectable thread names"
    else
        fail "Found $thread_hit detectable thread names"
    fi

    # Test 5: default port
    info "Test 5: Port 27042 (default frida port)"
    local port_hit=$(adb shell "su -c 'cat /proc/net/tcp /proc/net/tcp6'" 2>/dev/null | grep -ic "$(printf '%04X' 27042)" || true)
    if [ "$port_hit" = "0" ]; then
        pass "Port 27042 not in use"
    else
        fail "Port 27042 is listening"
    fi

    # Test 6: custom port
    info "Test 6: Custom port $PORT"
    local custom_port_hex=$(printf '%04X' $PORT)
    local custom_hit=$(adb shell "su -c 'cat /proc/net/tcp /proc/net/tcp6'" 2>/dev/null | grep -ic "$custom_port_hex" || true)
    if [ "$custom_hit" -gt "0" ]; then
        pass "Custom port $PORT is listening (expected)"
    else
        warn "Custom port $PORT not found in tcp table"
    fi

    # Test 7: D-Bus probe
    info "Test 7: D-Bus protocol probe"
    local dbus_response=$(adb shell "su -c 'echo -ne \"\\x00AUTH\\r\\n\" | nc -w 1 127.0.0.1 $PORT'" 2>/dev/null | tr -d '\r\n')
    if echo "$dbus_response" | grep -qi "REJECT"; then
        fail "D-Bus REJECTED response detected"
    elif echo "$dbus_response" | grep -qi "404"; then
        pass "Got HTTP 404 (magic protection active)"
    elif [ -z "$dbus_response" ]; then
        pass "No response (connection closed or magic required)"
    else
        pass "Non-D-Bus response: ${dbus_response:0:30}"
    fi

    # Test 8: process name
    info "Test 8: Process name in /proc"
    local proc_hit=$(adb shell "su -c 'ls /proc/*/comm'" 2>/dev/null | xargs -I{} adb shell "su -c 'cat {}'" 2>/dev/null | grep -ic "frida" || true)
    if [ "$proc_hit" = "0" ]; then
        pass "No 'frida' process names"
    else
        fail "Found $proc_hit processes with 'frida' in name"
    fi

    echo ""
    info "=== Verification Complete ==="
}

do_clean() {
    check_adb
    info "Removing frida from device..."
    adb shell "su -c 'pkill -f $SERVER_NAME'" 2>/dev/null || true
    adb shell "su -c 'rm -f $DEVICE_DIR/$SERVER_NAME'" 2>/dev/null || true
    adb shell "su -c 'rm -f $DEVICE_DIR/lib${GADGET_NAME}.so'" 2>/dev/null || true
    adb shell "su -c 'rm -f $DEVICE_DIR/lib${GADGET_NAME}.config.so'" 2>/dev/null || true
    info "Cleaned."
}

do_all() {
    do_push
    echo ""
    do_start
    echo ""
    sleep 2
    do_verify
}

# --- Execute ---
case "$COMMAND" in
    push)   do_push ;;
    start)  do_start ;;
    stop)   do_stop ;;
    status) do_status ;;
    verify) do_verify ;;
    clean)  do_clean ;;
    all)    do_all ;;
    *)      error "Unknown command: $COMMAND. Use: push, start, stop, status, verify, clean, all" ;;
esac
