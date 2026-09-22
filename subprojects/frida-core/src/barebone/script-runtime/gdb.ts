import { BNativePointer, BInt64, BUInt64 } from "./primitives.js";

export const gdb = $gdb;

export interface GDBClient {
    state: GDBState;
    exception: GDBException | null;
    continue(): void;
    stop(): void;
    restart(): void;
    readPointer(address: bigint): BNativePointer;
    writePointer(address: bigint, value: BNativePointer): void;
    readS8(address: bigint): number;
    writeS8(address: bigint, value: number | BInt64): void;
    readU8(address: bigint): number;
    writeU8(address: bigint, value: number | BUInt64): void;
    readS16(address: bigint): number;
    writeS16(address: bigint, value: number | BInt64): void;
    readU16(address: bigint): number;
    writeU16(address: bigint, value: number | BUInt64): void;
    readS32(address: bigint): number;
    writeS32(address: bigint, value: number | BInt64): void;
    readU32(address: bigint): number;
    writeU32(address: bigint, value: number | BUInt64): void;
    readS64(address: bigint): BInt64;
    writeS64(address: bigint, value: number | BInt64): void;
    readU64(address: bigint): BUInt64;
    writeU64(address: bigint, value: number | BUInt64): void;
    readFloat(address: bigint): number;
    writeFloat(address: bigint, value: number): void;
    readDouble(address: bigint): number;
    writeDouble(address: bigint, value: number): void;
    readByteArray(address: bigint, size: number): ArrayBuffer;
    writeByteArray(address: bigint, value: ArrayBuffer | number[]): void;
    readCString(address: bigint, size?: number): string;
    readUtf8String(address: bigint, size?: number): string;
    writeUtf8String(address: bigint, value: string): void;
    addBreakpoint(kind: GDBBreakpointKind, address: bigint, size: number): GDBBreakpoint;
    execute(command: string): void;
    query(request: string): string;
}

type GDBState =
    | "stopped"
    | "running"
    | "stopping"
    | "closed"
    ;

export interface GDBException {
    signum: number;
    breakpoint: GDBBreakpoint | null;
    thread: GDBThread;
}

export interface GDBThread {
    id: string;
    name: string | null;
    step(): void;
    stepAndContinue(): void;
    readRegisters(): { [name: string]: BNativePointer };
    readRegister(name: string): BNativePointer;
    writeRegister(name: string, val: BNativePointer): void;
}

export interface GDBBreakpoint {
    kind: GDBBreakpointKind;
    address: BNativePointer;
    size: number;
    enable(): void;
    disable(): void;
    remove(): void;
}

export type GDBBreakpointKind =
    | "soft"
    | "hard"
    | "write"
    | "read"
    | "access"
    ;

declare const $gdb: GDBClient;
