// fffffff0080863e8    int64_t IOInterruptController::registerInterrupt
Interceptor.breakpointKind = 'hard';
const listener = Interceptor.attach(ptr('0xfffffff0080863e8'), function (args) {
  const handler = unpac(args[4]);
  console.log(`IOInterruptController::registerInterrupt() called with handler=${handler} from ${this.returnAddress}`);
  //listener.detach();
});
$gdb.continue();

//const listener = Interceptor.attach(ptr('0xfffffff0091cb4cc'), function (args) {
//  const aic = args[0];
//  const service = args[1];
//  const arg3 = args[2];
//  console.log('AIC is at:', aic);
//  console.log('Service is at:', service);
//  console.log('arg3:', arg3);
//  Object.assign(globalThis, { aic, service });
//  listener.detach();
//});
//$gdb.continue();

//Interceptor.attach(ptr('0xfffffff007a55728'), function () {
//  console.log('Stopped on thread about to call thread_block()');
//  try {
//    lookup();
//  } catch (e) {
//    console.log('Oops:', e);
//  }
//});
//$gdb.continue();

const IO_SERVICE_VTABLE_LENGTH = 168;

function lookup() {
  const ioService_getPlatform = new NativeFunction(ptr('0xfffffff00801ed48'), 'pointer', []);
  const osSymbol_withCStringNoCopy = new NativeFunction(ptr('0xfffffff007fc45dc'), 'pointer', ['pointer']);

  const platformExpert = ioService_getPlatform();
  console.log('Got IOPlatformExpert:', platformExpert);
  const platformExpertVtable = vtableOf(platformExpert);
  console.log('Got IOPlatformExpertVtable:', platformExpertVtable);

  const appleInterruptControllerCString = ptr('0xfffffff00754d855'); // AppleInterruptController\0
  const appleInterruptControllerSym = osSymbol_withCStringNoCopy(appleInterruptControllerCString);
  console.log('Got appleInterruptControllerSym:', appleInterruptControllerSym);

  const lookupInterruptControllerImpl = concreteIoServiceImplFor(25, platformExpertVtable);
  console.log('Got lookupInterruptControllerImpl:', lookupInterruptControllerImpl);
  const lookupInterruptController = new NativeFunction(lookupInterruptControllerImpl, 'pointer', ['pointer', 'pointer']);

  const controller = lookupInterruptController(platformExpert, appleInterruptControllerSym);
  console.log('lookupInterruptController() returned:', controller);
}

function registerInterruptHandler() {
  const aicVtable = vtableOf(aic);

  const registerInterruptImpl = concreteIoServiceImplFor(0, aicVtable);
  console.log('Got registerInterruptImpl:', registerInterruptImpl);

  const enableInterruptImpl = concreteIoServiceImplFor(3, aicVtable);
  console.log('Got enableInterruptImpl:', enableInterruptImpl);
}

function vtableOf(instance) {
  return unpac(instance.readPointer());
}

function concreteIoServiceImplFor(n, vtable) {
  return unpac(vtable.add(concreteIoServiceVtableOffsetFor(n)).readPointer());
}

function concreteIoServiceVtableOffsetFor(n) {
  return (IO_SERVICE_VTABLE_LENGTH + n) * Process.pointerSize;
}

const UNPAC_MASK = ptr('0xfffffff000000000');

function unpac(p) {
  return p.or(UNPAC_MASK);
}

function toMatchPattern(p) {
  const cleanHex = p.toString().slice(2);
  const bytePairs = cleanHex.match(/.{1,2}/g);
  const reversed = bytePairs.reverse();
  return reversed.join(' ');
}

