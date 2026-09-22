import frida from "frida";
import { inspect } from "util";

async function main() {
    const device = await frida.getUsbDevice();

    const [stdoutSocket, stderrSocket] = await Promise.all([createStdioSocket(device), createStdioSocket(device)]);

    stdoutSocket.stream.pipe(process.stdout);
    stderrSocket.stream.pipe(process.stderr);

    const appservice = await device.openService("xpc:com.apple.coredevice.appservice");
    const response = await appservice.request({
        "CoreDevice.featureIdentifier": "com.apple.coredevice.feature.launchapplication",
        "CoreDevice.action": {},
        "CoreDevice.input": {
            applicationSpecifier: {
                bundleIdentifier: {
                    _0: "no.oleavr.HelloIOS"
                },
            },
            options: {
                arguments: [],
                environmentVariables: {},
                standardIOUsesPseudoterminals: true,
                startStopped: false,
                terminateExisting: true,
                user: {
                    active: true
                },
                platformSpecificOptions: Buffer.from("<?xml version=\"1.0\" encoding=\"UTF-8\"?><plist version=\"1.0\"><dict/></plist>"),
            },
            standardIOIdentifiers: {
                standardOutput: [Symbol("uuid"), stdoutSocket.uuid],
                standardError: [Symbol("uuid"), stderrSocket.uuid]
            }
        },
    });
    console.log(inspect(response, {
        colors: true,
        depth: Infinity,
        maxArrayLength: Infinity
    }));
}

async function createStdioSocket(device: frida.Device): Promise<StdioSocket> {
    const stream = await device.openChannel("tcp:com.apple.coredevice.openstdiosocket");
    return new Promise((resolve, reject) => {
        let uuid = Buffer.alloc(0);

        stream.addListener("data", onData);
        stream.addListener("end", onEnd);

        function onData(chunk: Buffer) {
            uuid = Buffer.concat([uuid, chunk]);
            if (uuid.length === 16) {
                stream.removeListener("end", onEnd);
                stream.removeListener("data", onData);
                resolve({ uuid, stream });
            }
        }

        function onEnd() {
            reject(new Error("Stdio socket closed prematurely"));
        }
    });
}

interface StdioSocket {
    uuid: Buffer;
    stream: NodeJS.ReadWriteStream;
}

main()
    .catch(e => {
        console.error(e);
    });
