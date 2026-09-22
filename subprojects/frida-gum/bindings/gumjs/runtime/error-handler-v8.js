_setUnhandledExceptionCallback(error => {
  const message = {
    type: 'error',
    description: '' + error
  };

  if (error instanceof Error) {
    const stack = error.stack;
    if (stack !== undefined) {
      message.stack = stack;

      const frame = stack.frames?.[0];
      if (frame !== undefined) {
        message.fileName = frame.getFileName();
        message.lineNumber = frame.getLineNumber();
        message.columnNumber = frame.getColumnNumber();
      }
    }
  }

  _send(JSON.stringify(message), null);
});

Error.prepareStackTrace = (error, stack) => {
  if (stack.length === 0) {
    const result = new String(error.toString());
    result.frames = [];
    return result;
  }
  const translatedStack = stack.map(wrapCallSite);
  if (translatedStack[0].toString() === 'Error (native)')
    translatedStack.splice(0, 1);
  const result = new String(error.toString() + translatedStack.map(frame => '\n    at ' + frame.toString()).join(''));
  result.frames = translatedStack;
  return result;
};

//
// Based on https://github.com/evanw/node-source-map-support
//

const sourceMapCache = {};

function wrapCallSite(frame) {
  const source = frame.getFileName() || frame.getScriptNameOrSourceURL();
  if (source) {
    const line = frame.getLineNumber();
    const column = frame.getColumnNumber() - 1;

    const position = mapSourcePosition({
      source: source,
      line: line,
      column: column
    });
    frame = cloneCallSite(frame);
    frame.getFileName = () => position.source;
    frame.getLineNumber = () => position.line;
    frame.getColumnNumber = () => position.column + 1;
    frame.getScriptNameOrSourceURL = () => position.source;
    return frame;
  }

  let origin = frame.isEval() && frame.getEvalOrigin();
  if (origin) {
    origin = mapEvalOrigin(origin);
    frame = cloneCallSite(frame);
    frame.getEvalOrigin = () => origin;
    return frame;
  }

  return frame;
}

function mapSourcePosition(position) {
  let item = sourceMapCache[position.source];
  if (item === undefined) {
    item = sourceMapCache[position.source] = {
      map: Script._findSourceMap(position.source)
    };
  }

  if (item.map !== null) {
    const originalPosition = item.map.resolve(position);

    if (originalPosition !== null)
      return originalPosition;
  }

  return position;
}

function mapEvalOrigin(origin) {
  let match = /^eval at ([^(]+) \((.+):(\d+):(\d+)\)$/.exec(origin);
  if (match !== null) {
    const position = mapSourcePosition({
      source: match[2],
      line: parseInt(match[3], 10),
      column: parseInt(match[4], 10) - 1
    });
    return 'eval at ' + match[1] + ' (' + position.source + ':' + position.line + ':' + (position.column + 1) + ')';
  }

  match = /^eval at ([^(]+) \((.+)\)$/.exec(origin);
  if (match !== null) {
    return 'eval at ' + match[1] + ' (' + mapEvalOrigin(match[2]) + ')';
  }

  return origin;
}

function cloneCallSite(frame) {
  const object = {};
  Object.getOwnPropertyNames(Object.getPrototypeOf(frame)).forEach(name => {
    object[name] = /^(?:is|get)/.test(name)
        ? () => frame[name].call(frame)
        : frame[name];
  });
  object.toString = CallSiteToString;
  return object;
}

function CallSiteToString() {
  let fileLocation = '';
  if (this.isNative()) {
    fileLocation = 'native';
  } else {
    const fileName = this.getScriptNameOrSourceURL();
    if (fileName === null && this.isEval()) {
      fileLocation = this.getEvalOrigin();
      fileLocation += ', ';
    }

    if (fileName !== null)
      fileLocation += fileName;
    else
      fileLocation += '<anonymous>';

    const lineNumber = this.getLineNumber();
    if (lineNumber !== 0) {
      fileLocation += ':' + lineNumber;
      const columnNumber = this.getColumnNumber();
      if (columnNumber !== 0)
        fileLocation += ':' + columnNumber;
    }
  }

  let line = '';
  const functionName = this.getFunctionName();
  let addSuffix = true;
  const isConstructor = this.isConstructor();
  const isMethodCall = !(this.isToplevel() || isConstructor);
  if (isMethodCall) {
    let typeName;
    try {
      typeName = this.getTypeName();
    } catch (e) {
      typeName = 'Proxy';
    }
    const methodName = this.getMethodName();
    if (functionName !== null) {
      if (typeName && functionName.indexOf(typeName) != 0) {
        line += typeName + '.';
      }
      line += functionName;
      if (methodName && functionName.indexOf('.' + methodName) != functionName.length - methodName.length - 1) {
        line += ' [as ' + methodName + ']';
      }
    } else {
      line += typeName + '.' + (methodName || '<anonymous>');
    }
  } else if (isConstructor) {
    line += 'new ' + (functionName ?? '<anonymous>');
  } else if (functionName !== null) {
    line += functionName;
  } else {
    line += fileLocation;
    addSuffix = false;
  }
  if (addSuffix) {
    line += ' (' + fileLocation + ')';
  }
  return line;
}
