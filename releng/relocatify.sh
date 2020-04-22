#!/bin/bash

package=$1
prefix=$2
releng=$3
[ -z "$package" -o -z "$prefix" -o -z "$releng" ] && exit 1

build_platform=$(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$,macos,')

toolchain_prefix=$(dirname $(dirname $(which perl)))

shopt -s expand_aliases
if [ "$build_platform" = "macos" ]; then
  alias sed_inplace='sed -i ""'
else
  alias sed_inplace='sed -i'
fi

for file in $(find "$package" -type f); do
  if grep -q "$prefix" $file; then
    if echo "$file" | grep -Eq "\\.pm$|aclocal.*|autoconf|autoheader|autom4te.*|automake.*|autopoint|autoreconf|autoscan|autoupdate|gdbus-codegen|gettextize|lib/gettext/user-email|/glib-2.0/codegen/|/gdb/auto-load/|ifnames|libtool|bin/vala-gen-introspect"; then
      newname="$file.frida.in"
      mv "$file" "$newname"
      sed_inplace \
        -e "s,$prefix-,,g" \
        -e "s,$prefix,@FRIDA_TOOLROOT@,g" \
        -e "s,$releng,@FRIDA_RELENG@,g" \
        -e "s,$toolchain_prefix,/usr,g" \
        -e "s,/usr/bin/ld.gold,/usr/bin/ld,g" \
        $newname || exit 1
    elif echo "$file" | grep -Eq "\\.la$"; then
      newname="$file.frida.in"
      mv "$file" "$newname"
      sed_inplace \
        -e "s,$prefix-,,g" \
        -e "s,$prefix,@FRIDA_SDKROOT@,g" \
        -e "s,$releng,@FRIDA_RELENG@,g" \
        $newname || exit 1
    elif echo "$file" | grep -Eq "\\.pc$"; then
      sed_inplace \
        -e "s,$prefix,\${frida_sdk_prefix},g" \
        $file || exit 1
    fi
  fi
done
