import { gdb } from "./gdb.js";

const u32Max = 0xffffffffn;
const u64Max = 0xffffffffffffffffn;

const ptrSize = BigInt(Process.pointerSize);
const ptrMax = (ptrSize === 8n) ? u64Max : u32Max;
const signBitMask = 1n << ((ptrSize * 8n) - 1n);

const longIsSixtyFourBitsWide = ptrSize === 8n;

export class BNativePointer {
    $v: bigint;

    constructor(v: string | number | bigint | BUInt64 | BInt64 | BNativePointerValue) {
        if (typeof v === "object") {
            if ("$v" in v) {
                this.$v = v.$v;
            } else {
                this.$v = v.handle.$v;
            }
        } else {
            let val = BigInt(v);
            if (val < 0n) {
                val = ptrMax - (-val - 1n);
            }
            this.$v = val;
        }
    }

    add(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return new BNativePointer(this.$v + parseBigInt(rhs));
    }

    sub(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return new BNativePointer(this.$v - parseBigInt(rhs));
    }

    and(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return new BNativePointer(this.$v & parseBigInt(rhs));
    }

    or(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return new BNativePointer(this.$v | parseBigInt(rhs));
    }

    xor(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return new BNativePointer(this.$v ^ parseBigInt(rhs));
    }

    shr(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return new BNativePointer(this.$v >> parseBigInt(rhs));
    }

    shl(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return new BNativePointer(this.$v << parseBigInt(rhs));
    }

    not() {
        return new BNativePointer(~this.$v);
    }

    sign() {
        return this;
    }

    strip() {
        return this;
    }

    blend() {
        return this;
    }

    compare(rawRhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        const lhs = this.$v;
        const rhs = parseBigInt(rawRhs);
        return (lhs === rhs) ? 0 : ((lhs < rhs) ? -1 : 1);
    }

    equals(rhs: BNativePointerValue | number | bigint | BUInt64 | BInt64 | string) {
        return this.compare(rhs) === 0;
    }

    toInt32() {
        const val = this.$v;
        return Number(((val & signBitMask) !== 0n)
            ? -(ptrMax - val + 1n)
            : val);
    }

    toUInt32() {
        return Number(this.$v);
    }

    toString(radix?: number) {
        if (radix === undefined)
            return "0x" + this.$v.toString(16);
        return this.$v.toString(radix);
    }

    toJSON() {
        return "0x" + this.$v.toString(16);
    }

    toMatchPattern(): string {
        throwNotImplemented();
    }

    readPointer(): BNativePointer {
        return gdb.readPointer(this.$v);
    }

    writePointer(value: BNativePointer): BNativePointer {
        gdb.writePointer(this.$v, value);
        return this;
    }

    readS8(): number {
        return gdb.readS8(this.$v);
    }

    writeS8(value: number | BInt64): BNativePointer {
        gdb.writeS8(this.$v, value);
        return this;
    }

    readU8(): number {
        return gdb.readU8(this.$v);
    }

    writeU8(value: number | BUInt64): BNativePointer {
        gdb.writeU8(this.$v, value);
        return this;
    }

    readS16(): number {
        return gdb.readS16(this.$v);
    }

    writeS16(value: number | BInt64): BNativePointer {
        gdb.writeS16(this.$v, value);
        return this;
    }

    readU16(): number {
        return gdb.readU16(this.$v);
    }

    writeU16(value: number | BUInt64): BNativePointer {
        gdb.writeU16(this.$v, value);
        return this;
    }

    readS32(): number {
        return gdb.readS32(this.$v);
    }

    writeS32(value: number | BInt64): BNativePointer {
        gdb.writeS32(this.$v, value);
        return this;
    }

    readU32(): number {
        return gdb.readU32(this.$v);
    }

    writeU32(value: number | BUInt64): BNativePointer {
        gdb.writeU32(this.$v, value);
        return this;
    }

    readS64(): BUInt64 {
        return gdb.readS64(this.$v);
    }

    writeS64(value: number | BInt64): BNativePointer {
        gdb.writeS64(this.$v, value);
        return this;
    }

    readU64(): BUInt64 {
        return gdb.readU64(this.$v);
    }

    writeU64(value: number | BUInt64): BNativePointer {
        gdb.writeU64(this.$v, value);
        return this;
    }

    readShort(): number {
        return this.readS16();
    }

    writeShort(value: number | BInt64): BNativePointer {
        return this.writeS16(value);
    }

    readUShort(): number {
        return this.readU16();
    }

    writeUShort(value: number | BUInt64): BNativePointer {
        return this.writeU16(value);
    }

    readInt(): number {
        return this.readS32();
    }

    writeInt(value: number | BInt64): BNativePointer {
        return this.writeS32(value);
    }

    readUInt(): number {
        return this.readU32();
    }

    writeUInt(value: number | BUInt64): BNativePointer {
        return this.writeU32(value);
    }

    readLong(): number | BInt64 {
        return longIsSixtyFourBitsWide ? this.readS64() : this.readS32();
    }

    writeLong(value: number | BInt64): BNativePointer {
        return longIsSixtyFourBitsWide ? this.writeS64(value) : this.writeS32(value);
    }

    readULong(): number | BUInt64 {
        return longIsSixtyFourBitsWide ? this.readU64() : this.readU32();
    }

    writeULong(value: number | BUInt64): BNativePointer {
        return longIsSixtyFourBitsWide ? this.writeU64(value) : this.writeU32(value);
    }

    readFloat(): number {
        return gdb.readFloat(this.$v);
    }

