import bisect
import re

from Foundation import NSAutoreleasePool, NSObject, NSThread
from PyObjCTools import AppHelper

PROBE_CALLS = re.compile(r"^\/stalker\/probes\/(.*?)\/calls$")


class Capture(NSObject):
    def __new__(cls, device):
        return cls.alloc().initWithDevice_(device)

    def initWithDevice_(self, device):
        self = self.init()
        self.state = CaptureState.DETACHED
        self.device = device
        self._delegate = None
        self.session = None
        self.script = None
        self.modules = Modules()
        self.recvTotal = 0
        self.calls = Calls(self)
        return self

    def delegate(self):
        return self._delegate

    def setDelegate_(self, delegate):
        self._delegate = delegate

    def attachToProcess_triggerPort_(self, process, triggerPort):
        assert self.state == CaptureState.DETACHED
        self._updateState_(CaptureState.ATTACHING)
        NSThread.detachNewThreadSelector_toTarget_withObject_("_doAttachWithParams:", self, (process.pid, triggerPort))

    def detach(self):
        assert self.state == CaptureState.ATTACHED
        session = self.session
        script = self.script
        self.session = None
        self.script = None
        self._updateState_(CaptureState.DETACHED)
        NSThread.detachNewThreadSelector_toTarget_withObject_("_doDetachWithParams:", self, (session, script))

    def _post(self, message):
        NSThread.detachNewThreadSelector_toTarget_withObject_("_doPostWithParams:", self, (self.script, message))

    def _updateState_(self, newState):
        self.state = newState
        self._delegate.captureStateDidChange()

    def _doAttachWithParams_(self, params):
        pid, triggerPort = params
        pool = NSAutoreleasePool.alloc().init()
        session = None
        script = None
        error = None
        try:
            session = self.device.attach(pid)
            session.on("detached", self._onSessionDetached)
            script = session.create_script(name="cpushark", source=SCRIPT_TEMPLATE % {"trigger_port": triggerPort})
            script.on("message", self._onScriptMessage)
            script.load()
        except Exception as e:
            if session is not None:
                try:
                    session.detach()
                except:
                    pass
                session = None
            script = None
            error = e
        AppHelper.callAfter(self._attachDidCompleteWithSession_script_error_, session, script, error)
        del pool

    def _doDetachWithParams_(self, params):
        session, script = params
        pool = NSAutoreleasePool.alloc().init()
        try:
            script.unload()
        except:
            pass
        try:
            session.detach()
        except:
            pass
        del pool

    def _doPostWithParams_(self, params):
        script, message = params
        pool = NSAutoreleasePool.alloc().init()
        try:
            script.post(message)
        except Exception as e:
            print("Failed to post to script:", e)
        del pool

    def _attachDidCompleteWithSession_script_error_(self, session, script, error):
        if self.state == CaptureState.ATTACHING:
            self.session = session
            self.script = script
            if error is None:
                self._updateState_(CaptureState.ATTACHED)
            else:
                self._updateState_(CaptureState.DETACHED)
                self._delegate.captureFailedToAttachWithError_(error)

    def _sessionDidDetach(self):
        if self.state == CaptureState.ATTACHING or self.state == CaptureState.ATTACHED:
            self.session = None
            self._updateState_(CaptureState.DETACHED)

    def _sessionDidReceiveMessage_data_(self, message, data):
        if message["type"] == "send":
            stanza = message["payload"]
            fromAddress = stanza["from"]
            name = stanza["name"]
            if fromAddress == "/process/modules" and name == "+sync":
                self.modules._sync(stanza["payload"])
            elif fromAddress == "/stalker/calls" and name == "+add":
                self.calls._add_(stanza["payload"])
            elif fromAddress == "/interceptor/functions" and name == "+add":
                self.recvTotal += 1
                self._delegate.captureRecvTotalDidChange()
            else:
                if not self.calls._handleStanza_(stanza):
                    print(f"Woot! Got stanza: {stanza['name']} from={stanza['from']}")
        else:
            print("Unhandled message:", message)

    def _onSessionDetached(self):
        AppHelper.callAfter(self._sessionDidDetach)

    def _onScriptMessage(self, message, data):
        AppHelper.callAfter(self._sessionDidReceiveMessage_data_, message, data)


