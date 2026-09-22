import { Buffer } from "buffer";
import RemoteStreamController, { IncomingStream, Packet } from "frida-remote-stream";
import fs from "fs";
import fsPath from "path";

const {
    S_IFMT,
    S_IFREG,
    S_IFDIR,
    S_IFCHR,
    S_IFBLK,
    S_IFIFO,
    S_IFLNK,
    S_IFSOCK,
} = fs.constants;
const ERANGE = 34;
const { pointerSize } = Process;

const cachedUsers = new Map<number, string>();
const cachedGroups = new Map<number, string>();
let getpwduidR: SystemFunction<number, [number, NativePointerValue, NativePointerValue, number, NativePointerValue]> | null = null;
let getgrgidR: SystemFunction<number, [number, NativePointerValue, NativePointerValue, number, NativePointerValue]> | null = null;

class Agent {
    #streamController = new RemoteStreamController();

    constructor() {
        recv(this.#onMessage);
        this.#streamController.events.on("send", this.#onStreamControllerSendRequest);
        this.#streamController.events.on("stream", this.#onStreamControllerStreamRequest);
    }

    ls(paths: string[]): FileGroup[] {
        if (paths.length === 0) {
            paths = [(Process.platform === "windows") ? "C:\\" : "/"];
        }

        const fileGroup: FileGroup = {
            path: "",
            entries: [],
            errors: [],
        };
        const directoryGroups: FileGroup[] = [];

        for (const path of paths) {
            let stats: fs.Stats;
            try {
                stats = fs.lstatSync(path);
            } catch (e) {
                fileGroup.errors.push([path, (e as Error).message]);
                continue;
            }

            let digDeeper;
            if (stats.isSymbolicLink()) {
                let s: fs.Stats;
                try {
                    s = fs.statSync(path);
                    digDeeper = s.isDirectory();
                    if (digDeeper) {
                        stats = s;
                    }
                } catch (e) {
                    digDeeper = false;
                }
            } else {
                digDeeper = stats.isDirectory();
            }

            if (digDeeper) {
                let names: string[];
                try {
                    names = fs.readdirSync(path)
                } catch (e) {
                    directoryGroups.push({
                        path,
                        entries: [],
                        errors: [[path, (e as Error).message]]
                    });
                    continue;
                }

                const entries: FileEntry[] = [];
                for (const name of names) {
                    const curPath = fsPath.join(path!, name);
                    try {
                        const curStats = (name === ".") ? stats : fs.lstatSync(curPath);
                        const entry = entryFromStats(curPath, name, curStats);
                        entries.push(entry);
                    } catch (e) {
                    }
                }

                directoryGroups.push({
                    path,
                    entries,
                    errors: []
                });
            } else {
                fileGroup.entries.push(entryFromStats(path, path, stats));
            }
        }

        return (fileGroup.entries.length > 0 || fileGroup.errors.length > 0)
            ? [fileGroup, ...directoryGroups]
            : directoryGroups;
    }

    rm(paths: string[], flags: string[]): string[] {
        const errors: string[] = [];

        const dirs: string[] = [];
        const files: string[] = [];

        const force = flags.includes("force");
        const recursive = flags.includes("recursive");

        if (recursive) {
            const pending = paths.slice();
            while (true) {
                const path = pending.shift();
                if (path === undefined) {
                    break;
                }

                let s: fs.Stats;
                try {
                    s = fs.statSync(path);
                } catch (e) {
                    files.push(path);
                    continue;
                }

                if (s.isDirectory()) {
                    pending.push(...fs.readdirSync(path)
                        .filter(filename => filename !== "." && filename !== "..")
                        .map(filename => fsPath.join(path, filename)));
                    dirs.unshift(path);
                } else {
                    files.unshift(path);
                }
            }
        } else {
            files.push(...paths);
        }

        for (const path of files) {
            try {
                fs.unlinkSync(path);
            } catch (e) {
                if (!force) {
                    collectError(path, e as Error);
                }
            }
        }

        for (const path of dirs) {
            try {
                fs.rmdirSync(path);
            } catch (e) {
                collectError(path, e as Error);
            }
        }

        function collectError(path: string, e: Error): void {
            errors.push(`${path}: ${(e as Error).message}`);
        }

        return errors;
    }

    async pull(paths: string[]): Promise<void> {
        let total = 0;
        for (const path of paths) {
            try {
                const s = fs.statSync(path);
                total += s.size;
            } catch (e) {
            }
        }
        send({
            type: "pull:status",
            total
        });

        let index = 0;
        for (const path of paths) {
            const reader = fs.createReadStream(path);
            const writer = reader.pipe(this.#streamController.open(index.toString()));

            const transfer = new Promise((resolve, reject) => {
                reader.addListener("error", onReaderError);
                writer.addListener("error", onWriterError);
                writer.addListener("finish", onWriterFinish);

                function onReaderError(error: Error): void {
                    detachListeners();
                    writer.end();
                    reject(error);
                }

                function onWriterError(error: Error): void {
                    detachListeners();
                    reader.destroy();
                    resolve(null);
                }

                function onWriterFinish(): void {
                    detachListeners();
                    resolve(null);
                }

                function detachListeners(): void {
                    writer.removeListener("finish", onWriterFinish);
                    writer.removeListener("error", onWriterError);
                    reader.removeListener("error", onReaderError);
                }
            });

            try {
                await transfer;
            } catch (e) {
                send({
                    type: "pull:io-error",
                    index,
                    error: (e as Error).message
                });
            }

            index++;
        }
    }

    #onMessage = (message: any, rawData: ArrayBuffer | null): void => {
        const type: string = message.type;

        if (type === "stream") {
            const data: Buffer | null = (rawData !== null) ? Buffer.from(rawData) : null;
            this.#streamController.receive({
                stanza: message.payload,
                data
            });
        }

