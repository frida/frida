#!/bin/bash

remote_host=iphone
remote_prefix=/usr/local/opt/frida-tests

gadget_stripped=$1
gadget_unstripped=$2
if [ -z "$gadget_stripped" -o -z "$gadget_unstripped" ]; then
  echo "Usage: $0 path/to/stripped/gadget path/to/unstripped/gadget" > /dev/stderr
  exit 1
fi

set -e

make

cd build/tests

ssh "$remote_host" "mkdir -p '$remote_prefix'"
rsync -rLz gum-tests data core/mapper-test $gadget_stripped "$remote_host:$remote_prefix/" || exit 1

ssh "$remote_host" "rm -f /var/mobile/Library/Logs/CrashReporter/mapper-test-*"

log_path=$(mktemp "$TMPDIR/mapper-test.XXXXXX")
ssh "$remote_host" "$remote_prefix/mapper-test $remote_prefix/frida-gadget.dylib" | tee "$log_path"

if [ ${PIPESTATUS[0]} -ne 0 ]; then
  while ! ssh "$remote_host" "ls /var/mobile/Library/Logs/CrashReporter/mapper-test-*" 2>/dev/null; do
    sleep 1
  done

  remote_report_path=$(ssh "$remote_host" "echo /var/mobile/Library/Logs/CrashReporter/mapper-test-*")
  local_report_path=$(mktemp "$TMPDIR/mapper-test.XXXXXX")
  scp "$remote_host:$remote_report_path" "$local_report_path"

  program_base=$(egrep "^(0x.+)\smapper-test" "$local_report_path" | awk '{ print $1; }')
  gadget_base=$(egrep "^Base address: 0x" "$log_path" | awk '{ print $3; }')

  cat "$local_report_path"

  echo "Crash:"
  egrep "^\d+\s+mapper-test" "$local_report_path" \
    | awk '{ print $3; }' \
    | xargs atos -o core/mapper-test -l $program_base
  egrep "^\d+\s+\\?\\?\\?" "$local_report_path" \
    | awk '{ print $3; }' \
    | xargs atos -o $gadget_unstripped -l $gadget_base

  rm "$local_report_path"
  ssh "$remote_host" "rm $remote_report_path"
fi

rm "$log_path"
