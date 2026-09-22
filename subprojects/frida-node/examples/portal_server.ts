import crypto from "crypto";
import frida from "frida";
import readline from "readline";

const ENABLE_CONTROL_INTERFACE = true;

class Application {
    #service: frida.PortalService;
    #device: frida.Device;
    #peers = new Map<frida.PortalConnectionId, Peer>();
    #nicks = new Set<string>();
    #channels = new Map<string, Channel>();

    constructor() {
        const clusterParams = new frida.EndpointParameters({
            address: "unix:/home/oleavr/src/cluster",
            certificate: "/home/oleavr/src/identity.pem",
            authentication: {
                scheme: "token",
                token: "wow-such-secret"
            },
        });

        let controlParams: frida.EndpointParameters | undefined;
        if (ENABLE_CONTROL_INTERFACE) {
            controlParams = new frida.EndpointParameters({
                address: "::1",
                port: 27042,
                authentication: {
                    scheme: "callback",
                    callback: this._authenticate
                },
                assetRoot: "/home/oleavr/src/frida/frida-python/examples/web_client/dist"
            });
        }

        const service = new frida.PortalService({ clusterParams, controlParams });
        this.#service = service;
        this.#device = service.device;

        service.nodeConnected.connect(this._onNodeConnected);
        service.nodeJoined.connect(this._onNodeJoined);
        service.nodeLeft.connect(this._onNodeLeft);
        service.nodeDisconnected.connect(this._onNodeDisconnected);

        service.controllerConnected.connect(this._onControllerConnected);
        service.controllerDisconnected.connect(this._onControllerDisconnected);

        service.authenticated.connect(this._onAuthenticated);
        service.subscribe.connect(this._onSubscribe);
        service.message.connect(this._onMessage);
    }

    async run() {
        await this.#service.start();
        console.log("Started!");

        await this.#device.enableSpawnGating();
        console.log("Enabled spawn gating");

        const rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout,
            terminal: true
        });
        rl.on("close", () => {
            this.#service.stop();
        });
        rl.on("line", async command => {
            try {
                if (command.length === 0) {
                    console.log("Processes:", await this.#device.enumerateProcesses());
                    return;
                }

                if (command === "stop") {
                    await this.#service.stop();
                }
            } catch (e) {
                console.error(e);
            } finally {
                this._showPrompt();
            }
        });
        this._showPrompt();
    }

    _showPrompt() {
        process.stdout.write("Enter command: ");
    }

    _authenticate = async (rawToken: string) => {
        let nick: string, secret: string;
        try {
            const token = JSON.parse(rawToken);
            ({ nick, secret } = token);
        } catch (e) {
            throw new Error("Invalid token");
        }
        if (typeof nick !== "string" || typeof secret !== "string")
            throw new Error("Invalid token");

        const provided = crypto.createHash("sha1").update(secret).digest();
        const expected = crypto.createHash("sha1").update("knock-knock").digest();
        if (!crypto.timingSafeEqual(provided, expected))
            throw new Error("Get outta here");

        return { nick };
    };

    _onNodeConnected = (connectionId: frida.PortalConnectionId, remoteAddress: frida.SocketAddress) => {
        console.log("onNodeConnected()", connectionId, remoteAddress);
    };

    _onNodeJoined = async (connectionId: frida.PortalConnectionId, application: frida.Application) => {
        console.log("onNodeJoined()", connectionId, application);
        console.log("\ttags:", this.#service.enumerateTags(connectionId));
    };

    _onNodeLeft = (connectionId: frida.PortalConnectionId, application: frida.Application) => {
        console.log("onNodeLeft()", connectionId, application);
    };

    _onNodeDisconnected = (connectionId: frida.PortalConnectionId, remoteAddress: frida.SocketAddress) => {
        console.log("onNodeDisconnected()", connectionId, remoteAddress);
    };

    _onControllerConnected = (connectionId: frida.PortalConnectionId, remoteAddress: frida.SocketAddress) => {
        console.log("onControllerConnected()", connectionId, remoteAddress);

        this.#peers.set(connectionId, new Peer(connectionId, remoteAddress));
    };

    _onControllerDisconnected = (connectionId: frida.PortalConnectionId, remoteAddress: frida.SocketAddress) => {
        console.log("onControllerDisconnected()", connectionId, remoteAddress);

        const peer = this.#peers.get(connectionId)!;
        this.#peers.delete(connectionId);

        for (const channel of peer.memberships)
            channel.removeMember(peer);

        if (peer.nick !== null)
            this._releaseNick(peer.nick);
    };

    _onAuthenticated = (connectionId: frida.PortalConnectionId, sessionInfo: frida.AuthenticatedSessionInfo) => {
        console.log("onAuthenticated()", connectionId, sessionInfo);

        const peer = this.#peers.get(connectionId);
        if (peer === undefined)
            return;

        peer.nick = this._acquireNick(sessionInfo.nick);
    };

    _onSubscribe = (connectionId: frida.PortalConnectionId) => {
        console.log("onSubscribe()", connectionId);

        const welcomeMessage: WelcomeMessage = {
            type: "welcome",
            channels: Array.from(this.#channels.keys())
        };
        this.#service.post(connectionId, welcomeMessage);
    };

    _onMessage = (connectionId: frida.PortalConnectionId, message: PortalClientRequest, data: Buffer | null) => {
        const peer = this.#peers.get(connectionId)!;

        switch (message.type) {
            case "join": {
                this._getChannel(message.channel).addMember(peer);

                break;
            }
            case "part": {
                const channel = this.#channels.get(message.channel);
                if (channel === undefined)
                    return;

                channel.removeMember(peer);

                break;
            }
            case "say": {
                const channel = this.#channels.get(message.channel);
                if (channel === undefined)
                    return;

                channel.post(message.text, peer);

                break;
            }
            case "announce": {
                this.#service.broadcast({
                    type: "announce",
                    sender: peer.nick!,
                    text: message.text
                } as AnnounceMessage);

                break;
            }
            default: {
                console.error("Unhandled message:", message);

                break;
            }
        }
    };

    _acquireNick(requested: string) {
        let candidate = requested;
        let serial = 2;
        while (this.#nicks.has(candidate)) {
            candidate = requested + serial;
            serial++;
        }

        const nick = candidate;
        this.#nicks.add(nick);

        return nick;
    }

    _releaseNick(nick: string) {
        this.#nicks.delete(nick);
    }

    _getChannel(name: string) {
        let channel = this.#channels.get(name);
        if (channel === undefined) {
            channel = new Channel(name, this.#service);
            this.#channels.set(name, channel);
        }
        return channel;
    }
}