class CaptureState:
    DETACHED = 1
    ATTACHING = 2
    ATTACHED = 3


class Modules:
    def __init__(self):
        self._modules = []
        self._indices = []

    def _sync(self, payload):
        modules = []
        for item in payload["items"]:
            modules.append(Module(item["name"], int(item["base"], 16), item["size"]))
        modules.sort(lambda x, y: x.address - y.address)
        self._modules = modules
        self._indices = [m.address for m in modules]

    def lookup(self, addr):
        idx = bisect.bisect(self._indices, addr)
        if idx == 0:
            return None
        m = self._modules[idx - 1]
        if addr >= m.address + m.size:
            return None
        return m


class Module:
    def __init__(self, name, address, size):
        self.name = name
        self.address = address
        self.size = size

    def __repr__(self):
        return "(%d, %d, %s)" % (self.address, self.size, self.name)


class Calls(NSObject):
    def __new__(cls, capture):
        return cls.alloc().initWithCapture_(capture)

    def initWithCapture_(self, capture):
        self = self.init()
        self.capture = capture
        self.targetModules = []
        self._targetModuleByAddress = {}
        self._delegate = None
        self._probes = {}
        return self

    def delegate(self):
        return self._delegate

    def setDelegate_(self, delegate):
        self._delegate = delegate

    def addProbe_(self, func):
        self.capture._post({"to": "/stalker/probes", "name": "+add", "payload": {"address": "0x%x" % func.address}})
        self._probes[func.address] = func

    def removeProbe_(self, func):
        self.capture._post({"to": "/stalker/probes", "name": "+remove", "payload": {"address": "0x%x" % func.address}})
        self._probes.pop(func.address, None)

    def _add_(self, data):
        modules = self.capture.modules
        for rawTarget, count in data["summary"].items():
            target = int(rawTarget, 16)
            tm = self.getTargetModuleByModule_(modules.lookup(target))
            if tm is not None:
                tm.total += count
                tf = tm.getTargetFunctionByAddress_(target)
                tf.total += count

        self.targetModules.sort(key=lambda tm: tm.total, reverse=True)
        for tm in self.targetModules:
            tm.functions.sort(self._compareFunctions)
        self._delegate.callsDidChange()

    def _compareFunctions(self, x, y):
        if x.hasProbe == y.hasProbe:
            return x.total - y.total
        elif x.hasProbe:
            return -1
        elif y.hasProbe:
            return 1
        else:
            return x.total - y.total

    def _handleStanza_(self, stanza):
        m = PROBE_CALLS.match(stanza["from"])
        if m is not None:
            func = self._probes.get(int(m.groups()[0], 16), None)
            if func is not None:
                if len(func.calls) == 3:
                    func.calls.pop(0)
                func.calls.append(FunctionCall(func, stanza["payload"]["args"]))
                self._delegate.callItemDidChange_(func)
            return True
        return False

    def getTargetModuleByModule_(self, module):
        if module is None:
            return None
        tm = self._targetModuleByAddress.get(module.address, None)
        if tm is None:
            tm = TargetModule(module)
            self.targetModules.append(tm)
            self._targetModuleByAddress[module.address] = tm
        return tm

    def outlineView_numberOfChildrenOfItem_(self, outlineView, item):
        if item is None:
            return len(self.targetModules)
        elif isinstance(item, TargetModule):
            return len(item.functions)
        elif isinstance(item, TargetFunction):
            return len(item.calls)
        else:
            return 0

    def outlineView_isItemExpandable_(self, outlineView, item):
        if item is None:
            return False
        elif isinstance(item, TargetModule):
            return len(item.functions) > 0
        elif isinstance(item, TargetFunction):
            return len(item.calls) > 0
        else:
            return False

    def outlineView_child_ofItem_(self, outlineView, index, item):
        if item is None:
            return self.targetModules[index]
        elif isinstance(item, TargetModule):
            return item.functions[index]
        elif isinstance(item, TargetFunction):
            return item.calls[index]
        else:
            return None

    def outlineView_objectValueForTableColumn_byItem_(self, outlineView, tableColumn, item):
        identifier = tableColumn.identifier()
        if isinstance(item, TargetModule):
            if identifier == "name":
                return item.module.name
            elif identifier == "total":
                return item.total
            else:
                return False
        elif isinstance(item, TargetFunction):
            if identifier == "name":
                return item.name
            elif identifier == "total":
                return item.total
            else:
                return item.hasProbe
        else:
            if identifier == "name":
                return item.summary
            elif identifier == "total":
                return ""
            else:
                return False


