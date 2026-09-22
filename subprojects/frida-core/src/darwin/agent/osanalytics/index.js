import ObjC from 'frida-objc-bridge';

const YES = ptr(1);

const {
  NSFileManager,
  NSMutableDictionary,
  OSALog,
} = ObjC.classes;

const sessions = new Map();

function tryInitialize() {
  const counterMethod = NSMutableDictionary['- osa_logCounter_isLog:byKey:count:withinLimit:withOptions:'];
  if (counterMethod === undefined)
    return false;

  Interceptor.attach(OSALog['+ locallyCreateForSubmission:metadata:options:error:writing:'].implementation, {
    onEnter(args) {
      sessions.set(this.threadId, {
        forcedByUs: false
      });
    },
    onLeave(retval) {
      const log = new ObjC.Object(retval);
      const { threadId } = this;

      const session = sessions.get(threadId);
      if (session.forcedByUs) {
        const oldPath = log.filepath().toString();
        const newPath = oldPath + '.forced-by-frida';
        NSFileManager.defaultManager().moveItemAtPath_toPath_error_(oldPath, newPath, NULL);
        log.rename_(newPath);
      }

      sessions.delete(threadId);
    },
  });

  Interceptor.attach(counterMethod.implementation, {
    onLeave(retval) {
      const session = sessions.get(this.threadId);
      if (session === undefined)
        return;

      const isWithinLimit = !!retval.toInt32();
      if (!isWithinLimit) {
        retval.replace(YES);
        session.forcedByUs = true;
      }
    },
  });

  return true;
}

if (!tryInitialize()) {
  const listener = Interceptor.attach(
      Process.getModuleByName('/usr/lib/system/libsystem_trace.dylib').getExportByName('os_log_type_enabled'), () => {
    tryInitialize();
    listener.detach();
  });
}
