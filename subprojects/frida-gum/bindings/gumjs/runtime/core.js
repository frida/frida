import { Console } from './console.js';
import { hexdump } from './hexdump.js';
import { MessageDispatcher } from './message-dispatcher.js';
import { Worker } from './worker.js';

let messageDispatcher;

function initialize() {
  messageDispatcher = new MessageDispatcher();

  const proxyClass = globalThis.Proxy;
  if ('create' in proxyClass) {
    const createProxy = proxyClass.create;
    globalThis.Proxy = function (target, handler) {
      return createProxy.call(proxyClass, handler, Object.getPrototypeOf(target));
    };
  }
}

Object.defineProperties(globalThis, {
  rpc: {
    enumerable: true,
    value: {
      exports: {}
    }
  },
  recv: {
    enumerable: true,
    value: function () {
      let type, callback;
      if (arguments.length === 1) {
        type = '*';
        callback = arguments[0];
      } else {
        type = arguments[0];
        callback = arguments[1];
      }
      return messageDispatcher.registerCallback(type, callback);
    }
  },
  send: {
    enumerable: true,
    value: function (payload, data) {
      const message = {
        type: 'send',
        payload: payload
      };
      globalThis._send(JSON.stringify(message), data || null);
    }
  },
  setTimeout: {
    enumerable: true,
    value: function (func, delay = 0, ...args) {
      return _setTimeout(function () {
        func.apply(null, args);
      }, delay);
    }
  },
  setInterval: {
    enumerable: true,
    value: function (func, delay, ...args) {
      return _setInterval(function () {
        func.apply(null, args);
      }, delay);
    }
  },
  setImmediate: {
    enumerable: true,
    value: function (func, ...args) {
      return setTimeout(func, 0, ...args);
    }
  },
  clearImmediate: {
    enumerable: true,
    value: function (id) {
      clearTimeout(id);
    }
  },
  int64: {
    enumerable: true,
    value: function (value) {
      return new Int64(value);
    }
  },
  uint64: {
    enumerable: true,
    value: function (value) {
      return new UInt64(value);
    }
  },
  ptr: {
    enumerable: true,
    value: function (str) {
      return new NativePointer(str);
    }
  },
  NULL: {
    enumerable: true,
    value: new NativePointer('0')
  },
  console: {
    enumerable: true,
    value: new Console()
  },
  hexdump: {
    enumerable: true,
    value: hexdump
  },
  Worker: {
    enumerable: true,
    value: Worker
  },
});

[
  Int64,
  UInt64,
  NativePointer
].forEach(klass => {
  klass.prototype.equals = numberWrapperEquals;
});

function numberWrapperEquals(rhs) {
  return this.compare(rhs) === 0;
}

const _nextTick = Script._nextTick;
Script.nextTick = function (callback, ...args) {
  _nextTick(callback.bind(globalThis, ...args));
};

makeEnumerateRanges(Kernel);

Object.defineProperties(Kernel, {
  scan: {
    enumerable: true,
    value: function (address, size, pattern, callbacks) {
      let onSuccess, onFailure;
      const request = new Promise((resolve, reject) => {
        onSuccess = resolve;
        onFailure = reject;
      });

      Kernel._scan(address, size, pattern, {
        onMatch: callbacks.onMatch,
        onError(reason) {
          onFailure(new Error(reason));
          callbacks.onError?.();
        },
        onComplete() {
          onSuccess();
          callbacks.onComplete?.();
        }
      });

      return request;
    }
  }
});

Object.defineProperties(Memory, {
  alloc: {
    enumerable: true,
    value: function (size, { near, maxDistance, protection } = {}) {
      if (near !== undefined && maxDistance === undefined)
        throw new Error('missing maxDistance option');

      return Memory._alloc(size, protection ?? "rw", near ?? NULL, maxDistance ?? 0);
    }
  },
  dup: {
    enumerable: true,
    value: function (mem, size) {
      const result = Memory.alloc(size);
      Memory.copy(result, mem, size);
      return result;
    }
  },
  patchCode: {
    enumerable: true,
    value: function (address, size, apply) {
      Memory._checkCodePointer(address);
      Memory._patchCode(address, size, apply);
    }
  },
  scan: {
    enumerable: true,
    value: function (address, size, pattern, callbacks) {
      let onSuccess, onFailure;
      const request = new Promise((resolve, reject) => {
        onSuccess = resolve;
        onFailure = reject;
      });

      Memory._scan(address, size, pattern, {
        onMatch: callbacks.onMatch,
        onError(reason) {
          onFailure(new Error(reason));
          callbacks.onError?.(reason);
        },
        onComplete() {
          onSuccess();
          callbacks.onComplete?.();
        }
      });

      return request;
    }
  }
});