class TargetModule(NSObject):
    def __new__(cls, module):
        return cls.alloc().initWithModule_(module)

    def initWithModule_(self, module):
        self = self.init()
        self.module = module
        self.functions = []
        self._functionByAddress = {}
        self.total = 0
        return self

    def getTargetFunctionByAddress_(self, address):
        f = self._functionByAddress.get(address, None)
        if f is None:
            f = TargetFunction(self, address - self.module.address)
            self.functions.append(f)
            self._functionByAddress[address] = f
        return f


class TargetFunction(NSObject):
    def __new__(cls, module, offset):
        return cls.alloc().initWithModule_offset_(module, offset)

    def initWithModule_offset_(self, targetModule, offset):
        self = self.init()
        self.name = "sub_%x" % offset
        self.module = targetModule
        self.address = targetModule.module.address + offset
        self.offset = offset
        self.total = 0
        self.hasProbe = False
        self.calls = []
        return self


class FunctionCall(NSObject):
    def __new__(cls, func, args):
        return cls.alloc().initWithFunction_args_(func, args)

    def initWithFunction_args_(self, func, args):
        self = self.init()
        self.func = func
        self.args = args
        self.summary = f"{func.name}({', '.join(args)})"
        return self


SCRIPT_TEMPLATE = """
var probes = Object.create(null);

var initialize = function initialize() {
    Stalker.trustThreshold = 2000;
    Stalker.queueCapacity = 1000000;
    Stalker.queueDrainInterval = 250;

    sendModules(function () {
        interceptReadFunction('recv');
        interceptReadFunction('read$UNIX2003');
        interceptReadFunction('readv$UNIX2003');
    });

    recv(onStanza);
};

var onStanza = function onStanza(stanza) {
    if (stanza.to === "/stalker/probes") {
        var address = stanza.payload.address,
            probeId;
        switch (stanza.name) {
            case '+add':
                if (probes[address] === undefined) {
                    var probeAddress = "/stalker/probes/" + address + "/calls";
                    probeId = Stalker.addCallProbe(ptr(address), function probe(args) {
                        var data = [
                            "0x" + args[0].toString(16),
                            "0x" + args[1].toString(16),
                            "0x" + args[2].toString(16),
                            "0x" + args[3].toString(16)
                        ];
                        send({ from: probeAddress, name: '+add', payload: { args: data } });
                    });
                    probes[address] = probeId;
                }
                break;
            case '+remove':
                probeId = probes[address];
                if (probeId !== undefined) {
                    Stalker.removeCallProbe(probeId);
                    delete probes[address];
                }
                break;
        }
    }

    recv(onStanza);
};

var sendModules = function sendModules(callback) {
    var modules = [];
    Process.enumerateModules({
        onMatch: function onMatch(module) {
            modules.push(module);
        },
        onComplete: function onComplete() {
            send({ name: '+sync', from: "/process/modules", payload: { items: modules } });
            callback();
        }
    });
};

var stalkedThreadId = null;
var interceptReadFunction = function interceptReadFunction(functionName) {
    Interceptor.attach(Module.getExportByName('libSystem.B.dylib', functionName), {
        onEnter: function(args) {
            this.fd = args[0].toInt32();
        },
        onLeave: function (retval) {
            var fd = this.fd;
            if (Socket.type(fd) === 'tcp') {
                var address = Socket.peerAddress(fd);
                if (address !== null && address.port === %(trigger_port)d) {
                    send({ name: '+add', from: "/interceptor/functions", payload: { items: [{ name: functionName }] } });
                    if (stalkedThreadId === null) {
                        stalkedThreadId = Process.getCurrentThreadId();
                        Stalker.follow(stalkedThreadId, {
                            events: {
                                call: true
                            },
                            onCallSummary: function onCallSummary(summary) {
                                send({ name: '+add', from: "/stalker/calls", payload: { summary: summary } });
                            }
                        });
                    }
                }
            }
        }
    });
}

setTimeout(initialize, 0);
"""
