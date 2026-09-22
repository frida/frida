const SBS = bind({
  module: '/System/Library/PrivateFrameworks/SpringBoardServices.framework/SpringBoardServices',
  cprefix: 'SBS',
  functions: {
    copyFrontmostApplicationDisplayIdentifier:        ['pointer', [                    ]],
    copyLocalizedApplicationNameForDisplayIdentifier: ['pointer', ['pointer'           ]],
    copyIconImagePNGDataForDisplayIdentifier:         ['pointer', ['pointer'           ]],
    processIDForDisplayIdentifier:                    ['bool',    ['pointer', 'pointer']],
  }
});

const CF = bind({
  module: '/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation',
  cprefix: 'CF',
  functions: {
    retain:                           ['pointer', ['pointer'                           ]],
    release:                          ['void',    ['pointer'                           ]],
    stringCreateWithCString:          ['pointer', ['pointer', 'pointer', 'uint'        ]],
    stringGetCStringPtr:              ['pointer', ['pointer', 'uint'                   ]],
    stringGetLength:                  ['long',    ['pointer'                           ]],
    stringGetMaximumSizeForEncoding:  ['long',    ['long', 'uint'                      ]],
    stringGetCString:                 ['bool',    ['pointer', 'pointer', 'long', 'uint']],
    dataGetLength:                    ['long',    ['pointer'                           ]],
    dataGetBytePtr:                   ['pointer', ['pointer'                           ]],
  }
});

const kCFBooleanTrue = Module.getGlobalExportByName('kCFBooleanTrue').readPointer();
const kCFStringEncodingUTF8 = 0x08000100;

rpc.exports = {
  getFrontmostApplication(scope) {
    const idObj = SBS.copyFrontmostApplicationDisplayIdentifier();
    if (idObj.isNull())
      return null;

    const id = cfStringToUtf8(idObj);

    const nameObj = SBS.copyLocalizedApplicationNameForDisplayIdentifier(idObj);
    const name = cfStringToUtf8(nameObj);

    const pidBuf = Memory.alloc(4);
    SBS.processIDForDisplayIdentifier(idObj, pidBuf);
    const pid = pidBuf.readU32();

    CF.release(nameObj);
    CF.release(idObj);

    if (pid === 0)
      return null;

    return [
      id,
      name,
      pid,
      (scope === 'full') ? fetchApplicationIcon(id) : null
    ];
  },
  fetchApplicationIcons(appIds) {
    return appIds.map(fetchApplicationIcon);
  },
};

function fetchApplicationIcon(appId) {
  const idObj = CF.stringCreateWithCString(NULL, Memory.allocUtf8String(appId), kCFStringEncodingUTF8);
  const pngObj = SBS.copyIconImagePNGDataForDisplayIdentifier(idObj);
  CF.release(idObj);

  const png = CF.dataGetBytePtr(pngObj).readByteArray(CF.dataGetLength(pngObj));
  CF.release(pngObj);
  return base64FromBytes(png);
}

Interceptor.attach(Module.getGlobalExportByName('SecTaskCopyValueForEntitlement'), {
  onEnter(args) {
    this.entitlement = cfStringToUtf8(args[1]);
  },
  onLeave(retval) {
    if (this.entitlement === 'com.apple.springboard.iconState' && retval.isNull())
      retval.replace(CF.retain(kCFBooleanTrue));
  }
});

function bind({ module, cprefix, functions }) {
  const mod = Process.getModuleByName(module);
  const nfOpts = { exceptions: 'propagate' };
  return Object.fromEntries(Object.entries(functions).map(([name, [retType, argTypes]]) => [
    name,
    new NativeFunction(mod.getExportByName(cprefix + name.charAt(0).toUpperCase() + name.slice(1)), retType, argTypes, nfOpts)
  ]));
}

function cfStringToUtf8(cfstr) {
  const direct = CF.stringGetCStringPtr(cfstr, kCFStringEncodingUTF8);
  if (!direct.isNull())
    return direct.readUtf8String();

  const len16 = CF.stringGetLength(cfstr);
  const maxBytes = CF.stringGetMaximumSizeForEncoding(len16, kCFStringEncodingUTF8).valueOf() + 1;
  const buf = Memory.alloc(maxBytes);
  CF.stringGetCString(cfstr, buf, maxBytes, kCFStringEncodingUTF8);
  return buf.readUtf8String();
}

function base64FromBytes(buf) {
  const bytes = new Uint8Array(buf);

  const enc = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  let i = 0;
  const len = bytes.length;

  let out = '';
  while (i < len) {
    const c1 = bytes[i++];
    const c2 = (i !== len) ? bytes[i++] : 0;
    const c3 = (i !== len) ? bytes[i++] : 0;

    const o1 = c1 >> 2;
    const o2 = ((c1 & 0x03) << 4) | (c2 >> 4);
    const o3 = ((c2 & 0x0f) << 2) | (c3 >> 6);
    const o4 = c3 & 0x3f;

    if (i - 1 > len)
      out += enc[o1] + enc[o2] + '==';
    else if (i > len)
      out += enc[o1] + enc[o2] + enc[o3] + '=';
    else
      out += enc[o1] + enc[o2] + enc[o3] + enc[o4];
  }
  return out;
}
