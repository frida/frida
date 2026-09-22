import { Plugin } from 'vuex';

import dbus from 'dbus-next';
import events from 'events';
import wrapEventStream from './message-event-stream';

const {
  Interface, method,
} = dbus.interface;

const { Variant } = dbus;

async function start() {
  const ws = wrapEventStream(new WebSocket(`ws://${location.host}/ws`));
  const bus = dbus.peerBus(ws, {
    authMethods: [],
  });
  let peerBus: dbus.MessageBus | null = null;

  const authServiceObj = await bus.getProxyObject('re.frida.AuthenticationService15', '/re/frida/AuthenticationService');
  const authService = authServiceObj.getInterface('re.frida.AuthenticationService15');

  const token = JSON.stringify({
    nick: 'SomeoneOnTheWeb',
    secret: 'knock-knock'
  });
  await authService.authenticate(token);

  const hostSessionObj = await bus.getProxyObject('re.frida.HostSession15', '/re/frida/HostSession');
  const hostSession = hostSessionObj.getInterface('re.frida.HostSession15');

  const processes: HostProcessInfo[] = await hostSession.enumerateProcesses({});
  console.log('Got processes:', processes);

  const target = processes.find(([, name]) => name === 'hello2');
  if (target === undefined) {
    throw new Error('Target process not found');
  }
  const [pid] = target;
  console.log('Got PID:', pid);

  const sessionId: AgentSessionId = await hostSession.attach(pid, { 'persist-timeout': new Variant('u', 300) });

  let agentSessionObj = await bus.getProxyObject('re.frida.AgentSession15', '/re/frida/AgentSession/' + sessionId[0]);
  let agentSession = agentSessionObj.getInterface('re.frida.AgentSession15');

  const sink = new MessageSink('re.frida.AgentMessageSink15');
  bus.export('/re/frida/AgentMessageSink/' + sessionId[0], sink);

  const scriptId: AgentScriptId = await agentSession.createScript(`
const _puts = new NativeFunction(Module.getExportByName(null, 'puts'), 'int', ['pointer']);

function puts(s) {
  _puts(Memory.allocUtf8String(s));
}

send({ name: 'Joe' });
puts('Hello World from the browser!');

let n = 1;
setInterval(() => {
  send({ n });
  n++;
}, 1000);
  `, {});
  await agentSession.loadScript(scriptId);

  await agentSession.beginMigration();

  const peerConnection = new RTCPeerConnection();

  const pendingLocalCandidates = new IceCandidateQueue();
  pendingLocalCandidates.on('add', (candidates: RTCIceCandidate[]) => {
    agentSession.addCandidates(candidates.map(c => c.candidate));
  });
  pendingLocalCandidates.once('done', () => {
    agentSession.notifyCandidateGatheringDone();
  });

  const pendingRemoteCandidates = new IceCandidateQueue();
  pendingRemoteCandidates.on('add', candidates => {
    for (const candidate of candidates) {
      peerConnection.addIceCandidate(candidate);
    }
  });
  pendingRemoteCandidates.once('done', () => {
    peerConnection.addIceCandidate(new RTCIceCandidate({
      candidate: "",
      sdpMid: "0",
      sdpMLineIndex: 0
    }));
  });

  peerConnection.oniceconnectionstatechange = e => {
    console.log("ICE connection state changed:", peerConnection.iceConnectionState);
  };
  peerConnection.onicegatheringstatechange = e => {
    console.log("ICE gathering state changed:", peerConnection.iceGatheringState);
  };
  peerConnection.onicecandidate = e => {
    pendingLocalCandidates.add(e.candidate);
  };
  agentSession.on('newCandidates', (sdps: string[]) => {
    for (const sdp of sdps) {
      pendingRemoteCandidates.add(new RTCIceCandidate({
        candidate: sdp,
        sdpMid: "0",
        sdpMLineIndex: 0
      }));
    }
  });
  agentSession.on('candidateGatheringDone', () => {
    pendingRemoteCandidates.add(null);
  });

  const peerChannel = peerConnection.createDataChannel('session');
  peerChannel.onopen = async event => {
    console.log('[PeerChannel] onopen()');

    peerBus = dbus.peerBus(wrapEventStream(peerChannel), {
      authMethods: [],
    });

    const peerAgentSessionObj = await peerBus.getProxyObject('re.frida.AgentSession15', '/re/frida/AgentSession');
    const peerAgentSession = peerAgentSessionObj.getInterface('re.frida.AgentSession15');

    peerBus.export('/re/frida/AgentMessageSink', sink);

    await agentSession.commitMigration();

    agentSessionObj = peerAgentSessionObj;
    agentSession = peerAgentSession;

    console.log('Yay, migrated to p2p!');
  };
  peerChannel.onclose = event => {
    console.log('[PeerChannel] onclose()');
  };
  peerChannel.onerror = event => {
    console.log('[PeerChannel] onerror()');
  };
  const offer = await peerConnection.createOffer();
  await peerConnection.setLocalDescription(offer);

  const answerSdp = await agentSession.offerPeerConnection(offer.sdp, {});
  const answer = new RTCSessionDescription({ type: "answer", sdp: answerSdp });
  await peerConnection.setRemoteDescription(answer);

  pendingLocalCandidates.notifySessionStarted();
  pendingRemoteCandidates.notifySessionStarted();
}

type HostProcessInfo = [pid: string, name: string, smallIcon: ImageData, largeIcon: ImageData];
type ImageData = [width: number, height: number, rowstride: number, pixels: string];
type AgentSessionId = [handle: string];
type AgentScriptId = [handle: string];
type AgentMessage = [kind: number, scriptId: AgentScriptId, text: string, hasData: boolean, data: number[]];
enum AgentMessageKind {
  Script = 1,
  Debugger
}

class MessageSink extends Interface {
  @method({ inSignature: 'a(i(u)sbay)u' })
  postMessages(messages: AgentMessage[], batchId: number): void {
    for (const [kind, scriptId, text, hasData, data] of messages) {
      if (kind === AgentMessageKind.Script) {
        const message = JSON.parse(text);
        console.log("Got message:", message);
      }
    }
  }
}

class IceCandidateQueue extends events.EventEmitter {
  private sessionState: 'starting' | 'started' = 'starting';
  private gatheringState: 'gathering' | 'gathered' | 'notified' = 'gathering';
  private pending: RTCIceCandidate[] = [];
  private timer: NodeJS.Timeout | null = null;

  add(candidate: RTCIceCandidate | null) {
    if (candidate !== null) {
      this.pending.push(candidate);
    } else {
      this.gatheringState = 'gathered';
    }

    if (this.timer === null) {
      this.timer = setTimeout(this.maybeEmitCandidates, 10);
    }
  }

  notifySessionStarted() {
    this.sessionState = 'started';

    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }

    this.maybeEmitCandidates();
  }

  private maybeEmitCandidates = () => {
    this.timer = null;

    if (this.sessionState !== 'started') {
      return;
    }

    if (this.pending.length > 0) {
      this.emit('add', this.pending.splice(0));
    }

    if (this.gatheringState === 'gathered') {
      this.emit('done');
      this.gatheringState = 'notified';
    }
  };
}

export default function createFridaBusPlugin(): Plugin<any> {
  return (store: any) => {
    start().catch(e => {
      console.error(e);
    });
  };
}