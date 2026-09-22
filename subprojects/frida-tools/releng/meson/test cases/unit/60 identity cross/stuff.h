#ifdef EXTERNAL_BUILD
  #ifndef ARG_BUILD
    #error "External is build but arg_build is not set."
  #elif defined(ARG_HOST)
    #error "External is build but arg_host is set."
  #else
    #define GOT BUILD
  #endif
#endif

#ifdef EXTERNAL_HOST
  #ifndef ARG_HOST
    #error "External is host but arg_host is not set."
  #elif defined(ARG_BUILD)
    #error "External is host but arg_build is set."
  #else
    #define GOT HOST
  #endif
#endif

#if defined(EXTERNAL_BUILD) && defined(EXTERNAL_HOST)
  #error "Both external build and external host set."
#endif

#if !defined(EXTERNAL_BUILD) && !defined(EXTERNAL_HOST)
  #error "Neither external build nor external host is set."
#endif
