Inject JavaScript to explore native apps on Windows, Mac, Linux, iOS and Android.
===

## Installing from prebuilt binaries

    pip install frida # CLI tools and Python bindings
    npm install frida # Node.js bindings

You may also download pre-built binaries for various operating systems from
[http://www.frida.re/](http://www.frida.re/).

## Building

### Dependencies

For running the Frida tools (`frida`, `frida-ls-devices`, `frida-ps`,
`frida-trace`, and `frida-discover`) you need to have python installed with
the `colorama` package (`pip3 install colorama`).

### Linux

    make

### Mac and iOS

First make a trusted code-signing certificate. You can use the guide at
https://sourceware.org/gdb/wiki/BuildingOnDarwin in the section
"Creating a certificate". You can use the name `frida-cert` instead of
`gdb-cert` if you'd like.

Next export the name of the created certificate to the environment
variables `MAC_CERTID` and `IOS_CERTID` and run `make`:

    export MAC_CERTID=frida-cert
    export IOS_CERTID=frida-cert
    make

To ensure that OS X accepts the newly created certificate, restart the
`taskgated` daemon:

    sudo killall taskgated

### Windows

    frida.sln

(Requires Visual Studio 2013.)

See [http://www.frida.re/docs/building/](http://www.frida.re/docs/building/)
for more details.

## Learn more

Have a look at our [documentation](http://www.frida.re/docs/home/).
