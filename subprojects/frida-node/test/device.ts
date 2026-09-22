import { expect } from "chai";
import frida from "frida";
import "mocha";

declare function gc(): void;

describe("Device", function () {
    afterEach(gc);

    it("should have some metadata", async () => {
        const device = await frida.getLocalDevice();
        expect(device.name).to.equal("Local System");
        expect(device.type).to.equal("local");
    });

    it("should enumerate processes", async () => {
        const device = await frida.getLocalDevice();

        const processes = await device.enumerateProcesses();
        expect(processes.length).to.be.above(0);

        const process = processes[0];
        expect(process.pid).to.be.at.least(0);
        expect(process.name.length).to.be.greaterThan(0);
    });
});
