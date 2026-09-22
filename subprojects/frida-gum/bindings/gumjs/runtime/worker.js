export class Worker {
  _pendingRequests = new Map();
  _nextRequestId = 1;

  constructor(url, { onMessage } = {}) {
    this._impl = new _Worker(url, this._dispatchMessage.bind(this, onMessage));

    this.exports = new WorkerExportsProxy(this);
  }

  terminate() {
    for (const callback of this._pendingRequests.values())
      callback(new Error('worker terminated'));
    this._pendingRequests.clear();
  }

  post(message, data = null) {
    this._impl.post(JSON.stringify(message), data);
  }

  _dispatchMessage(onMessage, rawMessage, data) {
    const message = JSON.parse(rawMessage);

    if (message.type !== 'send') {
      _send(rawMessage, data);
      return;
    }

    const {payload} = message;

    if (Array.isArray(payload) && payload[0] === 'frida:rpc') {
      const [, id, operation, ...params] = payload;
      this._onRpcMessage(id, operation, params, data);
      return;
    }

    onMessage?.(payload, data);
  }

  _request(operation, params) {
    return new Promise((resolve, reject) => {
      const id = this._nextRequestId++;

      this._pendingRequests.set(id, (error, result) => {
        this._pendingRequests.delete(id);

        if (error === null)
          resolve(result);
        else
          reject(error);
      });

      this.post(['frida:rpc', id, operation].concat(params));
    });
  }

  _onRpcMessage(id, operation, params, data) {
    switch (operation) {
      case 'ok':
      case 'error':
        break;
      default:
        return;
    }

    const callback = this._pendingRequests.get(id);
    if (callback === undefined)
      return;

    let value = null;
    let error = null;
    if (operation === 'ok') {
      value = (data !== null) ? data : params[0];
    } else {
      const [message, name, stack, rawErr] = params;
      error = new Error(message);
      error.name = name;
      error.stack = stack;
      Object.assign(error, rawErr);
    }

    callback(error, value);
  }
}

function WorkerExportsProxy(worker) {
  return new Proxy(this, {
    has(target, property) {
      return !isReservedMethodName(property);;
    },
    get(target, property, receiver) {
      if (property in target)
        return target[property];

      if (isReservedMethodName(property))
        return undefined;

      return (...args) => {
        return worker._request('call', [property, args]);
      };
    },
    set(target, property, value, receiver) {
      target[property] = value;
      return true;
    },
    ownKeys(target) {
      return Object.getOwnPropertyNames(target);
    },
    getOwnPropertyDescriptor(target, property) {
      if (property in target)
        return Object.getOwnPropertyDescriptor(target, property);

      if (isReservedMethodName(property))
        return undefined;

      return {
        writable: true,
        configurable: true,
        enumerable: true
      };
    },
  });
}

const reservedMethodNames = new Set([
  'then',
  'catch',
  'finally',
]);

function isReservedMethodName(name) {
  return reservedMethodNames.has(name.toString());
}
