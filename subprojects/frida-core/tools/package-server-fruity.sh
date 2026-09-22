#!/bin/sh

if [ -z "$FRIDA_VERSION" ]; then
  echo "FRIDA_VERSION must be set" > /dev/stderr
  exit 2
fi

if [ $# -ne 3 ]; then
  echo "Usage: $0 arch path/to/prefix output.deb" > /dev/stderr
  exit 3
fi
arch=$1
prefix=$2
output_deb=$3

executable=$prefix/usr/bin/frida-server
if [ ! -f "$executable" ]; then
  echo "$executable: not found" > /dev/stderr
  exit 4
fi

agent=$prefix/usr/lib/frida-1.0/frida-agent.dylib
if [ ! -f "$agent" ]; then
  echo "$agent: not found" > /dev/stderr
  exit 5
fi

if [ "$arch" = "iphoneos-arm64" ]; then
  rootless=1
else
  rootless=0
fi

if [ $rootless -eq 1 ]; then
  sysroot=/var/jb
else
  sysroot=""
fi

tmpdir="$(mktemp -d /tmp/package-server.XXXXXX)"

pkroot=$tmpdir$sysroot
bindir=$pkroot/usr/sbin
libdir=$pkroot/usr/lib/frida-1.0
daedir=$pkroot/Library/LaunchDaemons

mkdir -p "$bindir/"
cp "$executable" "$bindir/frida-server"
chmod 755 "$bindir/frida-server"

mkdir -p "$libdir/"
cp "$agent" "$libdir/frida-agent.dylib"
chmod 755 "$libdir/frida-agent.dylib"

mkdir -p "$daedir/"
(
  echo '<?xml version="1.0" encoding="UTF-8"?>'
  echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
  echo '<plist version="1.0">'
  echo "<dict>"
  echo "	<key>Label</key>"
  echo "	<string>re.frida.server</string>"
  echo "	<key>Program</key>"
  echo "	<string>$sysroot/usr/sbin/frida-server</string>"
  echo "	<key>ProgramArguments</key>"
  echo "	<array>"
  echo "		<string>$sysroot/usr/sbin/frida-server</string>"
  echo "	</array>"
  if [ $rootless -eq 0 ]; then
    echo "	<key>EnvironmentVariables</key>"
    echo "	<dict>"
    echo "		<key>_MSSafeMode</key>"
    echo "		<string>1</string>"
    echo "	</dict>"
  fi
  echo "	<key>UserName</key>"
  echo "	<string>root</string>"
  echo "	<key>POSIXSpawnType</key>"
  echo "	<string>Interactive</string>"
  echo "	<key>RunAtLoad</key>"
  echo "	<true/>"
  if [ $rootless -eq 0 ]; then
    echo "	<key>LimitLoadToSessionType</key>"
    echo "	<string>System</string>"
  fi
  echo "	<key>KeepAlive</key>"
  echo "	<true/>"
  echo "	<key>ThrottleInterval</key>"
  echo "	<integer>5</integer>"
  echo "	<key>ExecuteAllowed</key>"
  echo "	<true/>"
  echo "</dict>"
  echo "</plist>"
) > "$daedir/re.frida.server.plist"
chmod 644 "$daedir/re.frida.server.plist"

installed_size=$(du -sk "$tmpdir" | cut -f1)

mkdir -p "$tmpdir/DEBIAN/"
cat >"$tmpdir/DEBIAN/control" <<EOF
Package: re.frida.server
Name: Frida
Version: $FRIDA_VERSION
Priority: optional
Size: 1337
Installed-Size: $installed_size
Architecture: $arch
Description: Observe and reprogram running programs.
Homepage: https://frida.re/
Maintainer: Ole André Vadla Ravnås <oleavr@nowsecure.com>
Author: Frida Developers <oleavr@nowsecure.com>
Section: Development
Conflicts: re.frida.server64
EOF
chmod 644 "$tmpdir/DEBIAN/control"

cat >"$tmpdir/DEBIAN/extrainst_" <<EOF
#!/bin/bash

launchcfg=$sysroot/Library/LaunchDaemons/re.frida.server.plist
launchlog=\$(mktemp)

function dispose {
  rm -f "\$launchlog"
}
trap dispose EXIT

if [ "\$1" = upgrade ]; then
  launchctl unload "\$launchcfg" &> /dev/null
fi

if [ "\$1" = install ] || [ "\$1" = upgrade ]; then
  launchctl load "\$launchcfg" &> "\$launchlog"
  res=\$?

  if grep -q "Service cannot load in requested session" "\$launchlog"; then
    sed -ie "/LimitLoadToSessionType/,+1d" "\$launchcfg"
    launchctl load "\$launchcfg" &> "\$launchlog"
    res=\$?
  fi

  if [ \$res -ne 0 ]; then
    cat "\$launchlog" > /dev/stderr
    exit \$res
  fi
fi

exit 0
EOF
chmod 755 "$tmpdir/DEBIAN/extrainst_"
cat >"$tmpdir/DEBIAN/prerm" <<EOF
#!/bin/bash

if [ "\$1" = remove ] || [ "\$1" = purge ]; then
  launchctl unload $sysroot/Library/LaunchDaemons/re.frida.server.plist &> /dev/null
fi

exit 0
EOF
chmod 755 "$tmpdir/DEBIAN/prerm"

dpkg_options="-Zxz --root-owner-group"

dpkg-deb $dpkg_options --build "$tmpdir" "$output_deb"
package_size=$(expr $(du -sk "$output_deb" | cut -f1) \* 1024)

sed \
  -e "s,^Size: 1337$,Size: $package_size,g" \
  "$tmpdir/DEBIAN/control" > "$tmpdir/DEBIAN/control_"
mv "$tmpdir/DEBIAN/control_" "$tmpdir/DEBIAN/control"
dpkg-deb $dpkg_options --build "$tmpdir" "$output_deb"

rm -rf "$tmpdir"
