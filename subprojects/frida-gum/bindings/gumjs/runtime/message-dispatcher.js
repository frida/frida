export function MessageDispatcher() {
  const messages = [];
  const operations = {};

  function initialize() {
    _setIncomingMessageCallback(handleMessage);
  }

  this.registerCallback = function registerCallback(type, callback) {
    const op = new MessageRecvOperation(callback);

    const opsForType = operations[type];
    if (opsForType === undefined)
      operations[type] = [op[1]];
    else
      opsForType.push(op[1]);

    dispatchMessages();
    return op[0];
  };

  function handleMessage(rawMessage, data) {
    const message = JSON.parse(rawMessage);
    if (message instanceof Array && message[0] === 'frida:rpc') {
      handleRpcMessage(message[1], message[2], message.slice(3), data);
    } else {
      messages.push([message, data]);
      dispatchMessages();
    }
  }

  function handleRpcMessage(id, operation, params, data) {
    const exports = rpc.exports;

    if (operation === 'call') {
      const method = params[0];
      const args = params[1];

      if (!exports.hasOwnProperty(method)) {
        reply(id, 'error', "unable to find method '" + method + "'");
        return;
      }

      try {
        const result = exports[method].call(exports, ...args, data);
        if (typeof result === 'object' && result !== null &&
            typeof result.then === 'function') {
          result
          .then(value => {
            reply(id, 'ok', value);
          })
          .catch(error => {
            reply(id, 'error', error.message, [error.name, error.stack, error]);
          });
        } else {
          reply(id, 'ok', result);
        }
      } catch (e) {
        reply(id, 'error', e.message, [e.name, e.stack, e]);
      }
    } else if (operation === 'list') {
      reply(id, 'ok', Object.keys(exports));
    }
  }

  function reply(id, type, result, params = []) {
    if (Array.isArray(result) && result.length === 2 && result[1] instanceof ArrayBuffer) {
      const [value, data] = result;
      send(['frida:rpc', id, type, undefined, value, ...params], data);
    } else if (result instanceof ArrayBuffer) {
      send(['frida:rpc', id, type, undefined, ...params], result);
    } else {
      send(['frida:rpc', id, type, result, ...params]);
    }
  }

  function dispatchMessages() {
    messages.splice(0).forEach(dispatch);
  }

  function dispatch(item) {
    const [message, data] = item;

    let handlerType;
    if (operations.hasOwnProperty(message.type)) {
      handlerType = message.type;
    } else if (operations.hasOwnProperty('*')) {
      handlerType = '*';
    } else {
      messages.push(item);
      return;
    }

    const opsForType = operations[handlerType];
    const complete = opsForType.shift();
    if (opsForType.length === 0)
      delete operations[handlerType];

    complete(message, data);
  }

  initialize();
};

function MessageRecvOperation(callback) {
  let completed = false;

  this.wait = function wait() {
    while (!completed)
      _waitForEvent();
  };

  function complete(message, data) {
    try {
      callback(message, data);
    } finally {
      completed = true;
    }
  }

  return [this, complete];
}
