#!/bin/sh

instruments_profile=$1
agent_base=$2
tests_base=$3
if [ -z "$instruments_profile" -o -z "$agent_base" -o -z "$tests_base" ]; then
  echo "Usage: $0 <instruments-profile> <agent-base> <tests-base> <output-svg>" > /dev/stderr
  exit 1
fi

tests=$(dirname "$0")
repo=$(dirname $(dirname $(dirname "$tests")))
buildpfx=../build/tmp-macos-arm64
flamegraph=~/src/FlameGraph

cd "$repo"

set -ex

intdir=$(mktemp -d /tmp/mkflamegraph.XXXXXX)
stacks_symbolicated=$intdir/stacks_symbolicated
stacks_folded=$intdir/stacks_folded
stacks_deduped=$intdir/stacks_deduped

clean_up () {
  rm -rf "$intdir"
}
trap clean_up EXIT

"$repo/tools/symbolicate.py" \
  --input "$instruments_profile" \
  --output "$stacks_symbolicated" \
  --declare-module $buildpfx/frida-core/lib/agent/libfrida-agent-modulated.dylib:$agent_base \
  --declare-module $buildpfx/frida-gum/tests/core/swiftapiresolver/libtestswiftapiresolver.dylib:$tests_base
"$flamegraph/stackcollapse-instruments.pl" "$stacks_symbolicated" \
  | grep gum_script_scheduler_run_js_loop \
  > "$stacks_folded"
"$repo/tools/stackdedupe.py" \
  --input "$stacks_folded" \
  --output "$stacks_deduped"
"$flamegraph/flamegraph.pl" "$stacks_deduped"
