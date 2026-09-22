/*
 * Copyright (C) 2016 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_MACROS_H__
#define __GUM_V8_MACROS_H__

#include "gumv8value.h"

#define GUMJS_MODULE_TYPE G_PASTE (GumV8, GUMJS_MODULE_NAME)

#define GUMJS_DECLARE_CONSTRUCTOR GUMJS_DECLARE_FUNCTION
#define GUMJS_DECLARE_FUNCTION(N) \
    static void N (const FunctionCallbackInfo<Value> & info);
#define GUMJS_DECLARE_GETTER(N) \
    static void N (Local<Name> property, \
        const PropertyCallbackInfo<Value> & info);
#define GUMJS_DECLARE_SETTER(N) \
    static void N (Local<Name> property, Local<Value> value, \
        const PropertyCallbackInfo<void> & info);

#define GUMJS_DEFINE_CONSTRUCTOR(N) \
    struct GumV8Closure_##N \
    { \
    public: \
      GumV8Closure_##N (const FunctionCallbackInfo<Value> & info) \
        : wrapper (info.Holder ()), \
          module ((GUMJS_MODULE_TYPE *) \
              info.Data ().As<External> ()->Value ()), \
          core (module->core), \
          args (&_args), \
          info (info), \
          isolate (core->isolate) \
      { \
        _args.info = &info; \
        _args.core = core; \
      } \
      \
      void invoke (); \
      \
    protected: \
      Local<Object> wrapper; \
      GUMJS_MODULE_TYPE * module; \
      GumV8Core * core; \
      const GumV8Args * args; \
      const FunctionCallbackInfo<Value> & info; \
      Isolate * isolate; \
      \
      GumV8Args _args; \
    }; \
    \
    static void \
    N (const FunctionCallbackInfo<Value> & info) \
    { \
      GumV8Closure_##N closure (info); \
      closure.invoke (); \
    } \
    \
    void \
    GumV8Closure_##N::invoke ()
#define GUMJS_DEFINE_FUNCTION(N) \
    class GumV8Closure_##N \
    { \
    public: \
      GumV8Closure_##N (const FunctionCallbackInfo<Value> & info) \
        : module ((GUMJS_MODULE_TYPE *) \
              info.Data ().As<External> ()->Value ()), \
          core (module->core), \
          args (&_args), \
          info (info), \
          isolate (core->isolate) \
      { \
        _args.info = &info; \
        _args.core = core; \
      } \
      \
      void invoke (); \
      \
    protected: \
      GUMJS_MODULE_TYPE * module; \
      GumV8Core * core; \
      const GumV8Args * args; \
      const FunctionCallbackInfo<Value> & info; \
      Isolate * isolate; \
      \
      GumV8Args _args; \
    }; \
    \
    static void \
    N (const FunctionCallbackInfo<Value> & info) \
    { \
      GumV8Closure_##N closure (info); \
      closure.invoke (); \
    } \
    \
    void \
    GumV8Closure_##N::invoke ()
#define GUMJS_DEFINE_GETTER(N) \
    class GumV8Closure_##N \
    { \
    public: \
      GumV8Closure_##N (const PropertyCallbackInfo<Value> & info) \
        : module ( \
              (GUMJS_MODULE_TYPE *) info.Data ().As<External> ()->Value ()), \
          core (module->core), \
          info (info), \
          isolate (core->isolate) \
      { \
      } \
      \
      void invoke (); \
      \
    protected: \
      GUMJS_MODULE_TYPE * module; \
      GumV8Core * core; \
      const PropertyCallbackInfo<Value> & info; \
      Isolate * isolate; \
    }; \
    \
    static void \
    N (Local<Name> property, \
       const PropertyCallbackInfo<Value> & info) \
    { \
      GumV8Closure_##N closure (info); \
      closure.invoke (); \
    } \
    \
    void \
    GumV8Closure_##N::invoke ()
#define GUMJS_DEFINE_SETTER(N) \
    class GumV8Closure_##N \
    { \
    public: \
      GumV8Closure_##N (Local<Value> value, \
          const PropertyCallbackInfo<void> & info) \
        : module ( \
              (GUMJS_MODULE_TYPE *) info.Data ().As<External> ()->Value ()), \
          core (module->core), \
          value (value), \
          info (info), \
          isolate (core->isolate) \
      { \
      } \
      \
      void invoke (); \
      \
    protected: \
      GUMJS_MODULE_TYPE * module; \
      GumV8Core * core; \
      Local<Value> value; \
      const PropertyCallbackInfo<void> & info; \
      Isolate * isolate; \
    }; \
    \
    static void \
    N (Local<Name> property, \
       Local<Value> value, \
       const PropertyCallbackInfo<void> & info) \
    { \
      GumV8Closure_##N closure (value, info); \
      closure.invoke (); \
    } \
    \
    void \
    GumV8Closure_##N::invoke ()
#define GUMJS_DEFINE_CLASS_GETTER_AT_LEVEL(N, C, L) \
    class GumV8Closure_##N \
    { \
    public: \
      GumV8Closure_##N (const PropertyCallbackInfo<Value> & info) \
        : wrapper (info.Holder ()), \
          self ((C *) wrapper->GetAlignedPointerFromInternalField (L)), \
          module ( \
              (GUMJS_MODULE_TYPE *) info.Data ().As<External> ()->Value ()), \
          core (module->core), \
          info (info), \
          isolate (core->isolate) \
      { \
      } \
      \
      void invoke (); \
      \
    protected: \
      Local<Object> wrapper; \
      C * self; \
      GUMJS_MODULE_TYPE * module; \
      GumV8Core * core; \
      const PropertyCallbackInfo<Value> & info; \
      Isolate * isolate; \
    }; \
    \
    static void \
    N (Local<Name> property, \
       const PropertyCallbackInfo<Value> & info) \
    { \
      GumV8Closure_##N closure (info); \
      closure.invoke (); \
    } \
    \
    void \
    GumV8Closure_##N::invoke ()
#define GUMJS_DEFINE_CLASS_SETTER_AT_LEVEL(N, C, L) \
    class GumV8Closure_##N \
    { \
    public: \
      GumV8Closure_##N (Local<Value> value, \
          const PropertyCallbackInfo<void> & info) \
        : wrapper (info.Holder ()), \
          self ((C *) wrapper->GetAlignedPointerFromInternalField (L)), \
          module ( \
              (GUMJS_MODULE_TYPE *) info.Data ().As<External> ()->Value ()), \
          core (module->core), \
          value (value), \
          info (info), \
          isolate (core->isolate) \
      { \
      } \
      \
      void invoke (); \
      \
    protected: \
      Local<Object> wrapper; \
      C * self; \
      GUMJS_MODULE_TYPE * module; \
      GumV8Core * core; \
      Local<Value> value; \
      const PropertyCallbackInfo<void> & info; \
      Isolate * isolate; \
    }; \
    \
    static void \
    N (Local<Name> property, \
       Local<Value> value, \
       const PropertyCallbackInfo<void> & info) \
    { \
      GumV8Closure_##N closure (value, info); \
      closure.invoke (); \
    } \
    \
    void \
    GumV8Closure_##N::invoke ()
#define GUMJS_DEFINE_CLASS_METHOD_AT_LEVEL(N, C, L) \
    struct GumV8Closure_##N \
    { \
    public: \
      GumV8Closure_##N (const FunctionCallbackInfo<Value> & info) \
        : wrapper (info.Holder ()), \
          self ((C *) wrapper->GetAlignedPointerFromInternalField (L)), \
          module ( \
              (GUMJS_MODULE_TYPE *) info.Data ().As<External> ()->Value ()), \
          core (module->core), \
          args (&_args), \
          info (info), \
          isolate (core->isolate) \
      { \
        _args.info = &info; \
        _args.core = core; \
      } \
      \
      void invoke (); \
      \
    protected: \
      Local<Object> wrapper; \
      C * self; \
      GUMJS_MODULE_TYPE * module; \
      GumV8Core * core; \
      const GumV8Args * args; \
      const FunctionCallbackInfo<Value> & info; \
      Isolate * isolate; \
      \
      GumV8Args _args; \
    }; \
    \
    static void \
    N (const FunctionCallbackInfo<Value> & info) \
    { \
      GumV8Closure_##N closure (info); \
      closure.invoke (); \
    } \
    \
    void \
    GumV8Closure_##N::invoke ()

#define GUMJS_DEFINE_CLASS_GETTER(N, C) \
    GUMJS_DEFINE_CLASS_GETTER_AT_LEVEL (N, C, 0)
#define GUMJS_DEFINE_CLASS_SETTER(N, C) \
    GUMJS_DEFINE_CLASS_SETTER_AT_LEVEL (N, C, 0)
#define GUMJS_DEFINE_CLASS_METHOD(N, C) \
    GUMJS_DEFINE_CLASS_METHOD_AT_LEVEL (N, C, 0)

#define GUMJS_DEFINE_DIRECT_SUBCLASS_GETTER(N, C) \
    GUMJS_DEFINE_CLASS_GETTER_AT_LEVEL (N, C, 1)
#define GUMJS_DEFINE_DIRECT_SUBCLASS_SETTER(N, C) \
    GUMJS_DEFINE_CLASS_SETTER_AT_LEVEL (N, C, 1)
#define GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD(N, C) \
    GUMJS_DEFINE_CLASS_METHOD_AT_LEVEL (N, C, 1)

#endif
