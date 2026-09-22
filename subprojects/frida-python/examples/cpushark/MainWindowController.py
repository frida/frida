from Capture import Capture, CaptureState, TargetFunction
from Cocoa import NSRunCriticalAlertPanel, NSUserDefaults, NSWindowController, objc
from ProcessList import ProcessList

import frida


class MainWindowController(NSWindowController):
    processCombo = objc.IBOutlet()
    triggerField = objc.IBOutlet()
    attachProgress = objc.IBOutlet()
    attachButton = objc.IBOutlet()
    detachButton = objc.IBOutlet()
    recvTotalLabel = objc.IBOutlet()
    callTableView = objc.IBOutlet()

    def __new__(cls):
        return cls.alloc().initWithTitle_("CpuShark")

    def initWithTitle_(self, title):
        self = self.initWithWindowNibName_("MainWindow")
        self.window().setTitle_(title)

        self.retain()

        return self

    def windowDidLoad(self):
        NSWindowController.windowDidLoad(self)

        device = [device for device in frida.get_device_manager().enumerate_devices() if device.type == "local"][0]
        self.processList = ProcessList(device)
        self.capture = Capture(device)
        self.processCombo.setUsesDataSource_(True)
        self.processCombo.setDataSource_(self.processList)
        self.capture.setDelegate_(self)

        self.callTableView.setDataSource_(self.capture.calls)
        self.capture.calls.setDelegate_(self)

        self.loadDefaults()

        self.updateAttachForm_(self)

    def windowWillClose_(self, notification):
        self.saveDefaults()

        self.autorelease()

    def loadDefaults(self):
        defaults = NSUserDefaults.standardUserDefaults()
        targetProcess = defaults.stringForKey_("targetProcess")
        if targetProcess is not None:
            for i, process in enumerate(self.processList.processes):
                if process.name == targetProcess:
                    self.processCombo.selectItemAtIndex_(i)
                    break
        triggerPort = defaults.integerForKey_("triggerPort") or 80
        self.triggerField.setStringValue_(str(triggerPort))

    def saveDefaults(self):
        defaults = NSUserDefaults.standardUserDefaults()
        process = self.selectedProcess()
        if process is not None:
            defaults.setObject_forKey_(process.name, "targetProcess")
        defaults.setInteger_forKey_(self.triggerField.integerValue(), "triggerPort")

    def selectedProcess(self):
        index = self.processCombo.indexOfSelectedItem()
        if index != -1:
            return self.processList.processes[index]
        return None

    def triggerPort(self):
        return self.triggerField.integerValue()

    @objc.IBAction
    def attach_(self, sender):
        self.capture.attachToProcess_triggerPort_(self.selectedProcess(), self.triggerPort())

    @objc.IBAction
    def detach_(self, sender):
        self.capture.detach()

    @objc.IBAction
    def toggleTracing_(self, sender):
        item = sender.itemAtRow_(sender.selectedRow())
        if isinstance(item, TargetFunction):
            func = item
            if func.hasProbe:
                self.capture.calls.removeProbe_(func)
            else:
                self.capture.calls.addProbe_(func)
            func.hasProbe = not func.hasProbe
            self.callTableView.reloadItem_(func)

    def updateAttachForm_(self, sender):
        isDetached = self.capture.state == CaptureState.DETACHED
        hasProcess = self.selectedProcess() is not None
        hasTrigger = len(self.triggerField.stringValue()) > 0
        self.processCombo.setEnabled_(isDetached)
        self.triggerField.setEnabled_(isDetached)
        self.attachProgress.setHidden_(self.capture.state != CaptureState.ATTACHING)
        self.attachButton.setHidden_(self.capture.state == CaptureState.ATTACHED)
        self.attachButton.setEnabled_(isDetached and hasProcess and hasTrigger)
        self.detachButton.setHidden_(self.capture.state != CaptureState.ATTACHED)
        if self.capture.state == CaptureState.ATTACHING:
            self.attachProgress.startAnimation_(self)
        else:
            self.attachProgress.stopAnimation_(self)

    def controlTextDidChange_(self, notification):
        self.updateAttachForm_(self)

    def comboBoxSelectionDidChange_(self, notification):
        self.updateAttachForm_(self)

    def captureStateDidChange(self):
        self.updateAttachForm_(self)

    def captureFailedToAttachWithError_(self, error):
        NSRunCriticalAlertPanel("Error", "Failed to attach: %s" % error, None, None, None)

    def captureRecvTotalDidChange(self):
        self.recvTotalLabel.setStringValue_(self.capture.recvTotal)

    def callsDidChange(self):
        self.callTableView.reloadData()

    def callItemDidChange_(self, item):
        self.callTableView.reloadItem_reloadChildren_(item, True)
