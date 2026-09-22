let sharedDeviceManager: DeviceManager | null = null;

export async function querySystemParameters(cancellable?: Cancellable | null): Promise<SystemParameters> {
    const device = await getLocalDevice(cancellable);
    return await device.querySystemParameters(cancellable);
}

export async function spawn(program: string | string[], options?: SpawnOptions, cancellable?: Cancellable | null): Promise<number> {
    const device = await getLocalDevice(cancellable);
    return await device.spawn(program, options, cancellable);
}

export async function resume(target: TargetProcess, cancellable?: Cancellable | null): Promise<void> {
    const device = await getLocalDevice(cancellable);
    await device.resume(target, cancellable);
}

export async function kill(target: TargetProcess, cancellable?: Cancellable | null): Promise<void> {
    const device = await getLocalDevice(cancellable);
    await device.kill(target, cancellable);
}

export async function attach(target: TargetProcess, options?: SessionOptions, cancellable?: Cancellable | null): Promise<Session> {
    const device = await getLocalDevice(cancellable);
    return await device.attach(target, options, cancellable);
}

export async function injectLibraryFile(target: TargetProcess, path: string, entrypoint: string, data: string,
        cancellable?: Cancellable | null): Promise<number> {
    const device = await getLocalDevice(cancellable);
    return await device.injectLibraryFile(target, path, entrypoint, data, cancellable);
}

export async function injectLibraryBlob(target: TargetProcess, blob: Buffer, entrypoint: string, data: string,
        cancellable?: Cancellable | null): Promise<number> {
    const device = await getLocalDevice(cancellable);
    return await device.injectLibraryBlob(target, blob, entrypoint, data, cancellable);
}

export async function enumerateDevices(cancellable?: Cancellable | null): Promise<Device[]> {
    const deviceManager = getDeviceManager();
    return await deviceManager.enumerateDevices(cancellable);
};

export function getDeviceManager(): DeviceManager {
    if (sharedDeviceManager === null) {
        sharedDeviceManager = new DeviceManager();
    }
    return sharedDeviceManager;
}

export function getLocalDevice(cancellable?: Cancellable | null): Promise<Device> {
    return getMatchingDevice(device => device.type === DeviceType.Local, {}, cancellable);
}

export function getRemoteDevice(cancellable?: Cancellable | null): Promise<Device> {
    return getMatchingDevice(device => device.type === DeviceType.Remote, {}, cancellable);
}

export function getUsbDevice(options?: GetDeviceOptions, cancellable?: Cancellable | null): Promise<Device> {
    return getMatchingDevice(device => device.type === DeviceType.Usb, options, cancellable);
}

export function getDevice(id: string, options?: GetDeviceOptions, cancellable?: Cancellable | null): Promise<Device> {
    return getMatchingDevice(device => device.id === id, options, cancellable);
}

export interface GetDeviceOptions {
    timeout?: number | null;
}

async function getMatchingDevice(predicate: DevicePredicate, options: GetDeviceOptions = {}, cancellable: Cancellable | null = null): Promise<Device> {
    const device = await findMatchingDevice(predicate, cancellable);
    if (device !== null) {
        return device;
    }

    const { timeout = 0 } = options;
    if (timeout === 0) {
        throw new Error("Device not found");
    }

    const getDeviceEventually = new Promise((resolve: (device: Device) => void, reject: (error: Error) => void) => {
        const deviceManager = getDeviceManager();

        deviceManager.added.connect(onDeviceAdded);

        const timer = (timeout !== null) ? setTimeout(onTimeout, timeout) : null;

        if (cancellable !== null) {
            cancellable.cancelled.connect(onCancel);
            if (cancellable.isCancelled) {
                onCancel();
                return;
            }
        }

        findMatchingDevice(predicate, cancellable)
            .then(device => {
                if (device !== null) {
                    onSuccess(device);
                }
            })
            .catch(onError);

        function onDeviceAdded(device: Device): void {
            if (predicate(device)) {
                onSuccess(device);
            }
        }

        function onSuccess(device: Device): void {
            stopMonitoring();
            resolve(device);
        }

        function onError(error: Error): void {
            stopMonitoring();
            reject(error);
        }

        function onTimeout(): void {
            onError(new Error("Timed out while waiting for device to appear"));
        }

        function onCancel(): void {
            onError(new Error("Operation was cancelled"));
        }

        function stopMonitoring(): void {
            cancellable?.cancelled.disconnect(onCancel);

            if (timer !== null) {
                clearTimeout(timer);
            }

            deviceManager.added.disconnect(onDeviceAdded);
        }
    });

    return await getDeviceEventually;
}

async function findMatchingDevice(predicate: DevicePredicate, cancellable?: Cancellable | null): Promise<Device | null> {
    const deviceManager = getDeviceManager();

    const devices = await deviceManager.enumerateDevices(cancellable);

    return devices.find(predicate) ?? null;
}

type DevicePredicate = (device: Device) => boolean;
