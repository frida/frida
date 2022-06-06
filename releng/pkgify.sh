#!/usr/bin/env bash

package=$1
prefix=$2
releng=$3
[ -z "$package" -o -z "$prefix" -o -z "$releng" ] && exit 1

build_os=$("$releng/detect-os.sh")

shopt -s expand_aliases
case $build_os in
  macos|freebsd)
    alias sed_inplace='sed -i ""'
    ;;
  *)
    alias sed_inplace='sed -i'
    ;;
esac

for file in $(find "$package" -type f); do
  if grep -q "$prefix" $file; then
    if echo "$file" | grep -Eq "glib-gettextize|gdbus-codegen|/glib-2.0/codegen/|bin/vala-gen-introspect"; then
      newname="$file.frida.in"
      mv "$file" "$newname"
      sed_inplace \
        -e "s,$prefix,@FRIDA_TOOLROOT@,g" \
        -e "s,$releng,@FRIDA_RELENG@,g" \
        $newname || exit 1
    elif echo "$file" | grep -Eq "\\.pc$"; then
      sed_inplace \
        -e "s,$prefix,\${frida_sdk_prefix},g" \
        $file || exit 1
    fi
  fi
done

cd "$package" || exit 1
for pkg in manifest/*.pkg; do
  cat $pkg | while read entry; do
    [ -e $entry ] && echo $entry >> $pkg.filtered
  done
  if [ -f $pkg.filtered ]; then
    mv $pkg.filtered $pkg
  else
    rm $pkg
  fi
done