class Peer {
    connectionId: frida.PortalConnectionId;
    remoteAddress: frida.SocketAddress;
    nick: string | null = null;
    memberships = new Set<Channel>();

    constructor(connectionId: frida.PortalConnectionId, remoteAddress: frida.SocketAddress) {
        this.connectionId = connectionId;
        this.remoteAddress = remoteAddress;
    }

    toJSON(): User {
        let address: string;
        switch (this.remoteAddress.family) {
            case "ipv4":
            case "ipv6":
                address = this.remoteAddress.address;
                break;
            case "unix:path":
                address = this.remoteAddress.path;
                break;
            case "unix:abstract":
                address = this.remoteAddress.path.toString("hex");
                break;
            case "unix:anonymous":
                address = "<anonymous>";
                break;
        }
        return {
            nick: this.nick!,
            address,
        };
    }
}

class Channel {
    name: string;
    members = new Set<Peer>();
    history: ChatMessage[] = [];

    #service: frida.PortalService;

    constructor(name: string, service: frida.PortalService) {
        this.name = name;

        this.#service = service;
    }

    addMember(peer: Peer) {
        if (peer.memberships.has(this))
            return;

        peer.memberships.add(this);
        this.members.add(peer);

        this.#service.narrowcast(this.name, {
            type: "join",
            channel: this.name,
            user: peer.toJSON()
        } as JoinMessage);
        this.#service.tag(peer.connectionId, this.name);

        this.#service.post(peer.connectionId, {
            type: "membership",
            channel: this.name,
            members: Array.from(this.members).map(m => m.toJSON()),
            history: this.history
        } as MembershipMessage);
    }

    removeMember(peer: Peer) {
        if (!peer.memberships.has(this))
            return;

        peer.memberships.delete(this);
        this.members.delete(peer);

        this.#service.untag(peer.connectionId, this.name);
        this.#service.narrowcast(this.name, {
            type: "part",
            channel: this.name,
            user: peer.toJSON()
        } as PartMessage);
    }

    post(text: string, peer: Peer) {
        if (!peer.memberships.has(this))
            return;

        const message: ChatMessage = {
            type: "chat",
            sender: peer.nick!,
            text: text
        };

        this.#service.narrowcast(this.name, message);

        const { history } = this;
        history.push(message);
        if (history.length === 20)
            history.shift();
    }
}

const app = new Application();
app.run()
    .catch(e => {
        console.error(e);
    });
