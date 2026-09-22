#ifndef __GUMPP_HPP__
#define __GUMPP_HPP__

#if !defined (GUMPP_STATIC) && defined (WIN32)
#  ifdef GUMPP_EXPORTS
#    define GUMPP_API __declspec(dllexport)
#  else
#    define GUMPP_API __declspec(dllimport)
#  endif
#else
#  define GUMPP_API
#endif

#define GUMPP_CAPI extern "C" GUMPP_API

#define GUMPP_MAX_BACKTRACE_DEPTH 16
#define GUMPP_MAX_PATH            260
#define GUMPP_MAX_SYMBOL_NAME     2048

#include <cstddef>
#include <vector>

namespace Gum
{
  struct InvocationContext;
  struct InvocationListener;
  struct CpuContext;
  struct ReturnAddressArray;

  struct Object
  {
    virtual ~Object () {}

    virtual void ref () = 0;
    virtual void unref () = 0;
    virtual void * get_handle () const = 0;
  };

  struct String : public Object
  {
    virtual const char * c_str () = 0;
    virtual size_t length () const = 0;
  };

  struct PtrArray : public Object
  {
    virtual int length () = 0;
    virtual void * nth (int n) = 0;
  };

  struct Interceptor : public Object
  {
    virtual bool attach (void * function_address, InvocationListener * listener, void * listener_function_data = 0) = 0;
    virtual void detach (InvocationListener * listener) = 0;

    virtual void replace (void * function_address, void * replacement_address, void * replacement_data = 0) = 0;
    virtual void revert (void * function_address) = 0;

    virtual void begin_transaction () = 0;
    virtual void end_transaction () = 0;

    virtual InvocationContext * get_current_invocation () = 0;

    virtual void ignore_current_thread () = 0;
    virtual void unignore_current_thread () = 0;

    virtual void ignore_other_threads () = 0;
    virtual void unignore_other_threads () = 0;
  };

  GUMPP_CAPI Interceptor * Interceptor_obtain (void);

  struct InvocationContext
  {
    virtual ~InvocationContext () {}

    virtual void * get_function () const = 0;

    template <typename T>
    T get_nth_argument (unsigned int n) const
    {
      return static_cast<T> (get_nth_argument_ptr (n));
    }
    virtual void * get_nth_argument_ptr (unsigned int n) const = 0;
    virtual void replace_nth_argument (unsigned int n, void * value) = 0;
    template <typename T>
    T get_return_value () const
    {
      return static_cast<T> (get_return_value_ptr ());
    }
    virtual void * get_return_value_ptr () const = 0;

    virtual unsigned int get_thread_id () const = 0;

    template <typename T>
    T * get_listener_thread_data () const
    {
      return static_cast<T *> (get_listener_thread_data_ptr (sizeof (T)));
    }
    virtual void * get_listener_thread_data_ptr (size_t required_size) const = 0;
    template <typename T>
    T * get_listener_function_data () const
    {
      return static_cast<T *> (get_listener_function_data_ptr ());
    }
    virtual void * get_listener_function_data_ptr () const = 0;
    template <typename T>
    T * get_listener_invocation_data () const
    {
      return static_cast<T *> (get_listener_invocation_data_ptr (sizeof (T)));
    }
    virtual void * get_listener_invocation_data_ptr (size_t required_size) const = 0;

    template <typename T>
    T * get_replacement_data () const
    {
      return static_cast<T *> (get_replacement_data_ptr ());
    }
    virtual void * get_replacement_data_ptr () const = 0;

    virtual CpuContext * get_cpu_context () const = 0;
  };

  struct InvocationListener
  {
    virtual ~InvocationListener () {}

    virtual void on_enter (InvocationContext * context) = 0;
    virtual void on_leave (InvocationContext * context) = 0;
  };

  struct Backtracer : public Object
  {
    virtual void generate (const CpuContext * cpu_context, ReturnAddressArray & return_addresses) const = 0;
  };

  GUMPP_CAPI Backtracer * Backtracer_make_accurate ();
  GUMPP_CAPI Backtracer * Backtracer_make_fuzzy ();

  typedef void * ReturnAddress;

  struct ReturnAddressArray
  {
    unsigned int len;
    ReturnAddress items[GUMPP_MAX_BACKTRACE_DEPTH];
  };

  struct ReturnAddressDetails
  {
    ReturnAddress address;
    char module_name[GUMPP_MAX_PATH + 1];
    char function_name[GUMPP_MAX_SYMBOL_NAME + 1];
    char file_name[GUMPP_MAX_PATH + 1];
    unsigned int line_number;
    unsigned int column;
  };

  GUMPP_CAPI bool ReturnAddressDetails_from_address (ReturnAddress address, ReturnAddressDetails & details);

  struct SanityChecker : public Object
  {
    virtual void enable_backtraces_for_blocks_of_all_sizes () = 0;
    virtual void enable_backtraces_for_blocks_of_size (unsigned int size) = 0;
    virtual void set_front_alignment_granularity (unsigned int granularity) = 0;

