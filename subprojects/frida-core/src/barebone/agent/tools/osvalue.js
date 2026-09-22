const osSerialize_withCapacity = new NativeFunction(ptr('0xfffffff007fbf074'), 'pointer', ['uint']);

const UNPAC_MASK = ptr('0xfffffff000000000');

const serializer = osSerialize_withCapacity(4096);

const gIOInterruptControllers = ptr('0xfffffff009a3f090').readPointer();

const serialize = new NativeFunction(implFor(6, vtableOf(gIOInterruptControllers)), 'uint' /* 'bool' */, ['pointer', 'pointer']);
console.log('serialize() is at:', serialize.handle);
const result = serialize(gIOInterruptControllers, serializer);
console.log('result returned:', result);

const getText = new NativeFunction(implFor(15, vtableOf(serializer)), 'pointer', ['pointer']);
const text = getText(serializer);
console.log('Got text:', text.readUtf8String(text));

const release = new NativeFunction(implFor(5, vtableOf(serializer)), 'void', ['pointer']);
release(serializer);

Object.assign(globalThis, { osSerialize_withCapacity });

function implFor(n, vtable) {
  return unpac(vtable.add(n * Process.pointerSize).readPointer());
}

function vtableOf(instance) {
  return unpac(instance.readPointer());
}

function unpac(p) {
  return p.or(UNPAC_MASK);
}
