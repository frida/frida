; Innosetup file for My app.

[Setup]
AppName=My App
AppVersion=1.0
DefaultDirName={pf}\My App
DefaultGroupName=My App
UninstallDisplayIcon={app}\myapp.exe
Compression=lzma2
SolidCompression=yes
OutputDir=.

[Files]
Source: "myapp.exe"; DestDir: "{app}"
Source: "SDL2.dll"; DestDir: "{app}"

;[Icons]
;Name: "{group}\My App"; Filename: "{app}\myapp.exe"
