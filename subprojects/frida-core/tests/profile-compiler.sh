#!/bin/sh

tests=$(dirname "$0")
repo=$(dirname "$tests")
builddir=build
flamegraph=/Users/oleavr/src/FlameGraph

set -ex

intdir=$(mktemp -d /tmp/profile-compiler.XXXXXX)
test_log=$intdir/test.log
v8_log=$intdir/v8.log
stacks_raw=$intdir/stacks_raw
stacks_symbolicated=$intdir/stacks_symbolicated
stacks_folded=$intdir/stacks_folded

clean_up () {
  rm -rf "$intdir"
}
trap clean_up EXIT

export FRIDA_TEST_LOG=$test_log
export FRIDA_V8_EXTRA_FLAGS="--logfile=$v8_log --no-logfile-per-isolate --log-code --interpreted-frames-native-stack"

sudo --preserve-env=FRIDA_TEST_LOG,FRIDA_V8_EXTRA_FLAGS dtrace \
  -c "$builddir/tests/frida-tests -p /Compiler/Performance/build-simple-agent" \
  -x ustackframes=100 \
  -n 'profile-97 /pid == $target/ { @[ustack()] = count(); }' \
  -o "$stacks_raw"
"$repo/tools/symbolicate.py" \
  --input "$stacks_raw" \
  --output "$stacks_symbolicated" \
  --test-log "$test_log" \
  --v8-log "$v8_log" \
  --agent "$builddir/lib/agent/libfrida-agent-modulated.dylib"
"$flamegraph/stackcollapse.pl" "$stacks_symbolicated" \
  | grep gum_script_scheduler_run_js_loop \
  > "$stacks_folded"
"$flamegraph/flamegraph.pl" "$stacks_folded" > compiler-flamegraph.svg
