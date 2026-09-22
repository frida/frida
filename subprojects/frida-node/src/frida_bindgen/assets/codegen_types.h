typedef enum {
  FDN_SIGNAL_ALLOW_EXIT,
  FDN_SIGNAL_KEEP_ALIVE
} FdnSignalBehavior;

typedef struct {
  GObject * handle;
  guint id;
  FdnSignalBehavior behavior;
  GSList * closures;
} FdnSignal;

typedef enum {
  FDN_SIGNAL_CLOSURE_OPEN,
  FDN_SIGNAL_CLOSURE_CLOSED,
} FdnSignalClosureState;

typedef struct {
  GClosure closure;
  FdnSignal * sig;
  napi_ref js_sig;
  FdnSignalClosureState state;
  napi_threadsafe_function tsfn;
  napi_ref handler;
  gulong handler_id;
} FdnSignalClosure;

typedef enum {
  FDN_SIGNAL_CLOSURE_MESSAGE_DESTROY,
  FDN_SIGNAL_CLOSURE_MESSAGE_MARSHAL,
} FdnSignalClosureMessageType;

typedef struct {
  napi_ref js_sig;
  napi_threadsafe_function tsfn;
  napi_ref handler;
} FdnSignalClosureMessageDestroy;

typedef struct {
  GArray * args;
} FdnSignalClosureMessageMarshal;

typedef struct {
  FdnSignalClosureMessageType type;
  union {
    FdnSignalClosureMessageDestroy destroy;
    FdnSignalClosureMessageMarshal marshal;
  } payload;
} FdnSignalClosureMessage;

typedef struct {
  guint ref_count;
  GObject * handle;
  gulong signal_handler_id;
  napi_threadsafe_function tsfn;
} FdnKeepAliveContext;

typedef gboolean (* FdnIsDestroyedFunc) (GObject * handle);
