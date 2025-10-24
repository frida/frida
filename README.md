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

Run:

    make

You may also invoke `./configure` first if you want to specify a `--prefix`, or
any other options.

### CLI tools

For running the Frida CLI tools, e.g. `frida`, `frida-ls-devices`, `frida-ps`,
`frida-kill`, `frida-trace`, `frida-discover`, etc., you need a few packages:

    pip install colorama prompt-toolkit pygments

### Apple OSes

First make a trusted code-signing certificate. If you have already used Xcode
before, chances are you already have an Apple development certificate.
You can check it with the following command:

    security find-identity -v -p codesigning

Which will return the certificate in the following format:

    1) XXXXX "Apple Development: user@mail.com (XXXXX)"

If you do not have a certificate, follow this guide: 
https://help.apple.com/xcode/mac/current/#/dev154b28f09.

Next export the name of your certificate to relevant environment
variables, and run `make`:

    export MACOS_CERTID="Apple Development: user@mail.com (XXXXXXXXXX)"
    export IOS_CERTID="Apple Development: user@mail.com (XXXXXXXXXX)"
    export WATCHOS_CERTID="Apple Development: user@mail.com (XXXXXXXXXX)"
    export TVOS_CERTID="Apple Development: user@mail.com (XXXXXXXXXX)"
    make

## Learn more

Have a look at our [documentation](https://frida.re/docs/home/).