Object.defineProperties(Module, {
  getGlobalExportByName: {
    enumerable: true,
    value(symbolName) {
      const address = Module.findGlobalExportByName(symbolName);
      if (address === null)
        throw new Error(`unable to find global export '${symbolName}'`);
      return address;
    }
  },
});

const moduleProto = Module.prototype;

Object.defineProperties(moduleProto, {
  getExportByName: {
    enumerable: true,
    value(symbolName) {
      const address = this.findExportByName(symbolName);
      if (address === null)
        throw new Error(`${this.path}: unable to find export '${symbolName}'`);
      return address;
    }
  },
  getSymbolByName: {
    enumerable: true,
    value(symbolName) {
      const address = this.findSymbolByName(symbolName);
      if (address === null)
        throw new Error(`${this.path}: unable to find symbol '${symbolName}'`);
      return address;
    }
  },
  toJSON: {
    enumerable: true,
    value() {
      const {name, version, base, size, path} = this;
      return {name, version, base, size, path};
    }
  },
});

Object.defineProperties(ModuleMap.prototype, {
  get: {
    enumerable: true,
    value: function (address) {
      const details = this.find(address);
      if (details === null)
        throw new Error(`unable to find module containing ${address}`);
      return details;
    }
  },
  getName: {
    enumerable: true,
    value: function (address) {
      const name = this.findName(address);
      if (name === null)
        throw new Error(`unable to find module containing ${address}`);
      return name;
    }
  },
  getPath: {
    enumerable: true,
    value: function (address) {
      const path = this.findPath(address);
      if (path === null)
        throw new Error(`unable to find module containing ${address}`);
      return path;
    }
  },
});

makeEnumerateRanges(Process);

Object.defineProperties(Process, {
  getThreadById: {
    enumerable: true,
    value: function (id) {
      const thread = Process.findThreadById(id);
      if (thread === null)
        throw new Error(`unable to find thread with id ${id}`);
      return thread;
    }
  },
  runOnThread: {
    enumerable: true,
    value: function (threadId, callback) {
      return new Promise((resolve, reject) => {
        Process._runOnThread(threadId, () => {
          try {
            resolve(callback());
          } catch (e) {
            reject(e);
          }
        });
      });
    },
  },
  getModuleByAddress: {
    enumerable: true,
    value: function (address) {
      const module = Process.findModuleByAddress(address);
      if (module === null)
        throw new Error(`unable to find module containing ${address}`);
      return module;
    }
  },
  getModuleByName: {
    enumerable: true,
    value: function (name) {
      const module = Process.findModuleByName(name);
      if (module === null)
        throw new Error(`unable to find module '${name}'`);
      return module;
    }
  },
  getRangeByAddress: {
    enumerable: true,
    value: function (address) {
      const range = Process.findRangeByAddress(address);
      if (range === null)
        throw new Error(`unable to find range containing ${address}`);
      return range;
    }
  },
  getFunctionRange: {
    enumerable: true,
    value: function (address) {
      const range = Process.findFunctionRange(address);
      if (range === null)
        throw new Error(`unable to find function containing ${address}`);
      return range;
    }
  },
});

Object.defineProperties(Thread, {
  backtrace: {
    enumerable: true,
    value: function (cpuContext = null, backtracerOrOptions = {}) {
      const options = (typeof backtracerOrOptions === 'object')
          ? backtracerOrOptions
          : { backtracer: backtracerOrOptions };

      const {
        backtracer = Backtracer.ACCURATE,
        limit = 0,
      } = options;

      return Thread._backtrace(cpuContext, backtracer, limit);
    }
  },
});

if ('Interceptor' in globalThis) {
  const getCodeTarget = spec => spec.target ?? spec;

  Object.defineProperties(Interceptor, {
    attach: {
      enumerable: true,
      value: function (target, callbacks, data) {
        Memory._checkCodePointer(getCodeTarget(target));
        return Interceptor._attach(target, callbacks, data);
      }
    },
    replace: {
      enumerable: true,
      value: function (target, replacement, data) {
        Memory._checkCodePointer(getCodeTarget(target));
        Interceptor._replace(target, replacement, data);
      }
    },
    replaceFast: {
      enumerable: true,
      value: function (target, replacement) {
        Memory._checkCodePointer(getCodeTarget(target));
        return Interceptor._replaceFast(target, replacement);
      }
    },
  });
}

