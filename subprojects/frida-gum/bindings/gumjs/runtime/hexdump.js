export function hexdump(target, options = {}) {
  const startOffset = options.offset || 0;
  let length = options.length;
  const showHeader = options.hasOwnProperty('header') ? options.header : true;
  const useAnsi = options.hasOwnProperty('ansi') ? options.ansi : false;

  let buffer;
  let defaultStartAddress = NULL;
  if (target instanceof ArrayBuffer) {
    if (length === undefined)
      length = target.byteLength;
    else
      length = Math.min(length, target.byteLength);
    buffer = target;
  } else {
    if (!(target instanceof NativePointer))
      target = target.handle;
    if (length === undefined)
      length = 256;
    buffer = target.readByteArray(length);
    defaultStartAddress = target;
  }

  const startAddress = options.hasOwnProperty('address') ? options.address : defaultStartAddress;
  const endAddress = startAddress.add(length);

  const bytes = new Uint8Array(buffer);

  const columnPadding = '  ';
  const leftColumnWidth = Math.max(endAddress.toString(16).length, 8);
  const hexLegend = ' 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F';
  const asciiLegend = '0123456789ABCDEF';

  let resetColor, offsetColor, dataColor, newlineColor;
  if (useAnsi) {
    resetColor = '\x1b[0m';
    offsetColor = '\x1b[0;32m';
    dataColor = '\x1b[0;33m';
    newlineColor = resetColor;
  } else {
    resetColor = '';
    offsetColor = '';
    dataColor = '';
    newlineColor = '';
  }

  const result = [];

  if (showHeader) {
    result.push(
      pad('        ', leftColumnWidth, ' '),
      columnPadding,
      hexLegend,
      columnPadding,
      asciiLegend,
      '\n'
    );
  }

  let offset = startOffset;
  for (let bufferOffset = 0; bufferOffset < length; bufferOffset += 16) {
    if (bufferOffset !== 0)
      result.push('\n');

    result.push(
      offsetColor, pad(startAddress.add(offset).toString(16), leftColumnWidth, '0'), resetColor,
      columnPadding
    );

    const asciiChars = [];
    const lineSize = Math.min(length - offset, 16);

    for (let lineOffset = 0; lineOffset !== lineSize; lineOffset++) {
      const value = bytes[offset++];

      const isNewline = value === 10;

      const hexPair = pad(value.toString(16), 2, '0');
      if (lineOffset !== 0)
        result.push(' ');
      result.push(
        isNewline ? newlineColor : dataColor,
        hexPair,
        resetColor
      );

      asciiChars.push(
        isNewline ? newlineColor : dataColor,
        (value >= 32 && value <= 126) ? String.fromCharCode(value) : '.',
        resetColor
      );
    }

    for (let lineOffset = lineSize; lineOffset !== 16; lineOffset++) {
      result.push('   ');
      asciiChars.push(' ');
    }

    result.push(columnPadding);

    Array.prototype.push.apply(result, asciiChars);
  }

  let trailingSpaceCount = 0;
  for (let tailOffset = result.length - 1; tailOffset >= 0 && result[tailOffset] === ' '; tailOffset--) {
    trailingSpaceCount++;
  }

  return result.slice(0, result.length - trailingSpaceCount).join('');
}

function pad(str, width, fill) {
  const result = [];
  const paddingSize = Math.max(width - str.length, 0);
  for (let index = 0; index !== paddingSize; index++) {
    result.push(fill);
  }
  return result.join('') + str;
}
