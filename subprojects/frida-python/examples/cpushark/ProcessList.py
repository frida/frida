from Foundation import NSNotFound, NSObject


class ProcessList(NSObject):
    def __new__(cls, device):
        return cls.alloc().initWithDevice_(device)

    def initWithDevice_(self, device):
        self = self.init()
        self.processes = sorted(device.enumerate_processes(), key=lambda d: d.name.lower())
        self._processNames = []
        self._processIndexByName = {}
        for i, process in enumerate(self.processes):
            lowerName = process.name.lower()
            self._processNames.append(lowerName)
            self._processIndexByName[lowerName] = i
        return self

    def numberOfItemsInComboBox_(self, comboBox):
        return len(self.processes)

    def comboBox_objectValueForItemAtIndex_(self, comboBox, index):
        return self.processes[index].name

    def comboBox_completedString_(self, comboBox, uncompletedString):
        lowerName = uncompletedString.lower()
        for i, name in enumerate(self._processNames):
            if name.startswith(lowerName):
                return self.processes[i].name
        return None

    def comboBox_indexOfItemWithStringValue_(self, comboBox, value):
        return self._processIndexByName.get(value.lower(), NSNotFound)
