from Cocoa import NSApp
from Foundation import NSObject
from MainWindowController import MainWindowController


class AppDelegate(NSObject):
    def applicationDidFinishLaunching_(self, notification):
        window = MainWindowController()
        window.showWindow_(window)
        NSApp.activateIgnoringOtherApps_(True)

    def applicationShouldTerminateAfterLastWindowClosed_(self, sender):
        return True
