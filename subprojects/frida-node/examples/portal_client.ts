import frida from "frida";
import readline from "readline";
import { inspect } from "util";

class Application {
    #nick: string;
    #channel: string | null = null;
    #prompt = "> ";

    #device: frida.Device | null = null;
    #bus: frida.Bus | null = null;
    #input: readline.Interface | null = null;

    constructor(nick: string) {
        this.#nick = nick;
    }

    async run() {
        const token = {
            nick: this.#nick,
            secret: "knock-knock"
        };
        this.#device = await frida.getDeviceManager().addRemoteDevice("::1", {
            token: JSON.stringify(token)
        });

        const bus = this.#device.bus;
        this.#bus = bus;
        bus.detached.connect(this.#onBusDetached);
        bus.message.connect(this.#onBusMessage);
        await bus.attach();

        const input = readline.createInterface({
            input: process.stdin,
            output: process.stdout,
            terminal: true
        });
        this.#input = input;
        input.on("close", this.#onStdinClosed);
        input.on("line", this.#onStdinCommand);

        this.#showPrompt();
    }

    #quit() {
        const bus = this.#bus;
        if (bus !== null) {
            bus.detached.disconnect(this.#onBusDetached);
            bus.message.disconnect(this.#onBusMessage);
            this.#bus = null;
        }

        const input = this.#input;
        if (input !== null) {
            input.close();
            this.#input = null;
        }
    }

    #onStdinClosed = () => {
        this.#quit();
    };

    #onStdinCommand = async (command: string) => {
        const bus = this.#bus!;

        try {
            process.stdout.write("\x1B[1A\x1B[K");

            if (command.length === 0) {
                this.#print("Processes:", await this.#device!.enumerateProcesses());
                return;
            }

            if (command.startsWith("/join ")) {
                if (this.#channel !== null) {
                    bus.post({
                        type: "part",
                        channel: this.#channel
                    });
                }

                const channel = command.substr(6);
                this.#channel = channel;

                this.#prompt = `${channel} > `;

                bus.post({
                    type: "join",
                    channel: channel
                });

                return;
            }

            if (command.startsWith("/announce ")) {
                bus.post({
                    type: "announce",
                    text: command.substr(10)
                });

                return;
            }

            if (this.#channel !== null) {
                bus.post({
                    channel: this.#channel,
                    type: "say",
                    text: command
                });
            } else {
                this.#print("*** Need to /join a channel first");
            }
        } catch (e) {
            this.#print(e);
        } finally {
            this.#showPrompt();
        }
    };

    #onBusDetached = () => {
        this.#quit();
    };

    #onBusMessage = (message: PortalServerMessage, data: Buffer | null) => {
        switch (message.type) {
            case "welcome": {
                this.#print("*** Welcome! Available channels:", message.channels);

                break;
            }
            case "membership": {
                this.#print("*** Joined", message.channel);

                const membersSummary = message.members.map(m => `${m.nick} (connected from ${m.address})`).join("\n\t");
                this.#print("- Members:\n\t" + membersSummary);

                for (const item of message.history)
                    this.#print(`<${item.sender}> ${item.text}`);

                break;
            }
            case "join": {
                const { user } = message;
                this.#print(`👋 ${user.nick} (${user.address}) joined ${message.channel}`);

                break;
            }
            case "part": {
                const { user } = message;
                this.#print(`🚪 ${user.nick} (${user.address}) left ${message.channel}`);

                break;
            }
            case "chat": {
                this.#print(`<${message.sender}> ${message.text}`);

                break;
            }
            case "announce": {
                this.#print(`📣 <${message.sender}> ${message.text}`);

                break;
            }
            default: {
                this.#print("Unhandled message:", message);

                break;
            }
        }
    };

    #showPrompt() {
        process.stdout.write("\r\x1B[K" + this.#prompt);
    }

    #print(...words: any[]) {
        const text = words.map(w => (typeof w === "string") ? w : inspect(w, { colors: true })).join(" ");
        process.stdout.write(`\r\x1B[K${text}\n${this.#prompt}`);
    }
}

const nick = process.argv[2];
const app = new Application(nick);
app.run()
    .catch(e => {
        console.error(e);
    });
