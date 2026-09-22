import './core.js';
import './error-handler-quickjs.js';

Script.load = (name, source) => {
  return new Promise((resolve, reject) => {
    Script._load(name, source, async evalResult => {
      try {
        await evalResult;
        const namespace = await import(name);
        resolve(namespace);
      } catch (e) {
        reject(e);
      }
    });
  });
};

class WeakRef {
  constructor(target) {
    this._id = Script.bindWeak(target, this._onTargetDead);
  }

  deref() {
    if (this._id === null)
      return;
    return Script._derefWeak(this._id);
  }

  _onTargetDead = () => {
    this._id = null;
  };
}

globalThis.WeakRef = WeakRef;