    writeFloat(value: number): BNativePointer {
        gdb.writeFloat(this.$v, value);
        return this;
    }

    readDouble(): number {
        return gdb.readDouble(this.$v);
    }

    writeDouble(value: number): BNativePointer {
        gdb.writeDouble(this.$v, value);
        return this;
    }

    readByteArray(length: number): ArrayBuffer {
        return gdb.readByteArray(this.$v, length);
    }

    writeByteArray(value: ArrayBuffer | number[]): BNativePointer {
        gdb.writeByteArray(this.$v, value);
        return this;
    }

    readCString(size?: number): string {
        return gdb.readCString(this.$v, size);
    }

    readUtf8String(size?: number): string {
        return gdb.readUtf8String(this.$v, size);
    }

    writeUtf8String(value: string): BNativePointer {
        gdb.writeUtf8String(this.$v, value);
        return this;
    }

    readUtf16String(length?: number): string {
        throwNotImplemented();
    }

    writeUtf16String(value: string): BNativePointer {
        throwNotImplemented();
    }

    readAnsiString(size?: number): string {
        throwNotImplemented();
    }

    writeAnsiString(value: string): BNativePointer {
        throwNotImplemented();
    }
}

export type BNativePointerValue = BNativePointer | BObjectWrapper;

export interface BObjectWrapper {
    handle: BNativePointer;
}

export class BInt64 {
    $v: bigint;

    constructor(v: string | number | bigint | BInt64) {
        if (typeof v === "object") {
            this.$v = v.$v;
        } else {
            this.$v = BigInt(v);
        }
    }

    add(rhs: BInt64 | number | bigint | string) {
        return new BInt64(this.$v + parseBigInt(rhs));
    }

    sub(rhs: BInt64 | number | bigint | string) {
        return new BInt64(this.$v - parseBigInt(rhs));
    }

    and(rhs: BInt64 | number | bigint | string) {
        return new BInt64(this.$v & parseBigInt(rhs));
    }

    or(rhs: BInt64 | number | bigint | string) {
        return new BInt64(this.$v | parseBigInt(rhs));
    }

    xor(rhs: BInt64 | number | bigint | string) {
        return new BInt64(this.$v ^ parseBigInt(rhs));
    }

    shr(rhs: BInt64 | number | bigint | string) {
        return new BInt64(this.$v >> parseBigInt(rhs));
    }

    shl(rhs: BInt64 | number | bigint | string) {
        return new BInt64(this.$v << parseBigInt(rhs));
    }

    not() {
        return new BInt64(~this.$v);
    }

    compare(rawRhs: BInt64 | number | bigint | string) {
        const lhs = this.$v;
        const rhs = parseBigInt(rawRhs);
        return (lhs === rhs) ? 0 : ((lhs < rhs) ? -1 : 1);
    }

    equals(rhs: BInt64 | number | bigint | string) {
        return this.compare(rhs) === 0;
    }

    toNumber() {
        return Number(this.$v);
    }

    toString(radix?: number) {
        return this.$v.toString(radix);
    }

    toJSON() {
        return this.$v.toString();
    }

    valueOf() {
        return Number(this.$v);
    }
}

export class BUInt64 {
    $v: bigint;

    constructor(v: string | number | bigint | BUInt64) {
        if (typeof v === "object") {
            this.$v = v.$v;
        } else {
            let val = BigInt(v);
            if (val < 0n) {
                val = u64Max - (-val - 1n);
            }
            this.$v = val;
        }
    }

    add(rhs: BUInt64 | number | bigint | string) {
        return new BUInt64(this.$v + parseBigInt(rhs));
    }

    sub(rhs: BUInt64 | number | bigint | string) {
        return new BUInt64(this.$v - parseBigInt(rhs));
    }

    and(rhs: BUInt64 | number | bigint | string) {
        return new BUInt64(this.$v & parseBigInt(rhs));
    }

    or(rhs: BUInt64 | number | bigint | string) {
        return new BUInt64(this.$v | parseBigInt(rhs));
    }

    xor(rhs: BUInt64 | number | bigint | string) {
        return new BUInt64(this.$v ^ parseBigInt(rhs));
    }

    shr(rhs: BUInt64 | number | bigint | string) {
        return new BUInt64(this.$v >> parseBigInt(rhs));
    }

    shl(rhs: BUInt64 | number | bigint | string) {
        return new BUInt64(this.$v << parseBigInt(rhs));
    }

    not() {
        return new BUInt64(~this.$v);
    }

    compare(rawRhs: BUInt64 | number | bigint | string) {
        const lhs = this.$v;
        const rhs = parseBigInt(rawRhs);
        return (lhs === rhs) ? 0 : ((lhs < rhs) ? -1 : 1);
    }

    equals(rhs: BUInt64 | number | bigint | string) {
        return this.compare(rhs) === 0;
    }

    toNumber() {
        return Number(this.$v);
    }

    toString(radix?: number) {
        return this.$v.toString(radix);
    }

    toJSON() {
        return this.$v.toString();
    }

    valueOf() {
        return Number(this.$v);
    }
}

function parseBigInt(v: string | number | bigint | BUInt64 | BInt64 | BNativePointerValue): bigint {
    if (typeof v === "object") {
        if ("$v" in v) {
            return v.$v;
        } else {
            return v.handle.$v;
        }
    } else {
        return BigInt(v);
    }
}

function throwNotImplemented(): never {
    throw new Error("Not yet implemented by the barebone backend");
}
