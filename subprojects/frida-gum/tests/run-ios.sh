#!/bin/sh

remote_host=iphone
remote_prefix=/usr/local/opt/frida-tests

set -e

make

cd build/tests

ssh "$remote_host" "mkdir -p '$remote_prefix'"
rsync -rLz gum-tests data "$remote_host:$remote_prefix/" || exit 1

ssh "$remote_host" "rm -f /var/mobile/Library/Logs/CrashReporter/gum-tests-*"

log_path=$(mktemp "$TMPDIR/gum-tests.XXXXXX")
ssh "$remote_host" "$remote_prefix/gum-tests" "$@" | tee "$log_path"

if [ ${PIPESTATUS[0]} -ne 0 ]; then
  while ! ssh "$remote_host" "ls /var/mobile/Library/Logs/CrashReporter/gum-tests-*" 2>/dev/null; do
    sleep 1
  done

  remote_report_path=$(ssh "$remote_host" "echo /var/mobile/Library/Logs/CrashReporter/gum-tests-*")
  local_report_path=$(mktemp "$TMPDIR/gum-tests.XXXXXX")
  scp "$remote_host:$remote_report_path" "$local_report_path"

  program_base=$(egrep "^(0x.+)\sgum-tests" "$local_report_path" | awk '{ print $1; }')

  cat "$local_report_path"

  echo "Crash:"
  egrep "^\d+\s+gum-tests" "$local_report_path" \
    | awk '{ print $3; }' \
    | xargs atos -o gum-tests -l $program_base

  rm "$local_report_path"
  ssh "$remote_host" "rm $remote_report_path"
fi

rm "$log_path"
