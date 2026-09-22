#ifndef __FRIDA_DARWIN_SPRINGBOARD_H__
#define __FRIDA_DARWIN_SPRINGBOARD_H__

#include <glib.h>
#import <UIKit/UIKit.h>

typedef struct _FridaSpringboardApi FridaSpringboardApi;
typedef void (^ FBSOpenResultCallback) (NSError * error);
typedef enum _FBProcessKillReason FBProcessKillReason;

enum _FBProcessKillReason
{
  FBProcessKillReasonUnknown,
  FBProcessKillReasonUser,
  FBProcessKillReasonPurge,
  FBProcessKillReasonGracefulPurge,
  FBProcessKillReasonThermal,
  FBProcessKillReasonNone,
  FBProcessKillReasonShutdown,
  FBProcessKillReasonLaunchTest,
  FBProcessKillReasonInsecureDrawing
};

@interface FBSSystemService : NSObject

+ (FBSSystemService *)sharedService;

- (pid_t)pidForApplication:(NSString *)identifier;
- (void)openApplication:(NSString *)identifier
                options:(NSDictionary *)options
             clientPort:(mach_port_t)port
             withResult:(FBSOpenResultCallback)result;
- (void)openURL:(NSURL *)url
    application:(NSString *)identifier
        options:(NSDictionary *)options
     clientPort:(mach_port_t)port
     withResult:(FBSOpenResultCallback)result;
- (void)terminateApplication:(NSString *)identifier
                   forReason:(FBProcessKillReason)reason
                   andReport:(BOOL)report
             withDescription:(NSString *)description;

- (mach_port_t)createClientPort;
- (void)cleanupClientPort:(mach_port_t)port;

@end

@interface LSApplicationProxy : NSObject

+ (LSApplicationProxy *)applicationProxyForIdentifier:(NSString *)identifier;

- (NSString *)applicationIdentifier;
- (NSString *)itemName;
- (NSString *)shortVersionString;
- (NSString *)bundleVersion;
- (NSURL *)bundleURL;
- (NSURL *)dataContainerURL;
- (NSDictionary<NSString *, NSURL *> *)groupContainerURLs;
- (id)entitlementValueForKey:(NSString *)key ofClass:(Class)klass;
- (id)localizedNameWithPreferredLocalizations:(id)arg1 useShortNameOnly:(BOOL)arg2;

@end

@interface LSApplicationWorkspace : NSObject

+ (LSApplicationWorkspace *)defaultWorkspace;

- (NSArray <LSApplicationProxy *> *)allApplications;

- (BOOL)openApplicationWithBundleID:(NSString *)bundleID;
- (BOOL)openURL:(NSURL *)url;

@end

struct _FridaSpringboardApi
{
  void * sbs;
  void * fbs;
  void * mcs;

  mach_port_t (* SBSSpringBoardBackgroundServerPort) (void);
  NSString * (* SBSCopyFrontmostApplicationDisplayIdentifier) (void);
  NSArray * (* SBSCopyApplicationDisplayIdentifiers) (BOOL active, BOOL debuggable);
  NSString * (* SBSCopyDisplayIdentifierForProcessID) (UInt32 pid);
  NSString * (* SBSCopyLocalizedApplicationNameForDisplayIdentifier) (NSString * identifier);
  NSData * (* SBSCopyIconImagePNGDataForDisplayIdentifier) (NSString * identifier);
  NSDictionary * (* SBSCopyInfoForApplicationWithProcessID) (UInt32 pid);
  UInt32 (* SBSLaunchApplicationWithIdentifierAndLaunchOptions) (NSString * identifier, NSDictionary * options, BOOL suspended);
  UInt32 (* SBSLaunchApplicationWithIdentifierAndURLAndLaunchOptions) (NSString * identifier, NSURL * url, NSDictionary * params, NSDictionary * options, BOOL suspended);
  NSString * (* SBSApplicationLaunchingErrorString) (UInt32 error);

  NSString * SBSApplicationLaunchOptionUnlockDeviceKey;

  NSString * FBSOpenApplicationOptionKeyUnlockDevice;
  NSString * FBSOpenApplicationOptionKeyDebuggingOptions;

  NSString * FBSDebugOptionKeyArguments;
  NSString * FBSDebugOptionKeyEnvironment;
  NSString * FBSDebugOptionKeyStandardOutPath;
  NSString * FBSDebugOptionKeyStandardErrorPath;
  NSString * FBSDebugOptionKeyDisableASLR;

  id FBSSystemService;
  id LSApplicationProxy;
  id LSApplicationWorkspace;
};

G_GNUC_INTERNAL FridaSpringboardApi * _frida_get_springboard_api (void);

#endif
