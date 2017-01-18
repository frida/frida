function Install-Python2Bits()
{
    $version = "2.7.9"
    $base = "https://www.python.org/ftp/python/$version"

    $staging = "$pwd\staging"
    mkdir $staging | Out-Null

    wget "$base/python-$version.msi" -out "$staging\python-$version.msi"
    wget "$base/python-$version.amd64.msi" -out "$staging\python-$version.amd64.msi"

    $targetBase = "$pwd\python\$($version.Substring(0, 3))\"
    msiexec /qb /a $staging\python-$version.msi TARGETDIR="$staging\stage2\x86" | Out-Null
    msiexec /qb /a $staging\python-$version.amd64.msi TARGETDIR="$staging\stage2\x64" | Out-Null

    mkdir $targetBase\x86\libs | Out-Null
    mv $staging\stage2\x86\libs\python*.lib $targetBase\x86\libs
    mv $staging\stage2\x86\include $targetBase\x86

    mkdir $targetBase\x64\libs | Out-Null
    mv $staging\stage2\x64\libs\python*.lib $targetBase\x64\libs
    mv $staging\stage2\x64\include $targetBase\x64

    del staging -recurse -force
}

function Install-Python3Bits()
{
    $version = "3.6.0"
    $base = "https://www.python.org/ftp/python/$version/"

    $staging = "$pwd\staging"
    mkdir $staging | Out-Null

    wget "$base/win32/dev.msi" -out "$staging\dev.msi"
    wget "$base/amd64/dev.msi" -out "$staging\dev.amd64.msi"

    $targetBase = "$pwd\python\$($version.Substring(0, 3))"
    msiexec /qb /a $staging\dev.msi TARGETDIR="$targetBase\x86" | Out-Null
    msiexec /qb /a $staging\dev.amd64.msi TARGETDIR="$targetBase\x64" | Out-Null

    del $targetBase\x86\*.msi
    del $targetBase\x64\*.msi
    del staging -recurse -force
}

Install-Python2Bits
Install-Python3Bits
