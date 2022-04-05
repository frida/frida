# Frida

Dynamic instrumentation toolkit for developers, reverse-engineers, and security
researchers. Learn more at [frida.re](https://frida.re/).

Two ways to install
===================

## 1. Install from prebuilt binaries

This is the recommended way to get started. All you need to do is:

    pip install frida-tools # CLI tools
    pip install frida       # Python bindings
    npm install frida       # Node.js bindings

You may also download pre-built binaries for various operating systems from
Frida's [releases](https://github.com/frida/frida/releases) page on GitHub.

## 2. Build your own binaries

### Dependencies

For running the Frida CLI tools, i.e. `frida`, `frida-ls-devices`, `frida-ps`,
`frida-kill`, `frida-trace`, and `frida-discover`, you need Python plus a
few packages:

    pip3 install colorama prompt-toolkit pygments

_You might get errors from the `read-elf` if your OS is not
in English, since some of the tooling here uses regex with English words,
changing the language to `en_US` helps with it._
### Linux

    make

### macOS and iOS

First make a trusted code-signing certificate. You can use the guide at
https://sourceware.org/gdb/wiki/PermissionsDarwin in the sections
“Create a certificate in the System Keychain” and “Trust the certificate
for code signing”. You can use the name `frida-cert` instead of `gdb-cert`
if you'd like.

Next export the name of the created certificate to the environment variables
`MACOS_CERTID` and `IOS_CERTID`, and run `make`:

    export MACOS_CERTID=frida-cert
    export IOS_CERTID=frida-cert
    make

To ensure that macOS accepts the newly created certificate, restart the
`taskgated` daemon:

    sudo killall taskgated

### Windows

    frida.sln

(Requires Visual Studio 2019.)

See [https://frida.re/docs/building/](https://frida.re/docs/building/)
for details.

## Learn more

Have a look at our [documentation](https://frida.re/docs/home/).
