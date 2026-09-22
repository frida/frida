/*
 * Compile example.dylib like this:
 * $ clang -shared example.c -o ~/.Trash/example.dylib
 *
 * Then run:
 * $ node inject_file.js Twitter ~/.Trash/example.dylib
 */

import frida from "frida";

const [target, libraryPath] = process.argv.slice(2);

let device: frida.Device | null = null;

async function main() {
    device = await frida.getLocalDevice();
    device.uninjected.connect(onUninjected);

    try {
        const id = await device.injectLibraryFile(target, libraryPath, "example_main", "w00t");
        console.log("[*] Injected id:", id);
    } catch (e) {
        device.uninjected.disconnect(onUninjected);
        throw e;
    }
}

function onUninjected(id: frida.InjecteeID) {
    console.log("[*] onUninjected() id:", id);
    device!.uninjected.disconnect(onUninjected);
}

main()
    .catch(e => {
        console.error(e);
    });