    virtual void begin (unsigned int flags) = 0;
    virtual bool end () = 0;
  };

  struct HeapApi
  {
    void * (* malloc) (size_t size);
    void * (* calloc) (size_t num, size_t size);
    void * (* realloc) (void * old_address, size_t new_size);
    void (* free) (void * address);

    /* for Microsoft's Debug CRT: */
    void * (* _malloc_dbg) (size_t size, int block_type, const char * filename, int linenumber);
    void * (* _calloc_dbg) (size_t num, size_t size, int block_type, const char * filename, int linenumber);
    void * (* _realloc_dbg) (void * old_address, size_t new_size, int block_type, const char * filename, int linenumber);
    void (* _free_dbg) (void * address, int block_type);
  };

  GUMPP_CAPI SanityChecker * SanityChecker_new (void);
  GUMPP_CAPI SanityChecker * SanityChecker_new_with_heap_api (const HeapApi * api);

  enum SanityCheckFlags
  {
    CHECK_INSTANCE_LEAKS  = (1 << 0),
    CHECK_BLOCK_LEAKS     = (1 << 1),
    CHECK_BOUNDS          = (1 << 2)
  };

  typedef unsigned long long Sample;

  struct Sampler : public Object
  {
    virtual Sample sample () const = 0;
  };

  struct CallCountSampler : public Sampler
  {
    virtual void add_function (void * function_address) = 0;
    virtual Sample peek_total_count () const = 0;
  };

  GUMPP_CAPI Sampler * BusyCycleSampler_new ();
  GUMPP_CAPI Sampler * CycleSampler_new ();
  GUMPP_CAPI Sampler * MallocCountSampler_new ();
  GUMPP_CAPI Sampler * WallClockSampler_new ();

  GUMPP_CAPI CallCountSampler * CallCountSampler_new (void * first_function, ...);
  GUMPP_CAPI CallCountSampler * CallCountSampler_new_by_name (const char * first_function_name, ...);

  struct FunctionMatchCallbacks
  {
    virtual ~FunctionMatchCallbacks () {}

    virtual bool match_should_include (const char * function_name) = 0;
  };

  struct InspectorCallbacks
  {
    virtual ~InspectorCallbacks () {}

    virtual void inspect_worst_case (InvocationContext * context, char * output_buf, unsigned int output_buf_len) = 0;
  };

  struct ProfileReport : public Object
  {
    virtual String * emit_xml () = 0;
  };

  struct Profiler : public Object
  {
    virtual void instrument_functions_matching (const char * match_str, Sampler * sampler, FunctionMatchCallbacks * match_callbacks = 0) = 0;
    virtual bool instrument_function (void * function_address, Sampler * sampler) = 0;
    virtual bool instrument_function_with_inspector (void * function_address, Sampler * sampler, InspectorCallbacks * inspector_callbacks) = 0;

    virtual ProfileReport * generate_report () = 0;
  };

  GUMPP_CAPI Profiler * Profiler_new (void);

  GUMPP_CAPI void * find_function_ptr (const char * str);
  GUMPP_CAPI PtrArray * find_matching_functions_array (const char * str);

  template <typename T> class RefPtr
  {
  public:
    explicit RefPtr (T * ptr_) : ptr (ptr_) {}
    explicit RefPtr (const RefPtr<T> & other) : ptr (other.ptr)
    {
      if (ptr)
        ptr->ref ();
    }

    template <class U> RefPtr (const RefPtr<U> & other) : ptr (other.operator->())
    {
      if (ptr)
        ptr->ref ();
    }

    RefPtr () : ptr (0) {}

    bool is_null () const
    {
      return ptr == 0 || ptr->get_handle () == 0;
    }

    RefPtr & operator = (const RefPtr & other)
    {
      RefPtr tmp (other);
      swap (*this, tmp);
      return *this;
    }

    RefPtr & operator = (T * other)
    {
      RefPtr tmp (other);
      swap (*this, tmp);
      return *this;
    }

    T * operator-> () const
    {
      return ptr;
    }

    T & operator* () const
    {
      return *ptr;
    }

    operator T * ()
    {
      return ptr;
    }

    static void swap (RefPtr & a, RefPtr & b)
    {
      T * tmp = a.ptr;
      a.ptr = b.ptr;
      b.ptr = tmp;
    }

    ~RefPtr ()
    {
      if (ptr)
        ptr->unref ();
    }

  private:
    T * ptr;
  };

  class SymbolUtil
  {
  public:
    static void * find_function (const char * name)
    {
      return find_function_ptr (name);
    }

    static std::vector<void *> find_matching_functions (const char * str)
    {
      RefPtr<PtrArray> functions = RefPtr<PtrArray> (find_matching_functions_array (str));
      std::vector<void *> result;
      for (int i = functions->length () - 1; i >= 0; i--)
        result.push_back (functions->nth (i));
      return result;
    }
  };
}

#endif
