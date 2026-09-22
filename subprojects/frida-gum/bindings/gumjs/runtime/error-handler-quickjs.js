_setUnhandledExceptionCallback(error => {
  const message = {
    type: 'error',
    description: '' + error
  };

  if (error instanceof Error) {
    const stack = error.stack;
    if (stack !== undefined) {
      message.stack = stack;
    }

    const fileName = error.fileName;
    if (fileName !== undefined) {
      message.fileName = fileName;
    }

    const lineNumber = error.lineNumber;
    if (lineNumber !== undefined) {
      message.lineNumber = lineNumber;
      message.columnNumber = 1;
    }
  }

  _send(JSON.stringify(message), null);
});

if (Process.platform !== 'barebone') {
  Error.prepareStackTrace = (error, stack) => {
    let firstSourcePosition = null;

    stack = error.toString() + '\n' + stack.replace(/    at (.+) \((.+):(.+)\)/g,
        (match, scope, fileName, lineNumber) => {
          const position = mapSourcePosition({
            source: fileName,
            line: parseInt(lineNumber, 10)
          });

          if (firstSourcePosition === null)
            firstSourcePosition = position;

          return `    at ${scope} (${position.source}:${position.line})`;
        })
        .trimEnd();

    if (firstSourcePosition !== null) {
      error.fileName = firstSourcePosition.source;
      error.lineNumber = firstSourcePosition.line;
    }

    return stack;
  };
}

/*
 * Based on https://github.com/evanw/node-source-map-support
 */

const sourceMapCache = {};

function mapSourcePosition(position) {
  let item = sourceMapCache[position.source];
  if (item === undefined) {
    item = sourceMapCache[position.source] = {
      map: Script._findSourceMap(position.source)
    };
  }

  if (item.map !== null) {
    const originalPosition = item.map.resolve(position);

    // Only return the original position if a matching line was found. If no
    // matching line is found then we return position instead, which will cause
    // the stack trace to print the path and line for the compiled file. It is
    // better to give a precise location in the compiled file than a vague
    // location in the original file.
    if (originalPosition !== null)
      return originalPosition;
  }

  return position;
}