        recv(this.#onMessage);
    };

    #onStreamControllerSendRequest = (packet: Packet): void => {
        send({
            type: "stream",
            payload: packet.stanza
        }, packet.data?.buffer as ArrayBuffer);
    };

    #onStreamControllerStreamRequest = (stream: IncomingStream): void => {
        const index = parseInt(stream.label);

        const details = stream.details;
        const filename: string = details.filename;
        const target: string = details.target;

        let path: string | null = null;
        try {
            const s = fs.statSync(target);
            if (s.isDirectory()) {
                path = fsPath.join(target, filename);
            }
        } catch (e) {
        }
        if (path === null) {
            path = target;
        }

        const writer = fs.createWriteStream(path);
        stream.pipe(writer);

        stream.addListener("error", onStreamError);
        writer.addListener("error", onWriterError);
        writer.addListener("finish", onWriterFinish);

        function onStreamError(error: Error): void {
            detachListeners();
            writer.end();

            send({
                type: "push:io-error",
                index,
                error: error.message
            });
        }

        function onWriterError(error: Error): void {
            detachListeners();
            stream.destroy();

            send({
                type: "push:io-error",
                index,
                error: error.message
            });
        }

        function onWriterFinish(): void {
            detachListeners();

            send({
                type: "push:io-success",
                index
            });
        }

        function detachListeners(): void {
            writer.removeListener("finish", onWriterFinish);
            writer.removeListener("error", onWriterError);
            stream.removeListener("error", onStreamError);
        }
    };
}

interface FileGroup {
    path: string;
    entries: FileEntry[];
    errors: FileError[];
}

type FileEntry = [
    name: string,
    target: FileTarget | null,
    type: FileType,
    permissions: string,
    nlink: number,
    owner: string,
    group: string,
    size: number,
    mtime: number,
];
type FileTarget = [path: string, type: [FileType, string] | null];
type FileType = "-" | "d" | "c" | "b" | "p" | "l" | "s";
type FileError = [path: string, message: string];

function entryFromStats(path: string, name: string, stats: fs.Stats): FileEntry {
    const { mode } = stats;
    const type = typeFromMode(mode);

    let target: FileTarget | null;
    if (type === "l") {
        const targetPath = fs.readlinkSync(path);
        let targetType: FileType | null;
        let targetPerms: string;
        try {
            const s = fs.statSync(path);
            targetPerms = permissionsFromMode(s.mode);
            targetType = typeFromMode(s.mode);
        } catch (e) {
            targetType = null;
        }
        target = [targetPath, (targetType !== null) ? [targetType, targetPerms!] : null];
    } else {
        target = null;
    }

    return [
        name,
        target,
        type,
        permissionsFromMode(mode),
        stats.nlink,
        resolveUserID(stats.uid),
        resolveGroupID(stats.gid),
        stats.size,
        stats.mtimeMs,
    ];
}