const RANGES = [
  {
    "name": "com.apple.iokit.IONetworkingFamily.0.__TEXT_EXEC.__text",
    "size": 112560,
    "vsize": 112560,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1124d48",
    "vaddr": "0xfffffff008128d48"
  },
  {
    "name": "com.apple.iokit.IOTimeSyncFamily.0.__TEXT_EXEC.__text",
    "size": 95320,
    "vsize": 95320,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x11405c8",
    "vaddr": "0xfffffff0081445c8"
  },
  {
    "name": "com.apple.iokit.IOPCIFamily.0.__TEXT_EXEC.__text",
    "size": 90624,
    "vsize": 90624,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1157af0",
    "vaddr": "0xfffffff00815baf0"
  },
  {
    "name": "com.apple.driver.AppleConvergedIPC.0.__TEXT_EXEC.__text",
    "size": 503656,
    "vsize": 503656,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x116ddc0",
    "vaddr": "0xfffffff008171dc0"
  },
  {
    "name": "com.apple.driver.mDNSOffloadUserClient-Embedded.0.__TEXT_EXEC.__text",
    "size": 10488,
    "vsize": 10488,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x11e8df8",
    "vaddr": "0xfffffff0081ecdf8"
  },
  {
    "name": "com.apple.iokit.IOSkywalkFamily.0.__TEXT_EXEC.__text",
    "size": 194016,
    "vsize": 194016,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x11eb7c0",
    "vaddr": "0xfffffff0081ef7c0"
  },
  {
    "name": "com.apple.driver.AppleIPAppender.0.__TEXT_EXEC.__text",
    "size": 19976,
    "vsize": 19976,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x121ae70",
    "vaddr": "0xfffffff00821ee70"
  },
  {
    "name": "com.apple.driver.AppleConvergedIPCBaseband.0.__TEXT_EXEC.__text",
    "size": 163872,
    "vsize": 163872,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x121fd48",
    "vaddr": "0xfffffff008223d48"
  },
  {
    "name": "com.apple.iokit.IOSlowAdaptiveClockingFamily.0.__TEXT_EXEC.__text",
    "size": 7584,
    "vsize": 7584,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1247e38",
    "vaddr": "0xfffffff00824be38"
  },
  {
    "name": "com.company.driver.modulename.0.__TEXT_EXEC.__text",
    "size": 97176,
    "vsize": 97176,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1249ca8",
    "vaddr": "0xfffffff00824dca8"
  },
  {
    "name": "com.apple.iokit.IOReporting.0.__TEXT_EXEC.__text",
    "size": 11224,
    "vsize": 11224,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1261910",
    "vaddr": "0xfffffff008265910"
  },
  {
    "name": "com.apple.driver.AppleARMPlatform.0.__TEXT_EXEC.__text",
    "size": 227480,
    "vsize": 227480,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x12645b8",
    "vaddr": "0xfffffff0082685b8"
  },
  {
    "name": "com.apple.driver.AppleSamsungSPI.0.__TEXT_EXEC.__text",
    "size": 22368,
    "vsize": 22368,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x129bf20",
    "vaddr": "0xfffffff00829ff20"
  },
  {
    "name": "com.apple.kec.corecrypto.0.__TEXT_EXEC.__text",
    "size": 288624,
    "vsize": 288624,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x12a1750",
    "vaddr": "0xfffffff0082a5750"
  },
  {
    "name": "com.apple.kext.CoreTrust.0.__TEXT_EXEC.__text",
    "size": 17088,
    "vsize": 17088,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x12e7f90",
    "vaddr": "0xfffffff0082ebf90"
  },
  {
    "name": "com.apple.driver.AppleMobileFileIntegrity.0.__TEXT_EXEC.__text",
    "size": 35216,
    "vsize": 35216,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x12ec320",
    "vaddr": "0xfffffff0082f0320"
  },
  {
    "name": "com.apple.iokit.IOHIDFamily.0.__TEXT_EXEC.__text",
    "size": 334744,
    "vsize": 334744,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x12f4d80",
    "vaddr": "0xfffffff0082f8d80"
  },
  {
    "name": "com.apple.driver.IOSlaveProcessor.0.__TEXT_EXEC.__text",
    "size": 4736,
    "vsize": 4736,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13469e8",
    "vaddr": "0xfffffff00834a9e8"
  },
  {
    "name": "com.apple.driver.AppleA7IOP.0.__TEXT_EXEC.__text",
    "size": 40776,
    "vsize": 40776,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1347d38",
    "vaddr": "0xfffffff00834bd38"
  },
  {
    "name": "com.apple.driver.RTBuddy.0.__TEXT_EXEC.__text",
    "size": 139568,
    "vsize": 139568,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1351d50",
    "vaddr": "0xfffffff008355d50"
  },
  {
    "name": "com.apple.iokit.IOCryptoAcceleratorFamily.0.__TEXT_EXEC.__text",
    "size": 27048,
    "vsize": 27048,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1373f50",
    "vaddr": "0xfffffff008377f50"
  },
  {
    "name": "com.apple.security.AppleImage4.0.__TEXT_EXEC.__text",
    "size": 57184,
    "vsize": 57184,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x137a9c8",
    "vaddr": "0xfffffff00837e9c8"
  },
  {
    "name": "com.apple.driver.AppleFirmwareUpdateKext.0.__TEXT_EXEC.__text",
    "size": 7608,
    "vsize": 7608,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13889f8",
    "vaddr": "0xfffffff00838c9f8"
  },
  {
    "name": "com.apple.drivers.AppleSPU.0.__TEXT_EXEC.__text",
    "size": 222600,
    "vsize": 222600,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x138a880",
    "vaddr": "0xfffffff00838e880"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedLightSensor.0.__TEXT_EXEC.__text",
    "size": 98936,
    "vsize": 98936,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13c0ed8",
    "vaddr": "0xfffffff0083c4ed8"
  },
  {
    "name": "com.apple.driver.AppleS5L8920XPWM.0.__TEXT_EXEC.__text",
    "size": 6824,
    "vsize": 6824,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13d9220",
    "vaddr": "0xfffffff0083dd220"
  },
  {
    "name": "com.apple.driver.AppleBluetoothDebugService.0.__TEXT_EXEC.__text",
    "size": 368,
    "vsize": 368,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13dad98",
    "vaddr": "0xfffffff0083ded98"
  },
  {
    "name": "com.apple.driver.corecapture.0.__TEXT_EXEC.__text",
    "size": 106512,
    "vsize": 106512,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13dafd8",
    "vaddr": "0xfffffff0083defd8"
  },
  {
    "name": "com.apple.driver.AppleBluetoothDebug.0.__TEXT_EXEC.__text",
    "size": 43992,
    "vsize": 43992,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13f50b8",
    "vaddr": "0xfffffff0083f90b8"
  },
  {
    "name": "com.apple.iokit.CoreAnalytics.0.__TEXT_EXEC.__text",
    "size": 20728,
    "vsize": 20728,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x13ffd60",
    "vaddr": "0xfffffff008403d60"
  },
  {
    "name": "com.apple.driver.AppleInputDeviceSupport.0.__TEXT_EXEC.__text",
    "size": 64072,
    "vsize": 64072,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1404f28",
    "vaddr": "0xfffffff008408f28"
  },
  {
    "name": "com.apple.iokit.IOSerialFamily.0.__TEXT_EXEC.__text",
    "size": 27272,
    "vsize": 27272,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1414a40",
    "vaddr": "0xfffffff008418a40"
  },
  {
    "name": "com.apple.driver.AppleOnboardSerial.0.__TEXT_EXEC.__text",
    "size": 70824,
    "vsize": 70824,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x141b598",
    "vaddr": "0xfffffff00841f598"
  },
  {
    "name": "com.apple.iokit.IOAccessoryManager.0.__TEXT_EXEC.__text",
    "size": 484928,
    "vsize": 484928,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x142cb10",
    "vaddr": "0xfffffff008430b10"
  },
  {
    "name": "com.apple.driver.AppleARMPMU.0.__TEXT_EXEC.__text",
    "size": 93288,
    "vsize": 93288,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x14a3220",
    "vaddr": "0xfffffff0084a7220"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedTempSensor.0.__TEXT_EXEC.__text",
    "size": 93416,
    "vsize": 93416,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x14b9f58",
    "vaddr": "0xfffffff0084bdf58"
  },
  {
    "name": "com.apple.AppleSMC_Embedded.0.__TEXT_EXEC.__text",
    "size": 114144,
    "vsize": 114144,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x14d0d10",
    "vaddr": "0xfffffff0084d4d10"
  },
  {
    "name": "com.apple.driver.AppleHIDTransport.0.__TEXT_EXEC.__text",
    "size": 263840,
    "vsize": 263840,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x14ecbc0",
    "vaddr": "0xfffffff0084f0bc0"
  },
  {
    "name": "com.apple.driver.AppleHIDTransportSPI.0.__TEXT_EXEC.__text",
    "size": 250808,
    "vsize": 250808,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x152d330",
    "vaddr": "0xfffffff008531330"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedAudioLibs.0.__TEXT_EXEC.__text",
    "size": 40992,
    "vsize": 40992,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x156a7b8",
    "vaddr": "0xfffffff00856e7b8"
  },
  {
    "name": "com.apple.iokit.IOAudio2Family.0.__TEXT_EXEC.__text",
    "size": 19896,
    "vsize": 19896,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15748a8",
    "vaddr": "0xfffffff0085788a8"
  },
  {
    "name": "com.apple.iokit.AppleARMIISAudio.0.__TEXT_EXEC.__text",
    "size": 87136,
    "vsize": 87136,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1579730",
    "vaddr": "0xfffffff00857d730"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedAudio.0.__TEXT_EXEC.__text",
    "size": 198696,
    "vsize": 198696,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x158ec60",
    "vaddr": "0xfffffff008592c60"
  },
  {
    "name": "com.apple.driver.AppleAOPAudio.0.__TEXT_EXEC.__text",
    "size": 20584,
    "vsize": 20584,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15bf558",
    "vaddr": "0xfffffff0085c3558"
  },
  {
    "name": "com.apple.driver.AppleSART.0.__TEXT_EXEC.__text",
    "size": 7824,
    "vsize": 7824,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15c4690",
    "vaddr": "0xfffffff0085c8690"
  },
  {
    "name": "com.apple.driver.AppleProxDriver.0.__TEXT_EXEC.__text",
    "size": 9168,
    "vsize": 9168,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15c65f0",
    "vaddr": "0xfffffff0085ca5f0"
  },
  {
    "name": "com.apple.driver.AppleSmartIO.0.__TEXT_EXEC.__text",
    "size": 40744,
    "vsize": 40744,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15c8a90",
    "vaddr": "0xfffffff0085cca90"
  },
  {
    "name": "com.apple.driver.AppleMultitouchSPI.0.__TEXT_EXEC.__text",
    "size": 121016,
    "vsize": 121016,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15d2a88",
    "vaddr": "0xfffffff0085d6a88"
  },
  {
    "name": "com.apple.driver.AppleUSBHostMergeProperties.0.__TEXT_EXEC.__text",
    "size": 2384,
    "vsize": 2384,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15f0410",
    "vaddr": "0xfffffff0085f4410"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBCommon.0.__TEXT_EXEC.__text",
    "size": 17992,
    "vsize": 17992,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15f0e30",
    "vaddr": "0xfffffff0085f4e30"
  },
  {
    "name": "com.apple.iokit.IOUSBHostFamily.0.__TEXT_EXEC.__text",
    "size": 642192,
    "vsize": 642192,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x15f5548",
    "vaddr": "0xfffffff0085f9548"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBHostPacketFilter.0.__TEXT_EXEC.__text",
    "size": 6944,
    "vsize": 6944,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16922a8",
    "vaddr": "0xfffffff0086962a8"
  },
  {
    "name": "com.apple.driver.ProvInfoIOKit.0.__TEXT_EXEC.__text",
    "size": 41656,
    "vsize": 41656,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1693e98",
    "vaddr": "0xfffffff008697e98"
  },
  {
    "name": "com.apple.AppleARM64ErrorHandler.0.__TEXT_EXEC.__text",
    "size": 3152,
    "vsize": 3152,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x169e220",
    "vaddr": "0xfffffff0086a2220"
  },
  {
    "name": "com.apple.driver.DiskImages.0.__TEXT_EXEC.__text",
    "size": 34608,
    "vsize": 34608,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x169ef40",
    "vaddr": "0xfffffff0086a2f40"
  },
  {
    "name": "com.apple.driver.DiskImages.KernelBacked.0.__TEXT_EXEC.__text",
    "size": 14496,
    "vsize": 14496,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16a7740",
    "vaddr": "0xfffffff0086ab740"
  },
  {
    "name": "com.apple.driver.DiskImages.RAMBackingStore.0.__TEXT_EXEC.__text",
    "size": 2480,
    "vsize": 2480,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16ab0b0",
    "vaddr": "0xfffffff0086af0b0"
  },
  {
    "name": "com.apple.iokit.IOSurface.0.__TEXT_EXEC.__text",
    "size": 103320,
    "vsize": 103320,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16abb30",
    "vaddr": "0xfffffff0086afb30"
  },
  {
    "name": "com.apple.driver.AppleJPEGDriver.0.__TEXT_EXEC.__text",
    "size": 65624,
    "vsize": 65624,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16c4f98",
    "vaddr": "0xfffffff0086c8f98"
  },
  {
    "name": "com.apple.driver.IODARTFamily.0.__TEXT_EXEC.__text",
    "size": 71528,
    "vsize": 71528,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16d50c0",
    "vaddr": "0xfffffff0086d90c0"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedPCIE.0.__TEXT_EXEC.__text",
    "size": 60616,
    "vsize": 60616,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16e68f8",
    "vaddr": "0xfffffff0086ea8f8"
  },
  {
    "name": "com.apple.driver.AppleMultiFunctionManager.0.__TEXT_EXEC.__text",
    "size": 29848,
    "vsize": 29848,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16f5690",
    "vaddr": "0xfffffff0086f9690"
  },
  {
    "name": "com.apple.driver.AppleDAPF.0.__TEXT_EXEC.__text",
    "size": 3024,
    "vsize": 3024,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16fcbf8",
    "vaddr": "0xfffffff008700bf8"
  },
  {
    "name": "com.apple.driver.AppleCSEmbeddedAudio.0.__TEXT_EXEC.__text",
    "size": 38864,
    "vsize": 38864,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x16fd898",
    "vaddr": "0xfffffff008701898"
  },
  {
    "name": "com.apple.iokit.IOMikeyBusFamily.0.__TEXT_EXEC.__text",
    "size": 126216,
    "vsize": 126216,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1707138",
    "vaddr": "0xfffffff00870b138"
  },
  {
    "name": "com.apple.driver.AppleTriStar.0.__TEXT_EXEC.__text",
    "size": 104592,
    "vsize": 104592,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1725f10",
    "vaddr": "0xfffffff008729f10"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedMikeyBus.0.__TEXT_EXEC.__text",
    "size": 101136,
    "vsize": 101136,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x173f870",
    "vaddr": "0xfffffff008743870"
  },
  {
    "name": "com.apple.driver.AppleMikeyBusAudio.0.__TEXT_EXEC.__text",
    "size": 109296,
    "vsize": 109296,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1758450",
    "vaddr": "0xfffffff00875c450"
  },
  {
    "name": "com.apple.ApplePMGR.0.__TEXT_EXEC.__text",
    "size": 180104,
    "vsize": 180104,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1773010",
    "vaddr": "0xfffffff008777010"
  },
  {
    "name": "com.apple.ApplePMGR.0.__TEXT_EXEC.__text_1",
    "size": 4456,
    "vsize": 4456,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x179f068",
    "vaddr": "0xfffffff0087a3068"
  },
  {
    "name": "com.apple.driver.AppleMultitouchDriver.0.__TEXT_EXEC.__text",
    "size": 97360,
    "vsize": 97360,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x17a02a0",
    "vaddr": "0xfffffff0087a42a0"
  },
  {
    "name": "com.apple.driver.AppleGPIOICController.0.__TEXT_EXEC.__text",
    "size": 35744,
    "vsize": 35744,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x17b7fc0",
    "vaddr": "0xfffffff0087bbfc0"
  },
  {
    "name": "com.apple.driver.AppleS5L8940XI2C.0.__TEXT_EXEC.__text",
    "size": 10968,
    "vsize": 10968,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x17c0c30",
    "vaddr": "0xfffffff0087c4c30"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedUSB.0.__TEXT_EXEC.__text",
    "size": 32248,
    "vsize": 32248,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x17c37d8",
    "vaddr": "0xfffffff0087c77d8"
  },
  {
    "name": "com.apple.Libm.kext.0.__TEXT_EXEC.__text",
    "size": 9072,
    "vsize": 9072,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x17cb6a0",
    "vaddr": "0xfffffff0087cf6a0"
  },
  {
    "name": "com.apple.AppleT8030PPM.0.__TEXT_EXEC.__text",
    "size": 250896,
    "vsize": 250896,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x17cdae0",
    "vaddr": "0xfffffff0087d1ae0"
  },
  {
    "name": "com.apple.driver.AppleM2ScalerCSC.0.__TEXT_EXEC.__text",
    "size": 291552,
    "vsize": 291552,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x180afc0",
    "vaddr": "0xfffffff00880efc0"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBXHCI.0.__TEXT_EXEC.__text",
    "size": 371496,
    "vsize": 371496,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1852370",
    "vaddr": "0xfffffff008856370"
  },
  {
    "name": "com.apple.driver.AppleTypeCPhy.0.__TEXT_EXEC.__text",
    "size": 38152,
    "vsize": 38152,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x18acf68",
    "vaddr": "0xfffffff0088b0f68"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBHostCompositeDevice.0.__TEXT_EXEC.__text",
    "size": 11688,
    "vsize": 11688,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x18b6540",
    "vaddr": "0xfffffff0088ba540"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBHub.0.__TEXT_EXEC.__text",
    "size": 201880,
    "vsize": 201880,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x18b93b8",
    "vaddr": "0xfffffff0088bd3b8"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedUSBHost.0.__TEXT_EXEC.__text",
    "size": 9160,
    "vsize": 9160,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x18ea920",
    "vaddr": "0xfffffff0088ee920"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBXHCIARM.0.__TEXT_EXEC.__text",
    "size": 89320,
    "vsize": 89320,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x18ecdb8",
    "vaddr": "0xfffffff0088f0db8"
  },
  {
    "name": "com.apple.driver.usb.IOUSBHostHIDDevice.0.__TEXT_EXEC.__text",
    "size": 42880,
    "vsize": 42880,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1902b70",
    "vaddr": "0xfffffff008906b70"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBHostT8030.0.__TEXT_EXEC.__text",
    "size": 58064,
    "vsize": 58064,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x190d3c0",
    "vaddr": "0xfffffff0089113c0"
  },
  {
    "name": "com.apple.driver.usb.networking.0.__TEXT_EXEC.__text",
    "size": 6352,
    "vsize": 6352,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x191b760",
    "vaddr": "0xfffffff00891f760"
  },
  {
    "name": "com.apple.driver.usb.cdc.0.__TEXT_EXEC.__text",
    "size": 8568,
    "vsize": 8568,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x191d100",
    "vaddr": "0xfffffff008921100"
  },
  {
    "name": "com.apple.driver.usb.cdc.ncm.0.__TEXT_EXEC.__text",
    "size": 36456,
    "vsize": 36456,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x191f348",
    "vaddr": "0xfffffff008923348"
  },
  {
    "name": "com.apple.driver.AppleH11ANEInterface.0.__TEXT_EXEC.__text",
    "size": 285048,
    "vsize": 285048,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1928280",
    "vaddr": "0xfffffff00892c280"
  },
  {
    "name": "com.apple.iokit.IOUSBDeviceFamily.0.__TEXT_EXEC.__text",
    "size": 181472,
    "vsize": 181472,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x196dcc8",
    "vaddr": "0xfffffff008971cc8"
  },
  {
    "name": "com.apple.driver.AppleUSBEthernetDevice.0.__TEXT_EXEC.__text",
    "size": 15448,
    "vsize": 15448,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x199a278",
    "vaddr": "0xfffffff00899e278"
  },
  {
    "name": "com.apple.iokit.IO80211Family.0.__TEXT_EXEC.__text",
    "size": 983808,
    "vsize": 983808,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x199dfa0",
    "vaddr": "0xfffffff0089a1fa0"
  },
  {
    "name": "com.apple.plugin.IOgPTPPlugin.0.__TEXT_EXEC.__text",
    "size": 373576,
    "vsize": 373576,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1a8e370",
    "vaddr": "0xfffffff008a92370"
  },
  {
    "name": "com.apple.driver.AppleT8030CLPC.0.__TEXT_EXEC.__text",
    "size": 170136,
    "vsize": 170136,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1ae9788",
    "vaddr": "0xfffffff008aed788"
  },
  {
    "name": "com.apple.driver.AppleOrion.0.__TEXT_EXEC.__text",
    "size": 75928,
    "vsize": 75928,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1b130f0",
    "vaddr": "0xfffffff008b170f0"
  },
  {
    "name": "com.apple.ApplePMGR.0.__TEXT_EXEC.__text_2",
    "size": 65616,
    "vsize": 65616,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1b25a58",
    "vaddr": "0xfffffff008b29a58"
  },
  {
    "name": "com.apple.driver.AppleSEPManager.0.__TEXT_EXEC.__text",
    "size": 203936,
    "vsize": 203936,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1b35b78",
    "vaddr": "0xfffffff008b39b78"
  },
  {
    "name": "com.apple.driver.AppleSSE.0.__TEXT_EXEC.__text",
    "size": 27160,
    "vsize": 27160,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1b678e8",
    "vaddr": "0xfffffff008b6b8e8"
  },
  {
    "name": "com.apple.ASIOKit.0.__TEXT_EXEC.__text",
    "size": 234976,
    "vsize": 234976,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1b6e3d0",
    "vaddr": "0xfffffff008b723d0"
  },
  {
    "name": "com.apple.driver.AppleConvergedIPC.0.__TEXT_EXEC.__text_1",
    "size": 242184,
    "vsize": 242184,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1ba7a80",
    "vaddr": "0xfffffff008baba80"
  },
  {
    "name": "com.apple.AppleS8000DWI.0.__TEXT_EXEC.__text",
    "size": 4656,
    "vsize": 4656,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1be2d58",
    "vaddr": "0xfffffff008be6d58"
  },
  {
    "name": "com.apple.driver.AppleT8027TypeCPhy.0.__TEXT_EXEC.__text",
    "size": 30744,
    "vsize": 30744,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1be4058",
    "vaddr": "0xfffffff008be8058"
  },
  {
    "name": "com.apple.driver.AppleT8030TypeCPhy.0.__TEXT_EXEC.__text",
    "size": 360,
    "vsize": 360,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1beb940",
    "vaddr": "0xfffffff008bef940"
  },
  {
    "name": "com.apple.AppleHapticsSupportLEAP.0.__TEXT_EXEC.__text",
    "size": 100480,
    "vsize": 100480,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1bebb78",
    "vaddr": "0xfffffff008befb78"
  },
  {
    "name": "com.apple.driver.AppleT8030PCIe.0.__TEXT_EXEC.__text",
    "size": 20200,
    "vsize": 20200,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1c044c8",
    "vaddr": "0xfffffff008c084c8"
  },
  {
    "name": "com.apple.driver.AppleC26Charger.0.__TEXT_EXEC.__text",
    "size": 70952,
    "vsize": 70952,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1c09480",
    "vaddr": "0xfffffff008c0d480"
  },
  {
    "name": "com.apple.driver.AppleAuthCP.0.__TEXT_EXEC.__text",
    "size": 62984,
    "vsize": 62984,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1c1aa78",
    "vaddr": "0xfffffff008c1ea78"
  },
  {
    "name": "com.apple.driver.AppleSmartBatteryManagerEmbedded.0.__TEXT_EXEC.__text",
    "size": 87416,
    "vsize": 87416,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1c2a150",
    "vaddr": "0xfffffff008c2e150"
  },
  {
    "name": "com.apple.iokit.IOHDCPFamily.0.__TEXT_EXEC.__text",
    "size": 46872,
    "vsize": 46872,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1c3f798",
    "vaddr": "0xfffffff008c43798"
  },
  {
    "name": "com.apple.IOCECFamily.0.__TEXT_EXEC.__text",
    "size": 8984,
    "vsize": 8984,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1c4af80",
    "vaddr": "0xfffffff008c4ef80"
  },
  {
    "name": "com.apple.iokit.IOAVFamily.0.__TEXT_EXEC.__text",
    "size": 431784,
    "vsize": 431784,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1c4d368",
    "vaddr": "0xfffffff008c51368"
  },
  {
    "name": "com.apple.iokit.IOUserEthernet.0.__TEXT_EXEC.__text",
    "size": 14576,
    "vsize": 14576,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1cb6ae0",
    "vaddr": "0xfffffff008cbaae0"
  },
  {
    "name": "com.apple.driver.AppleUSBDeviceAudioController.0.__TEXT_EXEC.__text",
    "size": 9808,
    "vsize": 9808,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1cba4a0",
    "vaddr": "0xfffffff008cbe4a0"
  },
  {
    "name": "com.apple.driver.AppleUSBAudio.0.__TEXT_EXEC.__text",
    "size": 431616,
    "vsize": 431616,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1cbcbc0",
    "vaddr": "0xfffffff008cc0bc0"
  },
  {
    "name": "com.apple.driver.DiskImages.UDIFDiskImage.0.__TEXT_EXEC.__text",
    "size": 39032,
    "vsize": 39032,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d26290",
    "vaddr": "0xfffffff008d2a290"
  },
  {
    "name": "com.apple.AppleLMBacklight.0.__TEXT_EXEC.__text",
    "size": 7440,
    "vsize": 7440,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d2fbd8",
    "vaddr": "0xfffffff008d33bd8"
  },
  {
    "name": "com.apple.iokit.IOSCSIArchitectureModelFamily.0.__TEXT_EXEC.__text",
    "size": 89280,
    "vsize": 89280,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d319b8",
    "vaddr": "0xfffffff008d359b8"
  },
  {
    "name": "com.apple.iokit.IOSCSIBlockCommandsDevice.0.__TEXT_EXEC.__text",
    "size": 47816,
    "vsize": 47816,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d47748",
    "vaddr": "0xfffffff008d4b748"
  },
  {
    "name": "com.apple.iokit.IOUSBMassStorageDriver.0.__TEXT_EXEC.__text",
    "size": 103248,
    "vsize": 103248,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d532e0",
    "vaddr": "0xfffffff008d572e0"
  },
  {
    "name": "com.apple.driver.AppleUSBCardReader.0.__TEXT_EXEC.__text",
    "size": 42760,
    "vsize": 42760,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d6c700",
    "vaddr": "0xfffffff008d70700"
  },
  {
    "name": "com.apple.driver.ApplePinotLCD.0.__TEXT_EXEC.__text",
    "size": 36576,
    "vsize": 36576,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d76ed8",
    "vaddr": "0xfffffff008d7aed8"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedGPS.0.__TEXT_EXEC.__text",
    "size": 14240,
    "vsize": 14240,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d7fe88",
    "vaddr": "0xfffffff008d83e88"
  },
  {
    "name": "com.apple.driver.AppleSMCWirelessCharger.0.__TEXT_EXEC.__text",
    "size": 40496,
    "vsize": 40496,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d836f8",
    "vaddr": "0xfffffff008d876f8"
  },
  {
    "name": "com.apple.nke.ppp.0.__TEXT_EXEC.__text",
    "size": 28456,
    "vsize": 28456,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d8d5f8",
    "vaddr": "0xfffffff008d915f8"
  },
  {
    "name": "com.apple.nke.lttp.0.__TEXT_EXEC.__text",
    "size": 16480,
    "vsize": 16480,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d945f0",
    "vaddr": "0xfffffff008d985f0"
  },
  {
    "name": "com.apple.driver.AppleSynopsysOTGDevice.0.__TEXT_EXEC.__text",
    "size": 150016,
    "vsize": 150016,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1d98720",
    "vaddr": "0xfffffff008d9c720"
  },
  {
    "name": "com.apple.driver.AppleAOPAudio.0.__TEXT_EXEC.__text_1",
    "size": 173624,
    "vsize": 173624,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1dbd1f0",
    "vaddr": "0xfffffff008dc11f0"
  },
  {
    "name": "com.apple.driver.AppleAOPHaptics.0.__TEXT_EXEC.__text",
    "size": 10672,
    "vsize": 10672,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1de78f8",
    "vaddr": "0xfffffff008deb8f8"
  },
  {
    "name": "com.apple.drivers.AppleSPURose.0.__TEXT_EXEC.__text",
    "size": 38744,
    "vsize": 38744,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1dea378",
    "vaddr": "0xfffffff008dee378"
  },
  {
    "name": "com.apple.driver.AppleUSBTopCaseDriver.0.__TEXT_EXEC.__text",
    "size": 984,
    "vsize": 984,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1df3ba0",
    "vaddr": "0xfffffff008df7ba0"
  },
  {
    "name": "com.apple.AUC.0.__TEXT_EXEC.__text",
    "size": 15456,
    "vsize": 15456,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1df4048",
    "vaddr": "0xfffffff008df8048"
  },
  {
    "name": "com.apple.driver.AppleALSColorSensor.0.__TEXT_EXEC.__text",
    "size": 65392,
    "vsize": 65392,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1df7d78",
    "vaddr": "0xfffffff008dfbd78"
  },
  {
    "name": "com.apple.driver.FairPlayIOKit.0.__TEXT_EXEC.__text",
    "size": 391528,
    "vsize": 391528,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1e07db8",
    "vaddr": "0xfffffff008e0bdb8"
  },
  {
    "name": "com.apple.IOTextEncryptionFamily.0.__TEXT_EXEC.__text",
    "size": 5240,
    "vsize": 5240,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1e677f0",
    "vaddr": "0xfffffff008e6b7f0"
  },
  {
    "name": "com.apple.AppleAstrisGpioProbe.0.__TEXT_EXEC.__text",
    "size": 15120,
    "vsize": 15120,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1e68d38",
    "vaddr": "0xfffffff008e6cd38"
  },
  {
    "name": "com.apple.driver.AppleConvergedIPCOLYBTControl.0.__TEXT_EXEC.__text",
    "size": 251872,
    "vsize": 251872,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1e6c918",
    "vaddr": "0xfffffff008e70918"
  },
  {
    "name": "com.apple.driver.LSKDIOKit.0.__TEXT_EXEC.__text",
    "size": 630808,
    "vsize": 630808,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1eaa1c8",
    "vaddr": "0xfffffff008eae1c8"
  },
  {
    "name": "com.apple.driver.ApplePMPFirmware.0.__TEXT_EXEC.__text",
    "size": 3224,
    "vsize": 3224,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f442b0",
    "vaddr": "0xfffffff008f482b0"
  },
  {
    "name": "com.apple.driver.usb.AppleUSBHostiOSDevice.0.__TEXT_EXEC.__text",
    "size": 3920,
    "vsize": 3920,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f45018",
    "vaddr": "0xfffffff008f49018"
  },
  {
    "name": "com.apple.driver.AppleUSBMike.0.__TEXT_EXEC.__text",
    "size": 36784,
    "vsize": 36784,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f46038",
    "vaddr": "0xfffffff008f4a038"
  },
  {
    "name": "com.apple.driver.AppleMobileApNonce.0.__TEXT_EXEC.__text",
    "size": 11064,
    "vsize": 11064,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f4f0b8",
    "vaddr": "0xfffffff008f530b8"
  },
  {
    "name": "com.apple.driver.AppleSPMI.0.__TEXT_EXEC.__text",
    "size": 15248,
    "vsize": 15248,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f51cc0",
    "vaddr": "0xfffffff008f55cc0"
  },
  {
    "name": "com.apple.driver.AppleEffaceableStorage.0.__TEXT_EXEC.__text",
    "size": 19224,
    "vsize": 19224,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f55920",
    "vaddr": "0xfffffff008f59920"
  },
  {
    "name": "com.apple.driver.AppleSEPKeyStore.0.__TEXT_EXEC.__text",
    "size": 179232,
    "vsize": 179232,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f5a508",
    "vaddr": "0xfffffff008f5e508"
  },
  {
    "name": "com.apple.driver.IOImageLoader.0.__TEXT_EXEC.__text",
    "size": 67232,
    "vsize": 67232,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f861f8",
    "vaddr": "0xfffffff008f8a1f8"
  },
  {
    "name": "com.apple.driver.BCMWLANFirmware4378_Hashstore.0.__TEXT_EXEC.__text",
    "size": 1040,
    "vsize": 1040,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f96968",
    "vaddr": "0xfffffff008f9a968"
  },
  {
    "name": "com.apple.driver.DiskImages.FileBackingStore.0.__TEXT_EXEC.__text",
    "size": 4408,
    "vsize": 4408,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f96e48",
    "vaddr": "0xfffffff008f9ae48"
  },
  {
    "name": "com.apple.driver.ApplePMP.0.__TEXT_EXEC.__text",
    "size": 39840,
    "vsize": 39840,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1f98050",
    "vaddr": "0xfffffff008f9c050"
  },
  {
    "name": "com.apple.driver.AppleS5L8960XNCO.0.__TEXT_EXEC.__text",
    "size": 2952,
    "vsize": 2952,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fa1cc0",
    "vaddr": "0xfffffff008fa5cc0"
  },
  {
    "name": "com.apple.iokit.IOStreamFamily.0.__TEXT_EXEC.__text",
    "size": 11920,
    "vsize": 11920,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fa2918",
    "vaddr": "0xfffffff008fa6918"
  },
  {
    "name": "com.apple.driver.AppleAOPAD5860.0.__TEXT_EXEC.__text",
    "size": 17192,
    "vsize": 17192,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fa5878",
    "vaddr": "0xfffffff008fa9878"
  },
  {
    "name": "com.apple.AppleT8030.0.__TEXT_EXEC.__text",
    "size": 32672,
    "vsize": 32672,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fa9c70",
    "vaddr": "0xfffffff008fadc70"
  },
  {
    "name": "com.apple.driver.AppleHIDKeyboard.0.__TEXT_EXEC.__text",
    "size": 18664,
    "vsize": 18664,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fb1ce0",
    "vaddr": "0xfffffff008fb5ce0"
  },
  {
    "name": "com.apple.driver.AppleTopCaseHIDEventDriver.0.__TEXT_EXEC.__text",
    "size": 29072,
    "vsize": 29072,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fb6698",
    "vaddr": "0xfffffff008fba698"
  },
  {
    "name": "com.apple.driver.AppleChestnutDisplayPMU.0.__TEXT_EXEC.__text",
    "size": 2880,
    "vsize": 2880,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fbd8f8",
    "vaddr": "0xfffffff008fc18f8"
  },
  {
    "name": "com.apple.EncryptedBlockStorage.0.__TEXT_EXEC.__text",
    "size": 13176,
    "vsize": 13176,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fbe508",
    "vaddr": "0xfffffff008fc2508"
  },
  {
    "name": "com.apple.kec.pthread.0.__TEXT_EXEC.__text",
    "size": 22592,
    "vsize": 22592,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fc1950",
    "vaddr": "0xfffffff008fc5950"
  },
  {
    "name": "com.apple.driver.AppleStockholmControl.0.__TEXT_EXEC.__text",
    "size": 47056,
    "vsize": 47056,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fc7260",
    "vaddr": "0xfffffff008fcb260"
  },
  {
    "name": "com.apple.driver.AppleSamsungSerial.0.__TEXT_EXEC.__text",
    "size": 7832,
    "vsize": 7832,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fd2b00",
    "vaddr": "0xfffffff008fd6b00"
  },
  {
    "name": "com.apple.driver.AppleBSDKextStarter.0.__TEXT_EXEC.__text",
    "size": 936,
    "vsize": 936,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fd4a68",
    "vaddr": "0xfffffff008fd8a68"
  },
  {
    "name": "com.apple.driver.usb.cdc.ecm.0.__TEXT_EXEC.__text",
    "size": 16176,
    "vsize": 16176,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fd4ee0",
    "vaddr": "0xfffffff008fd8ee0"
  },
  {
    "name": "com.apple.filesystems.apfs.0.__TEXT_EXEC.__text",
    "size": 937872,
    "vsize": 937872,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1fd8ee0",
    "vaddr": "0xfffffff008fdcee0"
  },
  {
    "name": "com.apple.kext.Match.0.__TEXT_EXEC.__text",
    "size": 8192,
    "vsize": 8192,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x20bdf40",
    "vaddr": "0xfffffff0090c1f40"
  },
  {
    "name": "com.apple.driver.AppleEffaceableBlockDevice.0.__TEXT_EXEC.__text",
    "size": 3048,
    "vsize": 3048,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x20c0010",
    "vaddr": "0xfffffff0090c4010"
  },
  {
    "name": "com.apple.driver.AppleCS35L27Amp.0.__TEXT_EXEC.__text",
    "size": 86784,
    "vsize": 86784,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x20c0cc8",
    "vaddr": "0xfffffff0090c4cc8"
  },
  {
    "name": "com.apple.AppleS8000AES.0.__TEXT_EXEC.__text",
    "size": 12480,
    "vsize": 12480,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x20d6098",
    "vaddr": "0xfffffff0090da098"
  },
  {
    "name": "com.apple.driver.usb.ethernet.asix.0.__TEXT_EXEC.__text",
    "size": 73872,
    "vsize": 73872,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x20d9228",
    "vaddr": "0xfffffff0090dd228"
  },
  {
    "name": "com.apple.driver.AppleCredentialManager.0.__TEXT_EXEC.__text",
    "size": 110480,
    "vsize": 110480,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x20eb388",
    "vaddr": "0xfffffff0090ef388"
  },
  {
    "name": "com.apple.driver.AppleMCA2-T8030.0.__TEXT_EXEC.__text",
    "size": 147896,
    "vsize": 147896,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21063e8",
    "vaddr": "0xfffffff00910a3e8"
  },
  {
    "name": "com.apple.driver.AppleUSBXDCI.0.__TEXT_EXEC.__text",
    "size": 156360,
    "vsize": 156360,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x212a670",
    "vaddr": "0xfffffff00912e670"
  },
  {
    "name": "com.apple.driver.AppleT8011USBXDCI.0.__TEXT_EXEC.__text",
    "size": 14032,
    "vsize": 14032,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2150a08",
    "vaddr": "0xfffffff009154a08"
  },
  {
    "name": "com.apple.driver.AppleUSBXDCIARM.0.__TEXT_EXEC.__text",
    "size": 57400,
    "vsize": 57400,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21541a8",
    "vaddr": "0xfffffff0091581a8"
  },
  {
    "name": "com.apple.driver.AppleT8027USBXDCI.0.__TEXT_EXEC.__text",
    "size": 5752,
    "vsize": 5752,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21622b0",
    "vaddr": "0xfffffff0091662b0"
  },
  {
    "name": "com.apple.watchdog.0.__TEXT_EXEC.__text",
    "size": 5968,
    "vsize": 5968,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21639f8",
    "vaddr": "0xfffffff0091679f8"
  },
  {
    "name": "com.apple.driver.AppleAVE.0.__TEXT_EXEC.__text",
    "size": 392928,
    "vsize": 392928,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2165218",
    "vaddr": "0xfffffff009169218"
  },
  {
    "name": "com.apple.driver.AppleInterruptController.0.__TEXT_EXEC.__text",
    "size": 12832,
    "vsize": 12832,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21c51c8",
    "vaddr": "0xfffffff0091c91c8"
  },
  {
    "name": "com.apple.driver.AppleSamsungPKE.0.__TEXT_EXEC.__text",
    "size": 4416,
    "vsize": 4416,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21c84b8",
    "vaddr": "0xfffffff0091cc4b8"
  },
  {
    "name": "com.apple.iokit.IOGPUFamily.0.__TEXT_EXEC.__text",
    "size": 139872,
    "vsize": 139872,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21c96c8",
    "vaddr": "0xfffffff0091cd6c8"
  },
  {
    "name": "com.apple.AGXG12P.0.__TEXT_EXEC.__text",
    "size": 436392,
    "vsize": 436392,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x21eb9f8",
    "vaddr": "0xfffffff0091ef9f8"
  },
  {
    "name": "com.apple.drivers.AppleSPUSphere.0.__TEXT_EXEC.__text",
    "size": 5304,
    "vsize": 5304,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2256370",
    "vaddr": "0xfffffff00925a370"
  },
  {
    "name": "com.apple.driver.AppleAVD.0.__TEXT_EXEC.__text",
    "size": 262048,
    "vsize": 262048,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22578f8",
    "vaddr": "0xfffffff00925b8f8"
  },
  {
    "name": "com.apple.security.sandbox.0.__TEXT_EXEC.__text",
    "size": 135320,
    "vsize": 135320,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2297968",
    "vaddr": "0xfffffff00929b968"
  },
  {
    "name": "com.apple.driver.AppleTemperatureSensor.0.__TEXT_EXEC.__text",
    "size": 32160,
    "vsize": 32160,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22b8ad0",
    "vaddr": "0xfffffff0092bcad0"
  },
  {
    "name": "com.apple.iokit.AppleSEPGenericTransfer.0.__TEXT_EXEC.__text",
    "size": 8520,
    "vsize": 8520,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22c0940",
    "vaddr": "0xfffffff0092c4940"
  },
  {
    "name": "com.apple.iokit.IOBiometricFamily.0.__TEXT_EXEC.__text",
    "size": 47432,
    "vsize": 47432,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22c2b58",
    "vaddr": "0xfffffff0092c6b58"
  },
  {
    "name": "com.apple.driver.AppleH10CameraInterface.0.__TEXT_EXEC.__text",
    "size": 6536,
    "vsize": 6536,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22ce570",
    "vaddr": "0xfffffff0092d2570"
  },
  {
    "name": "com.apple.ApplePearlSEPDriver.0.__TEXT_EXEC.__text",
    "size": 183824,
    "vsize": 183824,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22cffc8",
    "vaddr": "0xfffffff0092d3fc8"
  },
  {
    "name": "com.apple.driver.AppleNANDConfigAccess.0.__TEXT_EXEC.__text",
    "size": 416,
    "vsize": 416,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22fcea8",
    "vaddr": "0xfffffff009300ea8"
  },
  {
    "name": "com.apple.iokit.IONVMeFamily.0.__TEXT_EXEC.__text",
    "size": 310792,
    "vsize": 310792,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x22fd118",
    "vaddr": "0xfffffff009301118"
  },
  {
    "name": "com.apple.driver.AppleH10CameraInterface.0.__TEXT_EXEC.__text_1",
    "size": 373752,
    "vsize": 373752,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2348ff0",
    "vaddr": "0xfffffff00934cff0"
  },
  {
    "name": "com.apple.driver.AppleEmbeddedAudioResourceManager.0.__TEXT_EXEC.__text",
    "size": 9312,
    "vsize": 9312,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x23a44b8",
    "vaddr": "0xfffffff0093a84b8"
  },
  {
    "name": "com.apple.driver.AppleBasebandI19.0.__TEXT_EXEC.__text",
    "size": 51200,
    "vsize": 51200,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x23a69e8",
    "vaddr": "0xfffffff0093aa9e8"
  },
  {
    "name": "com.apple.driver.AppleActuatorDriver.0.__TEXT_EXEC.__text",
    "size": 32072,
    "vsize": 32072,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x23b32b8",
    "vaddr": "0xfffffff0093b72b8"
  },
  {
    "name": "com.apple.driver.IOAudioCodecs.0.__TEXT_EXEC.__text",
    "size": 266464,
    "vsize": 266464,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x23bb0d0",
    "vaddr": "0xfffffff0093bf0d0"
  },
  {
    "name": "com.apple.driver.DiskImages.ReadWriteDiskImage.0.__TEXT_EXEC.__text",
    "size": 896,
    "vsize": 896,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x23fc280",
    "vaddr": "0xfffffff009400280"
  },
  {
    "name": "com.apple.AppleFSCompression.AppleFSCompressionTypeZlib.0.__TEXT_EXEC.__text",
    "size": 30528,
    "vsize": 30528,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x23fc6d0",
    "vaddr": "0xfffffff0094006d0"
  },
  {
    "name": "com.apple.driver.AppleBCMWLANCore.0.__TEXT_EXEC.__text",
    "size": 1635648,
    "vsize": 1635648,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2403ee0",
    "vaddr": "0xfffffff009407ee0"
  },
  {
    "name": "com.apple.driver.AppleBCMWLANBusInterfacePCIe.0.__TEXT_EXEC.__text",
    "size": 360336,
    "vsize": 360336,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x25934f0",
    "vaddr": "0xfffffff0095974f0"
  },
  {
    "name": "com.apple.driver.AppleUSBDeviceNCM.0.__TEXT_EXEC.__text",
    "size": 27344,
    "vsize": 27344,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x25eb550",
    "vaddr": "0xfffffff0095ef550"
  },
  {
    "name": "com.apple.driver.AppleS5L8960XUSB.0.__TEXT_EXEC.__text",
    "size": 9112,
    "vsize": 9112,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x25f20f0",
    "vaddr": "0xfffffff0095f60f0"
  },
  {
    "name": "com.apple.driver.AppleT8011USB.0.__TEXT_EXEC.__text",
    "size": 11552,
    "vsize": 11552,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x25f4558",
    "vaddr": "0xfffffff0095f8558"
  },
  {
    "name": "com.apple.driver.AppleT8027USB.0.__TEXT_EXEC.__text",
    "size": 17944,
    "vsize": 17944,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x25f7348",
    "vaddr": "0xfffffff0095fb348"
  },
  {
    "name": "com.apple.driver.AppleFAN53740.0.__TEXT_EXEC.__text",
    "size": 4640,
    "vsize": 4640,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x25fba30",
    "vaddr": "0xfffffff0095ffa30"
  },
  {
    "name": "com.apple.file-systems.hfs.kext.0.__TEXT_EXEC.__text",
    "size": 322256,
    "vsize": 322256,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x25fcd20",
    "vaddr": "0xfffffff009600d20"
  },
  {
    "name": "com.apple.driver.AppleM68Buttons.0.__TEXT_EXEC.__text",
    "size": 36560,
    "vsize": 36560,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x264b8c0",
    "vaddr": "0xfffffff00964f8c0"
  },
  {
    "name": "com.apple.driver.AppleDialogPMU.0.__TEXT_EXEC.__text",
    "size": 7024,
    "vsize": 7024,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2654860",
    "vaddr": "0xfffffff009658860"
  },
  {
    "name": "com.apple.driver.AppleSPMIPMU.0.__TEXT_EXEC.__text",
    "size": 37976,
    "vsize": 37976,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x26564a0",
    "vaddr": "0xfffffff00965a4a0"
  },
  {
    "name": "com.apple.driver.AppleUSBDeviceMux.0.__TEXT_EXEC.__text",
    "size": 22096,
    "vsize": 22096,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x265f9c8",
    "vaddr": "0xfffffff0096639c8"
  },
  {
    "name": "com.apple.driver.AOPTouchKext.0.__TEXT_EXEC.__text",
    "size": 7344,
    "vsize": 7344,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x26650e8",
    "vaddr": "0xfffffff0096690e8"
  },
  {
    "name": "com.apple.driver.AppleS5L8960XWatchDogTimer.0.__TEXT_EXEC.__text",
    "size": 8136,
    "vsize": 8136,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2666e68",
    "vaddr": "0xfffffff00966ae68"
  },
  {
    "name": "com.apple.iokit.IOAccessoryPortUSB.0.__TEXT_EXEC.__text",
    "size": 8648,
    "vsize": 8648,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2668f00",
    "vaddr": "0xfffffff00966cf00"
  },
  {
    "name": "com.apple.driver.ApplePinotLCD.0.__TEXT_EXEC.__text_1",
    "size": 4232,
    "vsize": 4232,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x266b198",
    "vaddr": "0xfffffff00966f198"
  },
  {
    "name": "com.apple.driver.BTM.0.__TEXT_EXEC.__text",
    "size": 18256,
    "vsize": 18256,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x266c2f0",
    "vaddr": "0xfffffff0096702f0"
  },
  {
    "name": "com.apple.filesystems.tmpfs.0.__TEXT_EXEC.__text",
    "size": 39144,
    "vsize": 39144,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2670b10",
    "vaddr": "0xfffffff009674b10"
  },
  {
    "name": "com.apple.driver.AppleT8020DART.0.__TEXT_EXEC.__text",
    "size": 34080,
    "vsize": 34080,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x267a4c8",
    "vaddr": "0xfffffff00967e4c8"
  },
  {
    "name": "com.apple.driver.AppleBluetoothModule.0.__TEXT_EXEC.__text",
    "size": 27488,
    "vsize": 27488,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2682ab8",
    "vaddr": "0xfffffff009686ab8"
  },
  {
    "name": "com.apple.driver.AppleUSBEthernetHost.0.__TEXT_EXEC.__text",
    "size": 16112,
    "vsize": 16112,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x26896e8",
    "vaddr": "0xfffffff00968d6e8"
  },
  {
    "name": "com.apple.driver.AppleIDV.0.__TEXT_EXEC.__text",
    "size": 1648,
    "vsize": 1648,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x268d6a8",
    "vaddr": "0xfffffff0096916a8"
  },
  {
    "name": "com.apple.AGXFirmwareKextRTBuddy64.0.__TEXT_EXEC.__text",
    "size": 8768,
    "vsize": 8768,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x268dde8",
    "vaddr": "0xfffffff009691de8"
  },
  {
    "name": "com.apple.AGXFirmwareKextG12PRTBuddy.0.__TEXT_EXEC.__text",
    "size": 1368,
    "vsize": 1368,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x26900f8",
    "vaddr": "0xfffffff0096940f8"
  },
  {
    "name": "com.apple.driver.AppleIDAMInterface.0.__TEXT_EXEC.__text",
    "size": 2704,
    "vsize": 2704,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2690720",
    "vaddr": "0xfffffff009694720"
  },
  {
    "name": "com.apple.driver.LSKDIOKitMSE.0.__TEXT_EXEC.__text",
    "size": 2600,
    "vsize": 2600,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2691280",
    "vaddr": "0xfffffff009695280"
  },
  {
    "name": "com.apple.driver.AppleCS42L77Audio.0.__TEXT_EXEC.__text",
    "size": 259992,
    "vsize": 259992,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2691d78",
    "vaddr": "0xfffffff009695d78"
  },
  {
    "name": "com.apple.driver.AppleDiagnosticDataAccessReadOnly.0.__TEXT_EXEC.__text",
    "size": 2600,
    "vsize": 2600,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x26d15e0",
    "vaddr": "0xfffffff0096d55e0"
  },
  {
    "name": "com.apple.iokit.IOMobileGraphicsFamily.0.__TEXT_EXEC.__text",
    "size": 201448,
    "vsize": 201448,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x26d20d8",
    "vaddr": "0xfffffff0096d60d8"
  },
  {
    "name": "com.apple.driver.AppleMobileDispH12P.0.__TEXT_EXEC.__text",
    "size": 797072,
    "vsize": 797072,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x2703490",
    "vaddr": "0xfffffff009707490"
  },
  {
    "name": "com.apple.driver.AppleGenericMultitouch.0.__TEXT_EXEC.__text",
    "size": 23512,
    "vsize": 23512,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x27c5ef0",
    "vaddr": "0xfffffff0097c9ef0"
  },
  {
    "name": "com.apple.driver.AppleDiskImages2.0.__TEXT_EXEC.__text",
    "size": 37952,
    "vsize": 37952,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x27cbb98",
    "vaddr": "0xfffffff0097cfb98"
  },
  {
    "name": "0.__TEXT.__const",
    "size": 3338838,
    "vsize": 3338838,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x1860",
    "vaddr": "0xfffffff007005860"
  },
  {
    "name": "1.__TEXT.__cstring",
    "size": 2987119,
    "vsize": 2987119,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x330ab6",
    "vaddr": "0xfffffff007334ab6"
  },
  {
    "name": "2.__TEXT.__os_log",
    "size": 710395,
    "vsize": 710395,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x609f25",
    "vaddr": "0xfffffff00760df25"
  },
  {
    "name": "3.__TEXT.__fips_hmacs",
    "size": 32,
    "vsize": 32,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x6b7620",
    "vaddr": "0xfffffff0076bb620"
  },
  {
    "name": "4.__TEXT.__info_plist",
    "size": 1262,
    "vsize": 1262,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x6b7640",
    "vaddr": "0xfffffff0076bb640"
  },
  {
    "name": "5.__TEXT.__ustring",
    "size": 106,
    "vsize": 106,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x6b7b2e",
    "vaddr": "0xfffffff0076bbb2e"
  },
  {
    "name": "6.__TEXT.__thread_starts",
    "size": 876,
    "vsize": 876,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x6b7b98",
    "vaddr": "0xfffffff0076bbb98"
  },
  {
    "name": "7.__TEXT.__eh_frame",
    "size": 248,
    "vsize": 248,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x6b7f08",
    "vaddr": "0xfffffff0076bbf08"
  },
  {
    "name": "8.__DATA_CONST.__auth_ptr",
    "size": 4152,
    "vsize": 4152,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x6b8000",
    "vaddr": "0xfffffff0076bc000"
  },
  {
    "name": "9.__DATA_CONST.__mod_init_func",
    "size": 640,
    "vsize": 640,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x6b9038",
    "vaddr": "0xfffffff0076bd038"
  },
  {
    "name": "10.__DATA_CONST.__kmod_init",
    "size": 13192,
    "vsize": 13192,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x6b92b8",
    "vaddr": "0xfffffff0076bd2b8"
  },
  {
    "name": "11.__DATA_CONST.__kmod_term",
    "size": 12864,
    "vsize": 12864,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x6bc640",
    "vaddr": "0xfffffff0076c0640"
  },
  {
    "name": "12.__DATA_CONST.__const",
    "size": 3287600,
    "vsize": 3287600,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x6c0000",
    "vaddr": "0xfffffff0076c4000"
  },
  {
    "name": "13.__DATA_CONST.__sysctl_set",
    "size": 10744,
    "vsize": 10744,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x9e2a30",
    "vaddr": "0xfffffff0079e6a30"
  },
  {
    "name": "14.__DATA_CONST.__hib_const",
    "size": 288,
    "vsize": 288,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x9e5430",
    "vaddr": "0xfffffff0079e9430"
  },
  {
    "name": "15.__TEXT_EXEC.__text",
    "size": 31390144,
    "vsize": 31390144,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x9e8000",
    "vaddr": "0xfffffff0079ec000"
  },
  {
    "name": "16.__TEXT_EXEC.__hib_text",
    "size": 3856,
    "vsize": 3856,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x27d79c0",
    "vaddr": "0xfffffff0097db9c0"
  },
  {
    "name": "17.__PPLTEXT.__text",
    "size": 104516,
    "vsize": 104516,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x27dc000",
    "vaddr": "0xfffffff0097e0000"
  },
  {
    "name": "18.__PPLDATA_CONST.__const",
    "size": 256,
    "vsize": 256,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x27f8000",
    "vaddr": "0xfffffff0097fc000"
  },
  {
    "name": "19.__LAST.__pinst",
    "size": 8,
    "vsize": 8,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x27fc000",
    "vaddr": "0xfffffff009800000"
  },
  {
    "name": "20.__LAST.__mod_init_func",
    "size": 8,
    "vsize": 8,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x27fc008",
    "vaddr": "0xfffffff009800008"
  },
  {
    "name": "21.__LAST.__last",
    "size": 0,
    "vsize": 0,
    "perm": "-r-x",
    "flags": 0,
    "paddr": "0x0",
    "vaddr": "0xfffffff009800010"
  },
  {
    "name": "22.__PPLDATA.__data",
    "size": 3640,
    "vsize": 3640,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2800000",
    "vaddr": "0xfffffff009804000"
  },
  {
    "name": "23.__KLD.__text",
    "size": 6420,
    "vsize": 6420,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2804000",
    "vaddr": "0xfffffff009808000"
  },
  {
    "name": "24.__KLD.__cstring",
    "size": 1752,
    "vsize": 1752,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2805914",
    "vaddr": "0xfffffff009809914"
  },
  {
    "name": "25.__KLD.__const",
    "size": 112,
    "vsize": 112,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2805ff0",
    "vaddr": "0xfffffff009809ff0"
  },
  {
    "name": "26.__KLD.__mod_init_func",
    "size": 8,
    "vsize": 8,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2806060",
    "vaddr": "0xfffffff00980a060"
  },
  {
    "name": "27.__KLD.__mod_term_func",
    "size": 8,
    "vsize": 8,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2806068",
    "vaddr": "0xfffffff00980a068"
  },
  {
    "name": "28.__KLD.__auth_ptr",
    "size": 8,
    "vsize": 8,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2806070",
    "vaddr": "0xfffffff00980a070"
  },
  {
    "name": "29.__KLD.__bss",
    "size": 0,
    "vsize": 1,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x0",
    "vaddr": "0xfffffff00980a078"
  },
  {
    "name": "30.__DATA.__data",
    "size": 1664464,
    "vsize": 1664464,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x2808000",
    "vaddr": "0xfffffff00980c000"
  },
  {
    "name": "31.__DATA.__lock_grp",
    "size": 11928,
    "vsize": 11928,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x299e5d0",
    "vaddr": "0xfffffff0099a25d0"
  },
  {
    "name": "32.__DATA.__percpu",
    "size": 2608,
    "vsize": 2608,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29a1480",
    "vaddr": "0xfffffff0099a5480"
  },
  {
    "name": "33.__DATA.__img4_rt",
    "size": 24,
    "vsize": 24,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29a1eb0",
    "vaddr": "0xfffffff0099a5eb0"
  },
  {
    "name": "34.__DATA.__common",
    "size": 0,
    "vsize": 423520,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x0",
    "vaddr": "0xfffffff0099a6000"
  },
  {
    "name": "35.__DATA.__bss",
    "size": 0,
    "vsize": 317723,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x0",
    "vaddr": "0xfffffff009a0e000"
  },
  {
    "name": "36.__BOOTDATA.__init_entry_set",
    "size": 9528,
    "vsize": 9528,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29a4000",
    "vaddr": "0xfffffff009a5c000"
  },
  {
    "name": "37.__BOOTDATA.__init",
    "size": 13440,
    "vsize": 13440,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29a6538",
    "vaddr": "0xfffffff009a5e538"
  },
  {
    "name": "38.__BOOTDATA.__data",
    "size": 98304,
    "vsize": 98304,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29ac000",
    "vaddr": "0xfffffff009a64000"
  },
  {
    "name": "39.__PRELINK_INFO.__kmod_info",
    "size": 1824,
    "vsize": 1824,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29c4000",
    "vaddr": "0xfffffff009a7c000"
  },
  {
    "name": "40.__PRELINK_INFO.__kmod_start",
    "size": 1832,
    "vsize": 1832,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29c4720",
    "vaddr": "0xfffffff009a7c720"
  },
  {
    "name": "41.__PRELINK_INFO.__info",
    "size": 1121030,
    "vsize": 1121030,
    "perm": "-rw-",
    "flags": 0,
    "paddr": "0x29c4e48",
    "vaddr": "0xfffffff009a7ce48"
  },
  {
    "name": "42.__PRELINK_TEXT.__text",
    "size": 0,
    "vsize": 0,
    "perm": "----",
    "flags": 0,
    "paddr": "0x2ad8000",
    "vaddr": "0xfffffff004004000"
  },
  {
    "name": "43.__PLK_TEXT_EXEC.__text",
    "size": 0,
    "vsize": 0,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x2ad8000",
    "vaddr": "0xfffffff009b90000"
  },
  {
    "name": "44.__PRELINK_DATA.__data",
    "size": 0,
    "vsize": 0,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x2ad8000",
    "vaddr": "0xfffffff009b90000"
  },
  {
    "name": "45.__PLK_DATA_CONST.__data",
    "size": 0,
    "vsize": 0,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x2ad8000",
    "vaddr": "0xfffffff009b90000"
  },
  {
    "name": "46.__PLK_LLVM_COV.__llvm_covmap",
    "size": 0,
    "vsize": 0,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x2ad8000",
    "vaddr": "0xfffffff009b90000"
  },
  {
    "name": "47.__PLK_LINKEDIT.__data",
    "size": 0,
    "vsize": 0,
    "perm": "-r--",
    "flags": 0,
    "paddr": "0x2ad8000",
    "vaddr": "0xfffffff009b90000"
  }
];

function dumpAllKernelDataRanges() {
  for (const r of RANGES) {
    if (r.perm.indexOf('x') !== -1)
      continue;
    const size = r.vsize;
    if (size === 0)
      continue;

    try {
      const dump = ptr(r.vaddr).readByteArray(size);
      File.writeAllBytes(`./tools/dumps/${r.name}.bin`, dump);
      console.log(`Dumped ${r.name}`);
    } catch (e) {
      console.log(`Ignoring ${r.name}: ${e}`);
    }
  }
}

function dumpAllReadWriteRegions() {
  const ranges = Process.enumerateRanges('rw-');
  let i = 0;
  for (const r of ranges) {
    try {
      const dump = r.base.readByteArray(r.size);
      console.log(`Dumping ${i + 1}/${ranges.length}: ${r.base}-${r.base.add(r.size)}`);
      File.writeAllBytes(`./tools/dumps/rw_${r.base}.bin`, dump);
    } catch (e) {
      console.log(`Ignoring ${r.base}: ${e}`);
    }
    i++;
  }
}
