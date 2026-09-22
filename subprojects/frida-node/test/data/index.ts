export function targetProgram() {
    const platform = process.platform;
    if (platform === "win32") {
        return "C:\\Windows\\notepad.exe";
    } else if (platform === "darwin") {
        return new URL("./unixvictim-macos", import.meta.url).pathname;
    } else if (platform === "linux" && ["ia32", "x64"].includes(process.arch)) {
        const fridaArch = (process.arch === "x64") ? "x86_64" : "x86";
        return new URL("./unixvictim-linux-" + fridaArch, import.meta.url).pathname;
    } else {
        return "/bin/cat";
    }
}