if ('Stalker' in globalThis) {
  const stalkerEventType = {
    call: 1,
    ret: 2,
    exec: 4,
    block: 8,
    compile: 16,
  };

  Object.defineProperties(Stalker, {
    exclude: {
      enumerable: true,
      value: function (range) {
        Stalker._exclude(range.base, range.size);
      }
    },
    follow: {
      enumerable: true,
      value: function (first, second) {
        let threadId = first;
        let options = second;

        if (typeof first === 'object') {
          threadId = undefined;
          options = first;
        }

        if (threadId === undefined)
          threadId = Process.getCurrentThreadId();
        if (options === undefined)
          options = {};

        if (typeof threadId !== 'number' || (options === null || typeof options !== 'object'))
          throw new Error('invalid argument');

        const {
          transform = null,
          events = {},
          onReceive = null,
          onCallSummary = null,
          onEvent = NULL,
          data = NULL,
        } = options;

        if (events === null || typeof events !== 'object')
          throw new Error('events must be an object');

        if (!data.isNull() && (onReceive !== null || onCallSummary !== null))
          throw new Error('onEvent precludes passing onReceive/onCallSummary');

        const eventMask = Object.keys(events).reduce((result, name) => {
          const value = stalkerEventType[name];
          if (value === undefined)
            throw new Error(`unknown event type: ${name}`);

          const enabled = events[name];
          if (typeof enabled !== 'boolean')
            throw new Error('desired events must be specified as boolean values');

          return enabled ? (result | value) : result;
        }, 0);

        Stalker._follow(threadId, transform, eventMask, onReceive, onCallSummary, onEvent, data);
      }
    },
    parse: {
      enumerable: true,
      value: function (events, options = {}) {
        const {
          annotate = true,
          stringify = false
        } = options;

        return Stalker._parse(events, annotate, stringify);
      }
    }
  });
}

Object.defineProperty(Instruction, 'parse', {
  enumerable: true,
  value: function (target) {
    Memory._checkCodePointer(target);
    return Instruction._parse(target);
  }
});

if ('IOStream' in globalThis) {
  const _closeIOStream = IOStream.prototype._close;
  IOStream.prototype.close = function () {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _closeIOStream.call(stream, function (error, success) {
        if (error === null)
          resolve(success);
        else
          reject(error);
      });
    });
  };

  const _closeInput = InputStream.prototype._close;
  InputStream.prototype.close = function () {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _closeInput.call(stream, function (error, success) {
        if (error === null)
          resolve(success);
        else
          reject(error);
      });
    });
  };

  const _read = InputStream.prototype._read;
  InputStream.prototype.read = function (size) {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _read.call(stream, size, function (error, data) {
        if (error === null)
          resolve(data);
        else
          reject(error);
      });
    });
  };

  const _readAll = InputStream.prototype._readAll;
  InputStream.prototype.readAll = function (size) {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _readAll.call(stream, size, function (error, data) {
        if (error === null) {
          resolve(data);
        } else {
          error.partialData = data;
          reject(error);
        }
      });
    });
  };

  const _closeOutput = OutputStream.prototype._close;
  OutputStream.prototype.close = function () {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _closeOutput.call(stream, function (error, success) {
        if (error === null)
          resolve(success);
        else
          reject(error);
      });
    });
  };

  const _write = OutputStream.prototype._write;
  OutputStream.prototype.write = function (data) {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _write.call(stream, data, function (error, size) {
        if (error === null)
          resolve(size);
        else
          reject(error);
      });
    });
  };

  const _writeAll = OutputStream.prototype._writeAll;
  OutputStream.prototype.writeAll = function (data) {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _writeAll.call(stream, data, function (error, size) {
        if (error === null) {
          resolve(size);
        } else {
          error.partialSize = size;
          reject(error);
        }
      });
    });
  };

  const _writeMemoryRegion = OutputStream.prototype._writeMemoryRegion;
  OutputStream.prototype.writeMemoryRegion = function (address, length) {
    const stream = this;
    return new Promise(function (resolve, reject) {
      _writeMemoryRegion.call(stream, address, length, function (error, size) {
        if (error === null) {
          resolve(size);
        } else {
          error.partialSize = size;
          reject(error);
        }
      });
    });
  };

  const _closeListener = SocketListener.prototype._close;
  SocketListener.prototype.close = function () {
    const listener = this;
    return new Promise(function (resolve) {
      _closeListener.call(listener, resolve);
    });
  };

  const _accept = SocketListener.prototype._accept;
  SocketListener.prototype.accept = function () {
    const listener = this;
    return new Promise(function (resolve, reject) {
      _accept.call(listener, function (error, connection) {
        if (error === null)
          resolve(connection);
        else
          reject(error);
      });
    });
  };

  const _setNoDelay = SocketConnection.prototype._setNoDelay;
  SocketConnection.prototype.setNoDelay = function (noDelay = true) {
    const connection = this;
    return new Promise(function (resolve, reject) {
      _setNoDelay.call(connection, noDelay, function (error, success) {
        if (error === null)
          resolve(success);
        else
          reject(error);
      });
    });
  };

  Object.defineProperties(Socket, {
    listen: {
      enumerable: true,
      value: function (options = {}) {
        return new Promise(function (resolve, reject) {
          const {
            family = null,

            host = null,
            port = 0,

            type = null,
            path = null,

            backlog = 10,
          } = options;

          Socket._listen(family, host, port, type, path, backlog, function (error, listener) {
            if (error === null)
              resolve(listener);
            else
              reject(error);
          });
        });
      },
    },
    connect: {
      enumerable: true,
      value: function (options) {
        return new Promise(function (resolve, reject) {
          const {
            family = null,

            host = 'localhost',
            port = 0,

            type = null,
            path = null,

            tls = false,
          } = options;

          Socket._connect(family, host, port, type, path, tls, function (error, connection) {
            if (error === null)
              resolve(connection);
            else
              reject(error);
          });
        });
      },
    },
  });
}

