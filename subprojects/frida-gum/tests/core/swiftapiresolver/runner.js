class Runner {
  constructor() {
    this._cm = null;
    this._run = null;
    recv('start', this._onStart);
  }

  run(query) {
    return this._run(Memory.allocUtf8String(query));
  }

  _onStart = (message, data) => {
    this._cm = new CModule(data);
    this._run = new NativeFunction(this._cm.run, 'uint', ['pointer'], { exceptions: 'propagate' });
    send({ type: 'ready', symbols: this._cm });
  };
}

const runner = new Runner();

rpc.exports = {
  run(query) {
    return runner.run(query);
  }
};
