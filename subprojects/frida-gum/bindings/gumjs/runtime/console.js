import { hexdump } from './hexdump.js';

export class Console {
  #counters;

  constructor() {
    this.#counters = new Map();
  }

  info(...args) {
    sendLogMessage('info', args);
  }

  log(...args) {
    sendLogMessage('info', args);
  }

  debug(...args) {
    sendLogMessage('debug', args);
  }

  warn(...args) {
    sendLogMessage('warning', args);
  }

  error(...args) {
    sendLogMessage('error', args);
  }

  count(label = 'default') {
    const newValue = (this.#counters.get(label) ?? 0) + 1;
    this.#counters.set(label, newValue);
    this.log(`${label}: ${newValue}`);
  }

  countReset(label = 'default') {
    if (this.#counters.has(label)) {
      this.#counters.delete(label);
    } else {
      this.warn(`Count for '${label}' does not exist`);
    }
  }
}

function sendLogMessage(level, values) {
  const text = values.map(parseLogArgument).join(' ');
  const message = {
    type: 'log',
    level: level,
    payload: text
  };
  _send(JSON.stringify(message), null);
}

function parseLogArgument(value) {
  if (value instanceof ArrayBuffer)
    return hexdump(value);

  if (value === undefined)
    return 'undefined';

  if (value === null)
    return 'null';

  return value;
}