SourceMap.prototype.resolve = function (generatedPosition) {
  const generatedColumn = generatedPosition.column;
  const position = (generatedColumn !== undefined)
      ? this._resolve(generatedPosition.line, generatedColumn)
      : this._resolve(generatedPosition.line);
  if (position === null)
    return null;

  const [source, line, column, name] = position;

  return {source, line, column, name};
};

if ('SqliteDatabase' in globalThis) {
  const sqliteOpenFlags = {
    readonly: 1,
    readwrite: 2,
    create: 4,
  };

  Object.defineProperties(SqliteDatabase, {
    open: {
      enumerable: true,
      value: function (file, options = {}) {
        if (typeof file !== 'string' || (options === null || typeof options !== 'object'))
          throw new Error('invalid argument');

        const {
          flags = ['readwrite', 'create'],
        } = options;

        if (!(flags instanceof Array) || flags.length === 0)
          throw new Error('flags must be a non-empty array');

        const flagsValue = flags.reduce((result, name) => {
          const value = sqliteOpenFlags[name];
          if (value === undefined)
            throw new Error(`unknown flag: ${name}`);

          return result | value;
        }, 0);

        if (flagsValue === 3 || flagsValue === 5 || flagsValue === 7)
          throw new Error(`invalid flags combination: ${flags.join(' | ')}`);

        return SqliteDatabase._open(file, flagsValue);
      }
    }
  });
}

Object.defineProperties(Cloak, {
  hasCurrentThread: {
    enumerable: true,
    value() {
      return Cloak.hasThread(Process.getCurrentThreadId());
    }
  },
  addRange: {
    enumerable: true,
    value(range) {
      Cloak._addRange(range.base, range.size);
    }
  },
  removeRange: {
    enumerable: true,
    value(range) {
      Cloak._removeRange(range.base, range.size);
    }
  },
  clipRange: {
    enumerable: true,
    value(range) {
      return Cloak._clipRange(range.base, range.size);
    }
  },
});

function makeEnumerateRanges(mod) {
  const impl = mod['_enumerateRanges'];

  Object.defineProperties(mod, {
    enumerateRanges: {
      enumerable: true,
      value(specifier) {
        return enumerateRanges(impl, this, specifier);
      }
    },
  });
}

function enumerateRanges(impl, self, specifier) {
  let protection;
  let coalesce = false;
  if (typeof specifier === 'string') {
    protection = specifier;
  } else {
    protection = specifier.protection;
    coalesce = specifier.coalesce;
  }

  const ranges = impl.call(self, protection);

  if (coalesce) {
    const coalesced = [];

    let current = null;
    for (const r of ranges) {
      if (current !== null) {
        if (r.base.equals(current.base.add(current.size)) && r.protection === current.protection) {
          const coalescedRange = {
            base: current.base,
            size: current.size + r.size,
            protection: current.protection
          };
          if (current.hasOwnProperty('file'))
            coalescedRange.file = current.file;
          Object.freeze(coalescedRange);
          current = coalescedRange;
        } else {
          coalesced.push(current);
          current = r;
        }
      } else {
        current = r;
      }
    }

    if (current !== null)
      coalesced.push(current);

    return coalesced;
  }

  return ranges;
}

initialize();
