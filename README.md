Inject JavaScript to explore native apps on Windows, Mac, Linux, iOS and Android.
===

## Dependencies

For the running the frida binaries (`frida-discover`, `frida-ps`, `frida-repl` and `frida-trace`) you need to have python 3.4 installed with the `colorama` package (`pip3 install colorama`).

## Building

### 64-bit Linux

    make

### Mac and iOS

First make a trusted codesigning certificate. You can use the guide at https://sourceware.org/gdb/wiki/BuildingOnDarwin in the section "Creating a certificate". You can use the name `frida-cert` instead of `gdb-cert` if you want to.

Next export the name of the created certificate to the environment variables `MAC_CERTID` and `IOS_CERTID` and run `make`:

    export MAC_CERTID=frida-cert
    export IOS_CERTID=frida-cert
    make

To ensure that OS X accepts the newly created certificate, restart the `taskgated` daemon:

    sudo killall taskgated

### Windows

    frida.sln

(Requires Visual Studio 2013.)

See [http://www.frida.re/docs/building/](http://www.frida.re/docs/building/)
for more details.

You may also download pre-built binaries for various operating systems from
[http://www.frida.re/](http://www.frida.re/).

## Learn more

Have a look at our [documentation](http://www.frida.re/docs/home/).