function typeFromMode(mode: number): FileType {
    switch (mode & S_IFMT) {
        case S_IFREG: return "-";
        case S_IFDIR: return "d";
        case S_IFCHR: return "c";
        case S_IFBLK: return "b";
        case S_IFIFO: return "p";
        case S_IFLNK: return "l";
        case S_IFSOCK: return "s";
    }
    throw new Error(`Invalid mode: 0x${mode.toString(16)}`);
}

function permissionsFromMode(mode: number): string {
    let access = "";
    for (let i = 8; i !== -1; i -= 3) {
        if (((mode >>> i) & 1) !== 0) {
            access += "r"
        } else {
            access += "-";
        }
        if (((mode >>> (i - 1)) & 1) !== 0) {
            access += "w"
        } else {
            access += "-";
        }
        if (((mode >>> (i - 2)) & 1) !== 0) {
            access += "x"
        } else {
            access += "-";
        }
    }
    return access;
}

function resolveUserID(uid: number): string {
    let name = cachedUsers.get(uid);
    if (name !== undefined) {
        return name;
    }

    if (Process.platform === "windows") {
        name = uid.toString();
    } else {
        if (getpwduidR === null) {
            getpwduidR = new SystemFunction(Module.getGlobalExportByName("getpwuid_r"),
                "int",
                ["uint", "pointer", "pointer", "size_t", "pointer"]);
        }

        let pwd: NativePointer;
        let pwdCapacity = 128;
        let buf: NativePointer;
        let bufCapacity = 1024;
        let res: NativePointer;
        do {
            pwd = Memory.alloc(pwdCapacity + bufCapacity + pointerSize);
            buf = pwd.add(pwdCapacity);
            res = buf.add(bufCapacity);

            const r = getpwduidR(uid, pwd, buf, bufCapacity, res) as UnixSystemFunctionResult<number>;
            if (r.value === 0) {
                break;
            }
            if (r.errno !== ERANGE) {
                throw new Error(`Unable to resolve user ID ${uid}: ${r.errno}`);
            }
            bufCapacity *= 2;
        } while (true);

        const entry = res.readPointer();
        if (!entry.isNull()) {
            name = entry.readPointer().readUtf8String()!;
        } else {
            name = uid.toString();
        }
    }

    cachedUsers.set(uid, name);

    return name;
}

function resolveGroupID(gid: number): string {
    let name = cachedGroups.get(gid);
    if (name !== undefined) {
        return name;
    }

    if (Process.platform === "windows") {
        name = gid.toString();
    } else {
        if (getgrgidR === null) {
            getgrgidR = new SystemFunction(Module.getGlobalExportByName("getgrgid_r"),
                "int",
                ["uint", "pointer", "pointer", "size_t", "pointer"]);
        }

        let group: NativePointer;
        let groupCapacity = 128;
        let buf: NativePointer;
        let bufCapacity = 1024;
        let res: NativePointer;
        do {
            group = Memory.alloc(groupCapacity + bufCapacity + pointerSize);
            buf = group.add(groupCapacity);
            res = buf.add(bufCapacity);

            const r = getgrgidR(gid, group, buf, bufCapacity, res) as UnixSystemFunctionResult<number>;
            if (r.value === 0) {
                break;
            }
            if (r.errno !== ERANGE) {
                throw new Error(`Unable to resolve group ID ${gid}: ${r.errno}`);
            }
            bufCapacity *= 2;
        } while (true);

        const entry = res.readPointer();
        if (!entry.isNull()) {
            name = entry.readPointer().readUtf8String()!;
        } else {
            name = gid.toString();
        }
    }

    cachedGroups.set(gid, name);

    return name;
}

const agent = new Agent();

rpc.exports = {
    ls: agent.ls.bind(agent),
    rm: agent.rm.bind(agent),
    pull: agent.pull.bind(agent),
};
