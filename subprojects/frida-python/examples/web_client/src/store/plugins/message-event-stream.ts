/*
 * Browser-only version of https://github.com/maxogden/websocket-stream,
 * adapted to support both WebSocket and RTCDataChannel.
 */

import duplexify from "duplexify";
import { Duplex, Transform, TransformCallback } from "stream";

export interface Options {
  bufferSize?: number;
  bufferTimeout?: number;
}

export default function wrapEventStream(target: WebSocket | RTCDataChannel, options: Options = {}): Duplex {
  const proxy = new Transform();
  proxy._write = write;
  proxy._writev = writev;
  proxy._flush = flush;

  const bufferSize = options.bufferSize ?? 1024 * 512;
  const bufferTimeout = options.bufferTimeout ?? 1000;

  let stream: Transform | duplexify.Duplexify;
  const openStateValue = ('OPEN' in target) ? target.OPEN : 'open';
  if (target.readyState === openStateValue) {
    stream = proxy;
  } else {
    stream = duplexify();
    stream._writev = writev;

    target.addEventListener('open', onOpen);
  }

  target.binaryType = 'arraybuffer';

  target.addEventListener('close', onClose);
  target.addEventListener('error', onError as EventListener);
  target.addEventListener('message', onMessage as EventListener);

  proxy.on('close', destroy);

  function write(chunk: ArrayBuffer | string, encoding: BufferEncoding, callback: (error?: Error | null) => void) {
    if (target.bufferedAmount > bufferSize) {
      setTimeout(write, bufferTimeout, chunk, encoding, callback);
      return;
    }

    if (typeof chunk === 'string') {
      chunk = Buffer.from(chunk, 'utf8');
    }

    try {
      target.send(chunk);
    } catch (e) {
      return callback(e);
    }

    callback();
  }

  function writev (this: Duplex, chunks: { chunk: any, encoding: BufferEncoding }[], callback: (error?: Error | null) => void) {
    const buffers = chunks.map(({ chunk }) => (typeof chunk === 'string') ? Buffer.from(chunk, 'utf8') : chunk);
    this._write(Buffer.concat(buffers), 'binary', callback);
  }

  function flush(callback: TransformCallback) {
    target.close();
    callback();
  }

  function onOpen() {
    const ds = stream as duplexify.Duplexify;
    ds.setReadable(proxy);
    ds.setWritable(proxy);
    stream.emit('connect');
  }

  function onClose() {
    stream.end();
    stream.destroy();
  }

  function onError(event: ErrorEvent) {
    stream.destroy(new Error(event.message));
  }

  function onMessage(event: MessageEvent) {
    const { data } = event;
    proxy.push((data instanceof ArrayBuffer) ? Buffer.from(data) : Buffer.from(data, 'utf8'));
  }

  function destroy() {
    target.close();
  }

  return stream;
}
