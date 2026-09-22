#!/bin/sh

host_os="$1"
runner_binary="$2"
runner_entitlements="$3"
signed_runner_binary="$4"

if [ -z "$CODESIGN" ]; then
  CODESIGN=codesign
fi

if [ -z "$MACOS_CERTID" ]; then
  MACOS_CERTID="-"
fi

if [ -z "$IOS_CERTID" ]; then
  IOS_CERTID="-"
fi

rm -f "$signed_runner_binary"
cp "$runner_binary" "$signed_runner_binary"

case $host_os in
  macos)
    "$CODESIGN" -f -s "$MACOS_CERTID" -i "re.frida.GumTests" "$signed_runner_binary" || exit 1
    ;;
  ios)
    "$CODESIGN" -f -s "$IOS_CERTID" --entitlements "$runner_entitlements" "$signed_runner_binary" || exit 1
    ;;
  tvos)
    "$CODESIGN" -f -s "$TVOS_CERTID" --entitlements "$runner_entitlements" "$signed_runner_binary" || exit 1
    ;;
esac
