[CCode (cheader_filename = "gum/gum.h", gir_namespace = "FridaGum", gir_version = "1.0")]
namespace Gum {
	public void init ();
	public void shutdown ();
	public void deinit ();

	public void init_embedded ();
	public void deinit_embedded ();

	public void prepare_to_fork ();
	public void recover_from_fork_in_parent ();
	public void recover_from_fork_in_child ();

	public void * sign_code_pointer (void * value);
	public void * strip_code_pointer (void * value);
	public Gum.Address sign_code_address (Gum.Address value);
	public Gum.Address strip_code_address (Gum.Address value);
	public Gum.PtrauthSupport query_ptrauth_support ();

	public uint query_page_size ();
	public bool query_is_rwx_supported ();
	public Gum.RwxSupport query_rwx_support ();

	public void ensure_code_readable (void * address, size_t size);

	public void mprotect (void * address, size_t size, Gum.PageProtection prot);
	public bool try_mprotect (void * address, size_t size, Gum.PageProtection prot);

	public void clear_cache (void * address, size_t size);

	public uint peek_private_memory_usage ();

	public void * malloc (size_t size);
	public void * malloc0 (size_t size);
	public void * calloc (size_t count, size_t size);
	public void * realloc (void * mem, size_t size);
	public void * memalign (size_t alignment, size_t size);
	public void * memdup (void * mem, size_t byte_size);
	public void free (void * mem);

	public void * memory_allocate (void * address, size_t size, size_t alignment, Gum.PageProtection prot);
	public void * memory_allocate_near (Gum.AddressSpec spec, size_t size, size_t alignment, Gum.PageProtection prot);
	public bool memory_free (void * address, size_t size);
	public bool memory_release (void * address, size_t size);

	public errordomain Error {
		FAILED,
		NOT_FOUND,
		EXISTS,
		PERMISSION_DENIED,
		INVALID_ARGUMENT,
		NOT_SUPPORTED,
		INVALID_DATA,
	}

	[CCode (cprefix = "GUM_TEARDOWN_REQUIREMENT_")]
	public enum TeardownRequirement {
		FULL,
		MINIMAL
	}

	[CCode (cprefix = "GUM_CODE_SIGNING_")]
	public enum CodeSigningPolicy {
		OPTIONAL,
		REQUIRED
	}

	public enum ModifyThreadFlags {
		NONE,
		ABORT_SAFELY,
	}

	public enum ThreadFlags {
		NAME,
		STATE,
		CPU_CONTEXT,
		ENTRYPOINT_ROUTINE,
		ENTRYPOINT_PARAMETER,

		NONE,
		ALL,
	}

	public enum OS {
		NONE,
		WINDOWS,
		MACOS,
		LINUX,
		IOS,
		WATCHOS,
		TVOS,
		ANDROID,
		FREEBSD,
		QNX,
	}

	[CCode (cprefix = "GUM_CALL_")]
	public enum CallingConvention {
		CAPI,
		SYSAPI
	}

	public const CpuType NATIVE_CPU;

	[CCode (cprefix = "GUM_CPU_")]
	public enum CpuType {
		INVALID,
		IA32,
		AMD64,
		ARM,
		ARM64,
		MIPS,
	}

	public struct Argument {
		public Gum.ArgType type;
		public Gum.ArgValue value;
	}

	[CCode (cprefix = "GUM_ARG_")]
	public enum ArgType {
		ADDRESS,
		REGISTER
	}

	public struct ArgValue {
		Gum.Address address;
		int reg;
	}

	[CCode (cprefix = "GUM_PTRAUTH_")]
	public enum PtrauthSupport {
		INVALID,
		UNSUPPORTED,
		SUPPORTED
	}

	[CCode (has_target = false)]
	public delegate Gum.Address PtrauthSignFunc (Gum.Address val);

	[CCode (cprefix = "GUM_RWX_")]
	public enum RwxSupport {
		NONE,
		ALLOCATIONS_ONLY,
		FULL
	}

	public class Interceptor : GLib.Object {
		public static Interceptor obtain ();

		public Gum.AttachReturn attach (void * target, Gum.InvocationListener listener,
			Gum.AttachOptions? options = null);
		public void detach (Gum.InvocationListener listener);

		public Gum.ReplaceReturn replace (void * function_address, void * replacement_function,
			out void * original_function = null, Gum.ReplaceOptions? options = null);
		public void revert (void * target);
		public void set_default_options (Gum.InterceptorOptions options);

		public void begin_transaction ();
		public void end_transaction ();
		public bool flush ();

		public static unowned Gum.InvocationContext get_current_invocation ();

		public void ignore_current_thread ();
		public void unignore_current_thread ();

		public void ignore_other_threads ();
		public void unignore_other_threads ();

		public void with_lock_held (Gum.Interceptor.LockedFunc func);
		public bool is_locked ();

		public delegate void LockedFunc ();
	}

	[CCode (cprefix = "GUM_ATTACH_")]
	public enum AttachReturn {
		OK		  =  0,
		WRONG_SIGNATURE	  = -1,
		ALREADY_ATTACHED  = -2
	}

	[CCode (has_type_id = false)]
	public struct AttachOptions {
		public Gum.InterceptorOptions instrumentation;
		public void * listener_function_data;
		public Gum.InvocationIgnorability ignorability;
	}

	[CCode (cprefix = "GUM_INVOCATION_")]
	public enum InvocationIgnorability {
		IGNORABLE,
		UNIGNORABLE,
	}

	[CCode (cprefix = "GUM_REPLACE_")]
	public enum ReplaceReturn {
		OK		  =  0,
		WRONG_SIGNATURE	  = -1,
		ALREADY_REPLACED  = -2
	}

	[CCode (has_type_id = false)]
	public struct ReplaceOptions {
		public Gum.InterceptorOptions instrumentation;
		public void * replacement_data;
	}

	[CCode (has_type_id = false)]
	public struct InterceptorOptions {
		public int scratch_register;
		public Gum.InterceptorScenario scenario;
		public Gum.RelocationPolicy relocation_policy;
		[CCode (delegate_target_cname = "write_redirect_data")]
		public unowned Gum.WriteRedirectFunc? write_redirect;
		public uint redirect_space_hint;
	}

	[CCode (has_type_id = false)]
	public struct RedirectWriteDetails {
		public void * writer;
		public void * target;
		public int scratch_register;
		public uint capacity;
	}

	[CCode (cprefix = "GUM_REDIRECT_")]
	public enum RedirectWriteResult {
		WRITTEN,
		DECLINED,
	}

	public delegate Gum.RedirectWriteResult WriteRedirectFunc (Gum.RedirectWriteDetails details);

	[CCode (cprefix = "GUM_INTERCEPTOR_SCENARIO_")]
	public enum InterceptorScenario {
		ONLINE,
		OFFLINE,
	}

	[CCode (cprefix = "GUM_RELOCATION_")]
	public enum RelocationPolicy {
		CHECKED,
		UNCHECKED,
		FORCED,
	}

	[CCode (type_cname = "GumInvocationListenerInterface")]
	public interface InvocationListener : GLib.Object {
		public virtual void on_enter (Gum.InvocationContext context);
		public virtual void on_leave (Gum.InvocationContext context);
	}

	[Compact]
	public class InvocationContext {
		public void * function;
		public CpuContext * cpu_context;
		public int system_error;

		public void * backend;

		public Gum.PointCut get_point_cut ();

		public void * get_nth_argument (uint n);
		public void replace_nth_argument (uint n, void * val);
		public void * get_return_value ();
		public void replace_return_value (void * val);

		public void * get_return_address ();

		public uint get_thread_id ();
		public uint get_depth ();

		public void * get_listener_thread_data (size_t required_size);
		public void * get_listener_function_data ();
		public void * get_listener_invocation_data (size_t required_size);

		public void * get_replacement_data ();
	}

	[CCode (cprefix = "GUM_POINT_")]
	public enum PointCut {
		ENTER,
		LEAVE
	}

	[CCode (type_cname = "GumBacktracerInterface")]
	public interface Backtracer : GLib.Object {
		public static Backtracer? make_accurate ();
		public static Backtracer? make_fuzzy ();

		public abstract void generate (Gum.CpuContext * cpu_context, out Gum.ReturnAddressArray return_addresses);
	}

	public struct ReturnAddressArray {
		public uint len;
		public void * items[16];
	}

	public class MemoryAccessMonitor : GLib.Object {
		public MemoryAccessMonitor ();

		public void enable (Gum.MemoryRange range, Gum.MemoryAccessNotify func);
		public void disable ();
	}

	public delegate void MemoryAccessNotify (Gum.MemoryAccessMonitor monitor, Gum.MemoryAccessDetails details);

	public struct MemoryAccessDetails {
		public Gum.MemoryOperation operation;
		public void * from;
		public void * address;

		public uint page_index;
		public uint pages_completed;
		public uint pages_remaining;
	}

	[CCode (cprefix = "GUM_MEMOP_")]
	public enum MemoryOperation {
		READ,
		WRITE,
		EXECUTE
	}

	public class Stalker : GLib.Object {
		public static bool is_supported ();

		public Stalker ();

		public void exclude (Gum.MemoryRange range);

		public int get_trust_threshold ();
		public void set_trust_threshold (int trust_threshold);

		public void flush ();
		public void stop ();
		public bool garbage_collect ();

		public void follow_me (Gum.EventSink sink);
		public void unfollow_me ();
		public bool is_following_me ();

		public void follow (Gum.ThreadId thread_id, Gum.EventSink sink);
		public void unfollow (Gum.ThreadId thread_id);

		public void activate (void * target);
		public void deactivate ();

		public Gum.Stalker.ProbeId add_call_probe (void * target_address, owned Gum.Stalker.CallProbeCallback callback);
		public void remove_call_probe (Gum.Stalker.ProbeId id);

		public struct ProbeId : uint {
		}

		public delegate void CallProbeCallback (Gum.CallSite site);
	}

	[CCode (type_cname = "GumEventSinkInterface")]
	public interface EventSink : GLib.Object {
		public abstract Gum.EventType query_mask ();
		public abstract void process (void * opaque_event);
	}

	public struct CallSite {
		public void * block_address;
		public void * stack_data;
		public CpuContext * cpu_context;
	}

	namespace Process {
		public Gum.TeardownRequirement get_teardown_requirement ();
		public void set_teardown_requirement (Gum.TeardownRequirement requirement);
		public Gum.CodeSigningPolicy get_code_signing_policy ();
		public void set_code_signing_policy (Gum.CodeSigningPolicy policy);
		public bool is_debugger_attached ();
		public Gum.ProcessId get_id ();
		public Gum.ThreadId get_current_thread_id ();
		public bool has_thread (Gum.ThreadId thread_id);
		public Gum.ThreadDetails? find_thread_by_id (Gum.ThreadId thread_id, Gum.ThreadFlags flags = ALL);
		public bool modify_thread (Gum.ThreadId thread_id, Gum.ModifyThreadFunc func, Gum.ModifyThreadFlags flags = NONE);
		public void enumerate_threads (Gum.FoundThreadFunc func, Gum.ThreadFlags flags = ALL);
		public unowned Module get_main_module ();
		public unowned Module? get_libc_module ();
		public Module? find_module_by_name (string name);
		public Module? find_module_by_address (Gum.Address address);
		public void enumerate_modules (Gum.FoundModuleFunc func);
		public void enumerate_ranges (Gum.PageProtection prot, Gum.FoundRangeFunc func);
		public Gum.HeapApiList find_heap_apis ();
	}

	[Compact]
	[CCode (copy_function = "gum_heap_api_list_copy", free_function = "gum_heap_api_list_free")]
	public class HeapApiList {
		public unowned HeapApi? get_nth (uint n);
		public void add (Gum.HeapApi api);

		public uint len;
	}

	public struct HeapApi {
		public void * malloc;
		public void * calloc;
		public void * realloc;
		public void * free;

		// For Microsoft's Debug CRT:
		public void * _malloc_dbg;
		public void * _calloc_dbg;
		public void * _realloc_dbg;
		public void * _free_dbg;
		public void * _CrtReportBlockType;
	}

	namespace Thread {
		public uint try_get_ranges (Gum.MemoryRange[] ranges);
		public bool suspend (Gum.ThreadId thread_id) throws Gum.Error;
		public bool resume (Gum.ThreadId thread_id) throws Gum.Error;
	}

	public interface Module : GLib.Object {
		public string name { get; }
		public string? version { get; }
		public string path { get; }
		public Gum.MemoryRange? range { get; }

		public static Module load (string module_name) throws Gum.Error;

		public void ensure_initialized ();
		public void enumerate_imports (Gum.FoundImportFunc func);
		public void enumerate_exports (Gum.FoundExportFunc func);
		public void enumerate_symbols (Gum.FoundSymbolFunc func);
		public void enumerate_ranges (Gum.PageProtection prot, Gum.FoundRangeFunc func);
		public void enumerate_sections (Gum.FoundSectionFunc func);
		public void enumerate_dependencies (Gum.FoundDependencyFunc func);
		public Gum.Address find_export_by_name (string symbol_name);
		public static Gum.Address find_global_export_by_name (string symbol_name);
		public Gum.Address find_symbol_by_name (string symbol_name);
	}

	public class ModuleMap : GLib.Object {
		public ModuleMap ();

		public unowned Gum.Module? find (Gum.Address address);

		public void update ();
	}

	namespace Memory {
		public bool is_readable (void * address, size_t len);
		public bool query_protection (void * address, out Gum.PageProtection prot);
		public uint8[] read (Address address, size_t len);
		public bool write (Address address, uint8[] bytes);
		public bool patch_code (void * address, size_t size, Gum.Memory.PatchApplyFunc apply);
		public bool mark_code (void * address, size_t size);

		public void scan (Gum.MemoryRange range, Gum.MatchPattern pattern, Gum.Memory.ScanMatchFunc func);
		public GLib.Array<Gum.PointerMatch> find_pointers (Gum.MemoryRange[] ranges, size_t[] values, size_t mask);

		public void * allocate (void * address, size_t size, size_t alignment, Gum.PageProtection prot);
		public bool free (void * address, size_t size);
		public bool release (void * address, size_t size);
		public bool commit (void * address, size_t size, Gum.PageProtection prot);
		public bool decommit (void * address, size_t size);

		public delegate void PatchApplyFunc (void * mem);
		public delegate bool ScanMatchFunc (Address address, size_t size);
	}

	namespace InternalHeap {
		public void ref ();
		public void unref ();
	}

	namespace Cloak {
		public void add_thread (Gum.ThreadId id);
		public void remove_thread (Gum.ThreadId id);
		public bool has_thread (Gum.ThreadId id);
		public void enumerate_threads (Gum.Cloak.FoundThreadFunc func);

		public void add_range (Gum.MemoryRange range);
		public void remove_range (Gum.MemoryRange range);
		public GLib.Array<Gum.MemoryRange>? clip_range (Gum.MemoryRange range);
		public void enumerate_ranges (Gum.Cloak.FoundRangeFunc func);

		public void add_file_descriptor (int fd);
		public void remove_file_descriptor (int fd);
		public bool has_file_descriptor (int fd);
		public void enumerate_file_descriptors (Gum.Cloak.FoundFDFunc func);

		public void with_lock_held (Gum.Cloak.LockedFunc func);
		public bool is_locked ();

		public delegate bool FoundThreadFunc (Gum.ThreadId id);
		public delegate bool FoundRangeFunc (Gum.MemoryRange range);
		public delegate bool FoundFDFunc (int fd);
		public delegate void LockedFunc ();
	}

	public struct CpuContext {
	}

	public struct IA32CpuContext {
		public uint32 eip;

		public uint32 edi;
		public uint32 esi;
		public uint32 ebp;
		public uint32 esp;
		public uint32 ebx;
		public uint32 edx;
		public uint32 ecx;
		public uint32 eax;

		public Gum.X86VectorReg * xmm;
	}

	public struct X86VectorReg {
		public uint8 q[16];
		public double d[2];
		public float s[4];
	}

	public struct X64CpuContext {
		public uint64 rip;

		public uint64 r15;
		public uint64 r14;
		public uint64 r13;
		public uint64 r12;
		public uint64 r11;
		public uint64 r10;
		public uint64 r9;
		public uint64 r8;

		public uint64 rdi;
		public uint64 rsi;
		public uint64 rbp;
		public uint64 rsp;
		public uint64 rbx;
		public uint64 rdx;
		public uint64 rcx;
		public uint64 rax;

		public Gum.X86VectorReg * xmm;
	}

	public struct ArmCpuContext {
		public uint32 cpsr;
		public uint32 pc;
		public uint32 sp;

		public uint32 r8;
		public uint32 r9;
		public uint32 r10;
		public uint32 r11;
		public uint32 r12;

		public uint32 r[8];
		public uint32 lr;
	}

	public struct Arm64VectorReg {
		public uint8 q[16];
		public double d;
		public float s;
		public uint16 h;
		public uint8 b;
	}

	public struct Arm64CpuContext {
		public uint64 pc;
		public uint64 sp;
		public uint64 nzcv;

		public uint64 x[29];
		public uint64 fp;
		public uint64 lr;

		public Gum.Arm64VectorReg v[32];
	}

	public struct MipsCpuContext {
		public uint32 pc;

		public uint32 gp;
		public uint32 sp;
		public uint32 fp;
		public uint32 ra;

		public uint32 hi;
		public uint32 lo;

		public uint32 at;

		public uint32 v0;
		public uint32 v1;

		public uint32 a0;
		public uint32 a1;
		public uint32 a2;
		public uint32 a3;

		public uint32 t0;
		public uint32 t1;
		public uint32 t2;
		public uint32 t3;
		public uint32 t4;
		public uint32 t5;
		public uint32 t6;
		public uint32 t7;
		public uint32 t8;
		public uint32 t9;

		public uint32 s0;
		public uint32 s1;
		public uint32 s2;
		public uint32 s3;
		public uint32 s4;
		public uint32 s5;
		public uint32 s6;
		public uint32 s7;

		public uint32 k0;
		public uint32 k1;
	}

	[CCode (cprefix = "GUM_SCENARIO_")]
	public enum RelocationScenario {
		OFFLINE,
		ONLINE
	}

	public delegate void ModifyThreadFunc (Gum.ThreadId thread_id, CpuContext * cpu_context);
	public delegate bool FoundThreadFunc (Gum.ThreadDetails details);
	public delegate bool FoundModuleFunc (Gum.Module module);
	public delegate bool FoundRangeFunc (Gum.RangeDetails details);
	public delegate bool FoundImportFunc (Gum.ImportDetails details);
	public delegate bool FoundExportFunc (Gum.ExportDetails details);
	public delegate bool FoundSymbolFunc (Gum.SymbolDetails details);
	public delegate bool FoundSectionFunc (Gum.SectionDetails details);
	public delegate bool FoundDependencyFunc (Gum.DependencyDetails details);

	public struct ProcessId : uint {
	}

	public struct ThreadId : size_t {
	}

	[CCode (cprefix = "GUM_THREAD_")]
	public enum ThreadState {
		RUNNING = 1,
		STOPPED,
		WAITING,
		UNINTERRUPTIBLE,
		HALTED
	}

	public struct ThreadEntrypoint {
		Gum.Address routine;
		Gum.Address parameter;
	}

	public struct ThreadDetails {
		public Gum.ThreadFlags flags;
		public Gum.ThreadId id;
		public Gum.ThreadState state;
		public Gum.CpuContext cpu_context;
		public Gum.ThreadEntrypoint entrypoint;
	}

	[CCode (cprefix = "GUM_IMPORT_")]
	public enum ImportType {
		FUNCTION = 1,
		VARIABLE
	}

	public struct ImportDetails {
		public Gum.ImportType type;
		public string name;
		public string module;
		public Gum.Address address;
		public Gum.Address slot;
	}

	[CCode (cprefix = "GUM_EXPORT_")]
	public enum ExportType {
		FUNCTION = 1,
		VARIABLE
	}

	public struct ExportDetails {
		public Gum.ExportType type;
		public string name;
		public Gum.Address address;
		public ssize_t size;
	}

	[CCode (cprefix = "GUM_SYMBOL_")]
	public enum SymbolType {
		UNKNOWN,
		UNDEFINED,
		ABSOLUTE,
		SECTION,
		PREBOUND_UNDEFINED,
		INDIRECT
	}

	public struct SymbolDetails {
		public bool is_global;
		public Gum.SymbolType type;
		public Gum.SymbolSection? section;
		public string name;
		public Gum.Address address;
		public ssize_t size;
	}

	public struct SymbolSection {
		public string id;
		public Gum.PageProtection protection;
	}

	public struct RangeDetails {
		public Gum.MemoryRange? range;
		public Gum.PageProtection protection;
		public Gum.FileMapping? file;
	}

	public struct FileMapping {
		public string path;
		public uint64 offset;
	}

	public struct SectionDetails {
		public string id;
		public string name;
		public Gum.Address address;
		public ssize_t size;
	}

	[CCode (cprefix = "GUM_DEPENDENCY_")]
	public enum DependencyType {
		REGULAR,
		WEAK,
		REEXPORT,
		UPWARD,
	}

	public struct DependencyDetails {
		public string name;
		public Gum.DependencyType type;
	}

	public struct Address : uint64 {
		public static Gum.Address from_pointer (void * p) {
			return (Gum.Address) (uintptr) p;
		}
	}

	public struct AddressSpec {
		public void * near_address;
		public size_t max_distance;
	}

	public struct MemoryRange {
		public Address base_address;
		public size_t size;
	}

	public struct PointerMatch {
		public Address address;
		public size_t value;
	}

	[Compact]
	[CCode (free_function = "gum_match_pattern_unref")]
	public class MatchPattern {
		public MatchPattern.from_string (string match_str);
	}

	[Flags]
	[CCode (cprefix = "GUM_PAGE_")]
	public enum PageProtection {
		NO_ACCESS = 0,
		READ      = (1 << 0),
		WRITE     = (1 << 1),
		EXECUTE   = (1 << 2)
	}

	public class Exceptor : GLib.Object {
		public static void set_mode (Gum.ExceptorMode mode);
		public static Exceptor obtain ();
	}

	public enum ExceptorMode {
		FULL,
		HANDLER_ONLY,
		OFF,
	}

	public class ModuleRegistry : GLib.Object {
		public static void set_rtld_notifier_offsets ([CCode (array_length_type = "guint")] uint[] offsets);
	}

	public class UnwindBroker : GLib.Object {
		public static UnwindBroker obtain ();
		public void add_sections_provider (Gum.UnwindSectionsProvider provider);
		public void remove_sections_provider (Gum.UnwindSectionsProvider provider);
		public void add_pc_translator (Gum.UnwindPcTranslator translator);
		public void remove_pc_translator (Gum.UnwindPcTranslator translator);
	}

	[CCode (type_cname = "GumUnwindSectionsProviderInterface")]
	public interface UnwindSectionsProvider : GLib.Object {
		public abstract unowned Gum.MemoryRange? range { get; }
		public abstract bool fill (Gum.Address address, void * info);
	}

	[CCode (type_cname = "GumUnwindPcTranslatorInterface")]
	public interface UnwindPcTranslator : GLib.Object {
		public abstract Gum.Address translate (Gum.Address code_address);
		public virtual bool install_resume_context (void * unwind_context, Gum.Address real_resume_ip);
	}

	public bool symbol_details_from_address (void * address, out Gum.DebugSymbolDetails details);
	public string symbol_name_from_address (void * address);

	public void * find_function (string name);
	public GLib.Array<void *> find_functions_named (string name);
	public GLib.Array<void *> find_functions_matching (string str);

	[CCode (has_copy_function = false, has_destroy_function = false)]
	public struct DebugSymbolDetails {
		public Gum.Address address;
		public unowned string module_name;
		public unowned string symbol_name;
		public unowned string file_name;
		public uint line_number;
	}

	[CCode (cprefix = "GUM_")]
	public enum EventType {
		NOTHING	= 0,
		CALL	= (1 << 0),
		RET	= (1 << 1),
		EXEC	= (1 << 2)
	}

	[Compact]
	public struct AnyEvent {
		public EventType type;
	}

	[Compact]
	public struct CallEvent {
		public EventType type;

		public void * location;
		public void * target;
		public int depth;
	}

	[Compact]
	public struct RetEvent {
		public EventType type;

		public void * location;
		public void * target;
		public int depth;
	}

	[Compact]
	public struct ExecEvent {
		public EventType type;

		public void * location;
	}

	public class ElfModule : GLib.Object {
		public ElfModule.from_file (string path) throws Gum.Error;
		public ElfModule.from_blob (GLib.Bytes blob) throws Gum.Error;
		public ElfModule.from_memory (string path, Gum.Address base_address) throws Gum.Error;

		public bool load () throws Gum.Error;

		public Gum.ElfType etype { get; }
		public uint pointer_size { get; }
		public GLib.ByteOrder byte_order { get; }
		public Gum.ElfOSABI os_abi { get; }
		public uint8 os_abi_version { get; }
		public Gum.ElfMachine machine { get; }
		public Gum.Address base_address { get; }
		public Gum.Address preferred_address { get; }
		public uint64 mapped_size { get; }
		public Gum.Address entrypoint { get; }
		public string interpreter { get; }
		public unowned string? source_path { get; }
		public unowned GLib.Bytes? source_blob { get; }
		public Gum.ElfSourceMode source_mode { get; }

		[CCode (array_length_type = "size_t")]
		public unowned uint8[] get_file_data ();

		public void enumerate_segments (Gum.FoundElfSegmentFunc func);
		public void enumerate_sections (Gum.FoundElfSectionFunc func);
		public void enumerate_relocations (Gum.FoundElfRelocationFunc func);
		public void enumerate_dynamic_entries (Gum.FoundElfDynamicEntryFunc func);
		public void enumerate_dependencies (Gum.FoundDependencyFunc func);
		public void enumerate_imports (Gum.FoundImportFunc func);
		public void enumerate_exports (Gum.FoundExportFunc func);
		public void enumerate_dynamic_symbols (Gum.FoundElfSymbolFunc func);
		public void enumerate_symbols (Gum.FoundElfSymbolFunc func);

		public Gum.Address translate_to_offline (Gum.Address online_address);
		public Gum.Address translate_to_online (Gum.Address offline_address);
	}

	[CCode (cprefix = "GUM_ELF_")]
	public enum ElfType {
		NONE,
		REL,
		EXEC,
		DYN,
		CORE,
	}

	[CCode (cprefix = "GUM_ELF_OS_")]
	public enum ElfOSABI {
		SYSV,
		HPUX,
		NETBSD,
		LINUX,
		SOLARIS,
		AIX,
		IRIX,
		FREEBSD,
		TRU64,
		MODESTO,
		OPENBSD,
		ARM_AEABI,
		ARM,
		STANDALONE,
	}

	public enum ElfMachine {
		NONE,

		M32,
		SPARC,
		@386,
		68K,
		88K,
		IAMCU,
		@860,
		MIPS,
		S370,
		MIPS_RS3_LE,

		PARISC,

		VPP500,
		SPARC32PLUS,
		@960,
		PPC,
		PPC64,
		S390,
		SPU,

		V800,
		FR20,
		RH32,
		RCE,
		ARM,
		FAKE_ALPHA,
		SH,
		SPARCV9,
		TRICORE,
		ARC,
		H8_300,
		H8_300H,
		H8S,
		H8_500,
		IA_64,
		MIPS_X,
		COLDFIRE,
		68HC12,
		MMA,
		PCP,
		NCPU,
		NDR1,
		STARCORE,
		ME16,
		ST100,
		TINYJ,
		X86_64,
		PDSP,
		PDP10,
		PDP11,
		FX66,
		ST9PLUS,
		ST7,
		68HC16,
		68HC11,
		68HC08,
		68HC05,
		SVX,
		ST19,
		VAX,
		CRIS,
		JAVELIN,
		FIREPATH,
		ZSP,
		MMIX,
		HUANY,
		PRISM,
		AVR,
		FR30,
		D10V,
		D30V,
		V850,
		M32R,
		MN10300,
		MN10200,
		PJ,
		OPENRISC,
		ARC_COMPACT,
		XTENSA,
		VIDEOCORE,
		TMM_GPP,
		NS32K,
		TPC,
		SNP1K,
		ST200,
		IP2K,
		MAX,
		CR,
		F2MC16,
		MSP430,
		BLACKFIN,
		SE_C33,
		SEP,
		ARCA,
		UNICORE,
		EXCESS,
		DXP,
		ALTERA_NIOS2,
		CRX,
		XGATE,
		C166,
		M16C,
		DSPIC30F,
		CE,
		M32C,

		TSK3000,
		RS08,
		SHARC,
		ECOG2,
		SCORE7,
		DSP24,
		VIDEOCORE3,
		LATTICEMICO32,
		SE_C17,
		TI_C6000,
		TI_C2000,
		TI_C5500,
		TI_ARP32,
		TI_PRU,

		MMDSP_PLUS,
		CYPRESS_M8C,
		R32C,
		TRIMEDIA,
		QDSP6,
		@8051,
		STXP7X,
		NDS32,
		ECOG1X,
		MAXQ30,
		XIMO16,
		MANIK,
		CRAYNV2,
		RX,
		METAG,
		MCST_ELBRUS,
		ECOG16,
		CR16,
		ETPU,
		SLE9X,
		L10M,
		K10M,

		AARCH64,

		AVR32,
		STM8,
		TILE64,
		TILEPRO,
		MICROBLAZE,
		CUDA,
		TILEGX,
		CLOUDSHIELD,
		COREA_1ST,
		COREA_2ND,
		ARCV2,
		OPEN8,
		RL78,
		VIDEOCORE5,
		78KOR,
		@56800EX,
		BA1,
		BA2,
		XCORE,
		MCHP_PIC,

		KM32,
		KMX32,
		EMX16,
		EMX8,
		KVARC,
		CDP,
		COGE,
		COOL,
		NORC,
		CSR_KALIMBA,
		Z80,
		VISIUM,
		FT32,
		MOXIE,
		AMDGPU,

		RISCV,

		BPF,

		CSKY,

		ALPHA,
	}

	public enum ElfSourceMode {
		OFFLINE,
		ONLINE,
	}

	public delegate bool FoundElfSegmentFunc (Gum.ElfSegmentDetails details);
	public delegate bool FoundElfSectionFunc (Gum.ElfSectionDetails details);
	public delegate bool FoundElfRelocationFunc (Gum.ElfRelocationDetails details);
	public delegate bool FoundElfDynamicEntryFunc (Gum.ElfDynamicEntryDetails details);
	public delegate bool FoundElfSymbolFunc (Gum.ElfSymbolDetails details);

	public struct ElfSegmentDetails {
		public Gum.Address vm_address;
		public uint64 vm_size;
		public uint64 file_offset;
		public uint64 file_size;
		public Gum.PageProtection protection;
	}

	public class ElfSectionDetails {
		public string id;
		public string name;
		public Gum.ElfSectionType type;
		public uint64 flags;
		public Gum.Address address;
		public uint64 offset;
		public size_t size;
		public uint32 link;
		public uint32 info;
		public uint64 alignment;
		public uint64 entry_size;
		public Gum.PageProtection protection;
	}

	[CCode (has_copy_function = false, has_destroy_function = false)]
	public struct ElfRelocationDetails {
		public Gum.Address address;
		public uint32 type;
		public unowned Gum.ElfSymbolDetails? symbol;
		public int64 addend;
		public unowned Gum.ElfSectionDetails parent;
	}

	public struct ElfDynamicEntryDetails {
		public Gum.ElfDynamicTag tag;
		public uint64 val;
	}

	[Compact]
	public class ElfSymbolDetails {
		public string name;
		public Gum.Address address;
		public size_t size;
		public Gum.ElfSymbolType type;
		public Gum.ElfSymbolBind bind;
		public Gum.ElfSectionDetails? section;

		public ElfSymbolDetails copy ();
	}

	[CCode (cprefix = "GUM_ELF_SECTION_")]
	public enum ElfSectionType {
		NULL,
		PROGBITS,
		SYMTAB,
		STRTAB,
		RELA,
		HASH,
		DYNAMIC,
		NOTE,
		NOBITS,
		REL,
		SHLIB,
		DYNSYM,
		INIT_ARRAY,
		FINI_ARRAY,
		PREINIT_ARRAY,
		GROUP,
		SYMTAB_SHNDX,
		RELR,
		NUM,
		GNU_ATTRIBUTES,
		GNU_HASH,
		GNU_LIBLIST,
		CHECKSUM,
		SUNW_MOVE,
		SUNW_COMDAT,
		SUNW_SYMINFO,
		GNU_VERDEF,
		GNU_VERNEED,
		GNU_VERSYM,
	}

	[CCode (cprefix = "GUM_ELF_DYNAMIC_")]
	public enum ElfDynamicTag {
		NULL,
		NEEDED,
		PLTRELSZ,
		PLTGOT,
		HASH,
		STRTAB,
		SYMTAB,
		RELA,
		RELASZ,
		RELAENT,
		STRSZ,
		SYMENT,
		INIT,
		FINI,
		SONAME,
		RPATH,
		SYMBOLIC,
		REL,
		RELSZ,
		RELENT,
		PLTREL,
		DEBUG,
		TEXTREL,
		JMPREL,
		BIND_NOW,
		INIT_ARRAY,
		FINI_ARRAY,
		INIT_ARRAYSZ,
		FINI_ARRAYSZ,
		RUNPATH,
		FLAGS,
		ENCODING,
		PREINIT_ARRAY,
		PREINIT_ARRAYSZ,
		MAXPOSTAGS,
		LOOS,
		SUNW_AUXILIARY,
		SUNW_RTLDINF,
		SUNW_FILTER,
		SUNW_CAP,
		SUNW_ASLR,
		HIOS,

		VALRNGLO,
		GNU_PRELINKED,
		GNU_CONFLICTSZ,
		GNU_LIBLISTSZ,
		CHECKSUM,
		PLTPADSZ,
		MOVEENT,
		MOVESZ,
		FEATURE,
		FEATURE_1,
		POSFLAG_1,

		SYMINSZ,
		SYMINENT,
		VALRNGHI,

		ADDRRNGLO,
		GNU_HASH,
		TLSDESC_PLT,
		TLSDESC_GOT,
		GNU_CONFLICT,
		GNU_LIBLIST,
		CONFIG,
		DEPAUDIT,
		AUDIT,
		PLTPAD,
		MOVETAB,
		SYMINFO,
		ADDRRNGHI,

		VERSYM,
		RELACOUNT,
		RELCOUNT,
		FLAGS_1,
		VERDEF,
		VERDEFNUM,
		VERNEED,
		VERNEEDNUM,

		LOPROC,

		ARM_SYMTABSZ,
		ARM_PREEMPTMAP,

		SPARC_REGISTER,
		DEPRECATED_SPARC_REGISTER,

		MIPS_RLD_VERSION,
		MIPS_TIME_STAMP,
		MIPS_ICHECKSUM,
		MIPS_IVERSION,
		MIPS_FLAGS,
		MIPS_BASE_ADDRESS,
		MIPS_CONFLICT,
		MIPS_LIBLIST,
		MIPS_LOCAL_GOTNO,
		MIPS_CONFLICTNO,
		MIPS_LIBLISTNO,
		MIPS_SYMTABNO,
		MIPS_UNREFEXTNO,
		MIPS_GOTSYM,
		MIPS_HIPAGENO,
		MIPS_RLD_MAP,
		MIPS_DELTA_CLASS,
		MIPS_DELTA_CLASS_NO,
		MIPS_DELTA_INSTANCE,
		MIPS_DELTA_INSTANCE_NO,
		MIPS_DELTA_RELOC,
		MIPS_DELTA_RELOC_NO,
		MIPS_DELTA_SYM,
		MIPS_DELTA_SYM_NO,
		MIPS_DELTA_CLASSSYM,
		MIPS_DELTA_CLASSSYM_NO,
		MIPS_CXX_FLAGS,
		MIPS_PIXIE_INIT,
		MIPS_SYMBOL_LIB,
		MIPS_LOCALPAGE_GOTIDX,
		MIPS_LOCAL_GOTIDX,
		MIPS_HIDDEN_GOTIDX,
		MIPS_PROTECTED_GOTIDX,
		MIPS_OPTIONS,
		MIPS_INTERFACE,
		MIPS_DYNSTR_ALIGN,
		MIPS_INTERFACE_SIZE,
		MIPS_RLD_TEXT_RESOLVE_ADDR,
		MIPS_PERF_SUFFIX,
		MIPS_COMPACT_SIZE,
		MIPS_GP_VALUE,
		MIPS_AUX_DYNAMIC,
		MIPS_PLTGOT,
		MIPS_RLD_OBJ_UPDATE,
		MIPS_RWPLT,
		MIPS_RLD_MAP_REL,

		PPC_GOT,
		PPC_TLSOPT,

		PPC64_GLINK,
		PPC64_OPD,
		PPC64_OPDSZ,
		PPC64_TLSOPT,

		AUXILIARY,
		USED,
		FILTER,
		HIPROC,
	}

	[CCode (cprefix = "GUM_ELF_SYMBOL_")]
	public enum ElfSymbolType {
		NOTYPE,
		OBJECT,
		FUNC,
		SECTION,
		FILE,
		COMMON,
		TLS,
		NUM,
		LOOS,
		GNU_IFUNC,
		HIOS,
		LOPROC,
		SPARC_REGISTER,
		HIPROC,
	}

	[CCode (cprefix = "GUM_ELF_BIND_")]
	public enum ElfSymbolBind {
		LOCAL,
		GLOBAL,
		WEAK,
		LOOS,
		GNU_UNIQUE,
		HIOS,
		LOPROC,
		HIPROC,
	}

	[CCode (cprefix = "GUM_ELF_IA32_")]
	public enum ElfIA32Relocation {
		NONE,
		@32,
		PC32,
		GOT32,
		PLT32,
		COPY,
		GLOB_DAT,
		JMP_SLOT,
		RELATIVE,
		GOTOFF,
		GOTPC,
		32PLT,
		TLS_TPOFF,
		TLS_IE,
		TLS_GOTIE,
		TLS_LE,
		TLS_GD,
		TLS_LDM,
		@16,
		PC16,
		@8,
		PC8,
		TLS_GD_32,
		TLS_GD_PUSH,
		TLS_GD_CALL,
		TLS_GD_POP,
		TLS_LDM_32,
		TLS_LDM_PUSH,
		TLS_LDM_CALL,
		TLS_LDM_POP,
		TLS_LDO_32,
		TLS_IE_32,
		TLS_LE_32,
		TLS_DTPMOD32,
		TLS_DTPOFF32,
		TLS_TPOFF32,
		SIZE32,
		TLS_GOTDESC,
		TLS_DESC_CALL,
		TLS_DESC,
		IRELATIVE,
		GOT32X,
	}

	[CCode (cprefix = "GUM_ELF_X64_")]
	public enum ElfX64Relocation {
		NONE,
		@64,
		PC32,
		GOT32,
		PLT32,
		COPY,
		GLOB_DAT,
		JUMP_SLOT,
		RELATIVE,
		GOTPCREL,
		@32,
		@32S,
		@16,
		PC16,
		@8,
		PC8,
		DTPMOD64,
		DTPOFF64,
		TPOFF64,
		TLSGD,
		TLSLD,
		DTPOFF32,
		GOTTPOFF,
		TPOFF32,
		PC64,
		GOTOFF64,
		GOTPC32,
		GOT64,
		GOTPCREL64,
		GOTPC64,
		GOTPLT64,
		PLTOFF64,
		SIZE32,
		SIZE64,
		GOTPC32_TLSDESC,
		TLSDESC_CALL,
		TLSDESC,
		IRELATIVE,
		RELATIVE64,
		GOTPCRELX,
		REX_GOTPCRELX,
	}

	[CCode (cprefix = "GUM_ELF_ARM_")]
	public enum ElfArmRelocation {
		NONE,
		PC24,
		ABS32,
		REL32,
		PC13,
		ABS16,
		ABS12,
		THM_ABS5,
		ABS8,
		SBREL32,
		THM_PC22,
		THM_PC8,
		AMP_VCALL9,
		SWI24,
		TLS_DESC,
		THM_SWI8,
		XPC25,
		THM_XPC22,
		TLS_DTPMOD32,
		TLS_DTPOFF32,
		TLS_TPOFF32,
		COPY,
		GLOB_DAT,
		JUMP_SLOT,
		RELATIVE,
		GOTOFF,
		GOTPC,
		GOT32,
		PLT32,
		CALL,
		JUMP24,
		THM_JUMP24,
		BASE_ABS,
		ALU_PCREL_7_0,
		ALU_PCREL_15_8,
		ALU_PCREL_23_15,
		LDR_SBREL_11_0,
		ALU_SBREL_19_12,
		ALU_SBREL_27_20,
		TARGET1,
		SBREL31,
		V4BX,
		TARGET2,
		PREL31,
		MOVW_ABS_NC,
		MOVT_ABS,
		MOVW_PREL_NC,
		MOVT_PREL,
		THM_MOVW_ABS_NC,
		THM_MOVT_ABS,
		THM_MOVW_PREL_NC,
		THM_MOVT_PREL,
		THM_JUMP19,
		THM_JUMP6,
		THM_ALU_PREL_11_0,
		THM_PC12,
		ABS32_NOI,
		REL32_NOI,
		ALU_PC_G0_NC,
		ALU_PC_G0,
		ALU_PC_G1_NC,
		ALU_PC_G1,
		ALU_PC_G2,
		LDR_PC_G1,
		LDR_PC_G2,
		LDRS_PC_G0,
		LDRS_PC_G1,
		LDRS_PC_G2,
		LDC_PC_G0,
		LDC_PC_G1,
		LDC_PC_G2,
		ALU_SB_G0_NC,
		ALU_SB_G0,
		ALU_SB_G1_NC,
		ALU_SB_G1,
		ALU_SB_G2,
		LDR_SB_G0,
		LDR_SB_G1,
		LDR_SB_G2,
		LDRS_SB_G0,
		LDRS_SB_G1,
		LDRS_SB_G2,
		LDC_SB_G0,
		LDC_SB_G1,
		LDC_SB_G2,
		MOVW_BREL_NC,
		MOVT_BREL,
		MOVW_BREL,
		THM_MOVW_BREL_NC,
		THM_MOVT_BREL,
		THM_MOVW_BREL,
		TLS_GOTDESC,
		TLS_CALL,
		TLS_DESCSEQ,
		THM_TLS_CALL,
		PLT32_ABS,
		GOT_ABS,
		GOT_PREL,
		GOT_BREL12,
		GOTOFF12,
		GOTRELAX,
		GNU_VTENTRY,
		GNU_VTINHERIT,
		THM_PC11,
		THM_PC9,
		TLS_GD32,
		TLS_LDM32,
		TLS_LDO32,
		TLS_IE32,
		TLS_LE32,
		TLS_LDO12,
		TLS_LE12,
		TLS_IE12GP,
		ME_TOO,
		THM_TLS_DESCSEQ,
		THM_TLS_DESCSEQ16,
		THM_TLS_DESCSEQ32,
		THM_GOT_BREL12,
		IRELATIVE,
		RXPC25,
		RSBREL32,
		THM_RPC22,
		RREL32,
		RABS22,
		RPC24,
		RBASE,
	}

	[CCode (cprefix = "GUM_ELF_ARM64_")]
	public enum ElfArm64Relocation {
		NONE,
		P32_ABS32,
		P32_COPY,
		P32_GLOB_DAT,
		P32_JUMP_SLOT,
		P32_RELATIVE,
		P32_TLS_DTPMOD,
		P32_TLS_DTPREL,
		P32_TLS_TPREL,
		P32_TLSDESC,
		P32_IRELATIVE,
		ABS64,
		ABS32,
		ABS16,
		PREL64,
		PREL32,
		PREL16,
		MOVW_UABS_G0,
		MOVW_UABS_G0_NC,
		MOVW_UABS_G1,
		MOVW_UABS_G1_NC,
		MOVW_UABS_G2,
		MOVW_UABS_G2_NC,
		MOVW_UABS_G3,
		MOVW_SABS_G0,
		MOVW_SABS_G1,
		MOVW_SABS_G2,
		LD_PREL_LO19,
		ADR_PREL_LO21,
		ADR_PREL_PG_HI21,
		ADR_PREL_PG_HI21_NC,
		ADD_ABS_LO12_NC,
		LDST8_ABS_LO12_NC,
		TSTBR14,
		CONDBR19,
		JUMP26,
		CALL26,
		LDST16_ABS_LO12_NC,
		LDST32_ABS_LO12_NC,
		LDST64_ABS_LO12_NC,
		MOVW_PREL_G0,
		MOVW_PREL_G0_NC,
		MOVW_PREL_G1,
		MOVW_PREL_G1_NC,
		MOVW_PREL_G2,
		MOVW_PREL_G2_NC,
		MOVW_PREL_G3,
		LDST128_ABS_LO12_NC,
		MOVW_GOTOFF_G0,
		MOVW_GOTOFF_G0_NC,
		MOVW_GOTOFF_G1,
		MOVW_GOTOFF_G1_NC,
		MOVW_GOTOFF_G2,
		MOVW_GOTOFF_G2_NC,
		MOVW_GOTOFF_G3,
		GOTREL64,
		GOTREL32,
		GOT_LD_PREL19,
		LD64_GOTOFF_LO15,
		ADR_GOT_PAGE,
		LD64_GOT_LO12_NC,
		LD64_GOTPAGE_LO15,
		TLSGD_ADR_PREL21,
		TLSGD_ADR_PAGE21,
		TLSGD_ADD_LO12_NC,
		TLSGD_MOVW_G1,
		TLSGD_MOVW_G0_NC,
		TLSLD_ADR_PREL21,
		TLSLD_ADR_PAGE21,
		TLSLD_ADD_LO12_NC,
		TLSLD_MOVW_G1,
		TLSLD_MOVW_G0_NC,
		TLSLD_LD_PREL19,
		TLSLD_MOVW_DTPREL_G2,
		TLSLD_MOVW_DTPREL_G1,
		TLSLD_MOVW_DTPREL_G1_NC,
		TLSLD_MOVW_DTPREL_G0,
		TLSLD_MOVW_DTPREL_G0_NC,
		TLSLD_ADD_DTPREL_HI12,
		TLSLD_ADD_DTPREL_LO12,
		TLSLD_ADD_DTPREL_LO12_NC,
		TLSLD_LDST8_DTPREL_LO12,
		TLSLD_LDST8_DTPREL_LO12_NC,
		TLSLD_LDST16_DTPREL_LO12,
		TLSLD_LDST16_DTPREL_LO12_NC,
		TLSLD_LDST32_DTPREL_LO12,
		TLSLD_LDST32_DTPREL_LO12_NC,
		TLSLD_LDST64_DTPREL_LO12,
		TLSLD_LDST64_DTPREL_LO12_NC,
		TLSIE_MOVW_GOTTPREL_G1,
		TLSIE_MOVW_GOTTPREL_G0_NC,
		TLSIE_ADR_GOTTPREL_PAGE21,
		TLSIE_LD64_GOTTPREL_LO12_NC,
		TLSIE_LD_GOTTPREL_PREL19,
		TLSLE_MOVW_TPREL_G2,
		TLSLE_MOVW_TPREL_G1,
		TLSLE_MOVW_TPREL_G1_NC,
		TLSLE_MOVW_TPREL_G0,
		TLSLE_MOVW_TPREL_G0_NC,
		TLSLE_ADD_TPREL_HI12,
		TLSLE_ADD_TPREL_LO12,
		TLSLE_ADD_TPREL_LO12_NC,
		TLSLE_LDST8_TPREL_LO12,
		TLSLE_LDST8_TPREL_LO12_NC,
		TLSLE_LDST16_TPREL_LO12,
		TLSLE_LDST16_TPREL_LO12_NC,
		TLSLE_LDST32_TPREL_LO12,
		TLSLE_LDST32_TPREL_LO12_NC,
		TLSLE_LDST64_TPREL_LO12,
		TLSLE_LDST64_TPREL_LO12_NC,
		TLSDESC_LD_PREL19,
		TLSDESC_ADR_PREL21,
		TLSDESC_ADR_PAGE21,
		TLSDESC_LD64_LO12,
		TLSDESC_ADD_LO12,
		TLSDESC_OFF_G1,
		TLSDESC_OFF_G0_NC,
		TLSDESC_LDR,
		TLSDESC_ADD,
		TLSDESC_CALL,
		TLSLE_LDST128_TPREL_LO12,
		TLSLE_LDST128_TPREL_LO12_NC,
		TLSLD_LDST128_DTPREL_LO12,
		TLSLD_LDST128_DTPREL_LO12_NC,
		COPY,
		GLOB_DAT,
		JUMP_SLOT,
		RELATIVE,
		TLS_DTPMOD,
		TLS_DTPREL,
		TLS_TPREL,
		TLSDESC,
		IRELATIVE,
	}

	[CCode (cprefix = "GUM_ELF_MIPS_")]
	public enum ElfMipsRelocation {
		NONE,
		@16,
		@32,
		REL32,
		@26,
		HI16,
		LO16,
		GPREL16,
		LITERAL,
		GOT16,
		PC16,
		CALL16,
		GPREL32,
		SHIFT5,
		SHIFT6,
		@64,
		GOT_DISP,
		GOT_PAGE,
		GOT_OFST,
		GOT_HI16,
		GOT_LO16,
		SUB,
		INSERT_A,
		INSERT_B,
		DELETE,
		HIGHER,
		HIGHEST,
		CALL_HI16,
		CALL_LO16,
		SCN_DISP,
		REL16,
		ADD_IMMEDIATE,
		PJUMP,
		RELGOT,
		JALR,
		TLS_DTPMOD32,
		TLS_DTPREL32,
		TLS_DTPMOD64,
		TLS_DTPREL64,
		TLS_GD,
		TLS_LDM,
		TLS_DTPREL_HI16,
		TLS_DTPREL_LO16,
		TLS_GOTTPREL,
		TLS_TPREL32,
		TLS_TPREL64,
		TLS_TPREL_HI16,
		TLS_TPREL_LO16,
		GLOB_DAT,
		COPY,
		JUMP_SLOT,
	}

	public class DarwinModule : GLib.Object {
		public Filetype filetype;
		public string? name;
		public string? uuid;
		public string? source_version;

		public DarwinPort task;
		public bool is_local;
		public bool is_kernel;
		public Gum.CpuType cpu_type;
		public size_t pointer_size;
		public size_t page_size;
		public Gum.Address base_address;
		public string? source_path;
		public GLib.Bytes? source_blob;

		public DarwinModuleImage image;

		public void * info;
		public void * symtab;
		public void * dysymtab;

		public Gum.Address preferred_address;

		public GLib.Array<DarwinSegment> segments;

		public bool lacks_exports_for_reexports {
			get;
		}

		public Gum.Address slide {
			get;
		}

		public enum Filetype {
			OBJECT = 1,
			EXECUTE,
			FVMLIB,
			CORE,
			PRELOAD,
			DYLIB,
			DYLINKER,
			BUNDLE,
			DYLIB_STUB,
			DSYM,
			KEXT_BUNDLE,
			FILESET,
		}

		[Flags]
		public enum Flags {
			NONE        = 0,
			HEADER_ONLY = (1 << 0),
		}

		public DarwinModule.from_file (string path, Gum.CpuType cpu_type, Gum.PtrauthSupport ptrauth_support, Gum.DarwinModule.Flags flags = NONE) throws Gum.Error;
		public DarwinModule.from_blob (GLib.Bytes blob, Gum.CpuType cpu_type, Gum.PtrauthSupport ptrauth_support, Gum.DarwinModule.Flags flags = NONE) throws Gum.Error;
		public DarwinModule.from_memory (string? name, Gum.DarwinPort task, Gum.Address base_address, Gum.DarwinModule.Flags flags = NONE) throws Gum.Error;

		public bool load () throws Gum.Error;

		public bool resolve_export (string symbol, out Gum.DarwinExportDetails details);
		public Gum.Address resolve_symbol_address (string symbol);
		public void enumerate_imports (Gum.FoundImportFunc func);
		public void enumerate_exports (Gum.FoundDarwinExportFunc func);
		public void enumerate_symbols (Gum.FoundDarwinSymbolFunc func);
		public void enumerate_sections (Gum.FoundDarwinSectionFunc func);
		public bool is_address_in_text_section (Gum.Address address);
		public void enumerate_chained_fixups (Gum.FoundDarwinChainedFixupsFunc func);
		public void enumerate_rebases (Gum.FoundDarwinRebaseFunc func);
		public void enumerate_binds (Gum.FoundDarwinBindFunc func);
		public void enumerate_lazy_binds (Gum.FoundDarwinBindFunc func);
		public void enumerate_init_pointers (Gum.FoundDarwinInitPointersFunc func);
		public void enumerate_init_offsets (Gum.FoundDarwinInitOffsetsFunc func);
		public void enumerate_term_pointers (Gum.FoundDarwinTermPointersFunc func);
		public void enumerate_dependencies (Gum.FoundDependencyFunc func);
		public unowned string? get_dependency_by_ordinal (int ordinal);
	}

	public delegate bool FoundDarwinExportFunc (Gum.DarwinExportDetails details);
	public delegate bool FoundDarwinSymbolFunc (Gum.DarwinSymbolDetails details);
	public delegate bool FoundDarwinSectionFunc (Gum.DarwinSectionDetails details);
	public delegate bool FoundDarwinChainedFixupsFunc (Gum.DarwinChainedFixupsDetails details);
	public delegate bool FoundDarwinRebaseFunc (Gum.DarwinRebaseDetails details);
	public delegate bool FoundDarwinBindFunc (Gum.DarwinBindDetails details);
	public delegate bool FoundDarwinInitPointersFunc (Gum.DarwinInitPointersDetails details);
	public delegate bool FoundDarwinInitOffsetsFunc (Gum.DarwinInitOffsetsDetails details);
	public delegate bool FoundDarwinTermPointersFunc (Gum.DarwinTermPointersDetails details);

	[Compact]
	public class DarwinModuleImage {
		public void * data;
		public uint64 size;
		public void * linkedit;

		public uint64 source_offset;
		public uint64 source_size;
		public uint64 shared_offset;
		public uint64 shared_size;
		public GLib.Array<Gum.DarwinModuleImageSegment> shared_segments;

		public GLib.Bytes bytes;
		public void * malloc_data;
	}

	public struct DarwinModuleImageSegment {
		public uint64 offset;
		public uint64 size;
		public int protection;
	}

	public struct DarwinSectionDetails {
		public string segment_name;
		public string section_name;
		public Gum.Address vm_address;
		public uint64 size;
		public Gum.DarwinPageProtection protection;
		public uint32 file_offset;
		public uint32 flags;
	}

	public struct DarwinChainedFixupsDetails {
		public Gum.Address vm_address;
		uint64 file_offset;
		uint32 size;
	}

	public struct DarwinRebaseDetails {
		public Gum.DarwinSegment? segment;
		public uint64 offset;
		public DarwinRebaseType type;
		public Gum.Address slide;
	}

	public struct DarwinBindDetails {
		public Gum.DarwinSegment? segment;
		public uint64 offset;
		public Gum.DarwinBindType type;
		public Gum.DarwinBindOrdinal library_ordinal;
		public string symbol_name;
		public Gum.DarwinBindSymbolFlags symbol_flags;
		public int64 addend;
	}

	public struct DarwinThreadedItem {
		public bool is_authenticated;
		public Gum.DarwinThreadedItemType type;
		public uint16 delta;
		public uint8 key;
		public bool has_address_diversity;
		public uint16 diversity;

		public uint16 bind_ordinal;

		public Gum.Address rebase_address;

		public static void parse (uint64 value, out Gum.DarwinThreadedItem result);
	}

	public struct DarwinInitPointersDetails {
		public Gum.Address address;
		public uint64 count;
	}

	public struct DarwinInitOffsetsDetails {
		public Gum.Address address;
		public uint64 count;
	}

	public struct DarwinTermPointersDetails {
		public Gum.Address address;
		public uint64 count;
	}

	public struct DarwinSegment {
		public string name;
		public Gum.Address vm_address;
		public uint64 vm_size;
		public uint64 file_offset;
		public uint64 file_size;
		public Gum.DarwinPageProtection protection;
	}

	public struct DarwinExportDetails {
		public string name;
		public uint64 flags;

		public uint64 offset;

		public uint64 stub;
		public uint64 resolver;

		public int reexport_library_ordinal;
		public string reexport_symbol;
	}

	public struct DarwinSymbolDetails {
		public string name;
		public Gum.Address address;

		public uint8 type;
		public uint8 section;
		public uint16 description;
	}

	[CCode (cprefix = "GUM_DARWIN_REBASE_")]
	public enum DarwinRebaseType {
		POINTER = 1,
		TEXT_ABSOLUTE32,
		TEXT_PCREL32,
	}

	[CCode (cprefix = "GUM_DARWIN_BIND_")]
	public enum DarwinBindType {
		POINTER = 1,
		TEXT_ABSOLUTE32,
		TEXT_PCREL32,
		THREADED_TABLE,
		THREADED_ITEMS,
	}

	[CCode (cprefix = "GUM_DARWIN_THREADED_")]
	public enum DarwinThreadedItemType {
		REBASE,
		BIND
	}

	[CCode (cprefix = "GUM_DARWIN_BIND_")]
	public enum DarwinBindOrdinal {
		SELF            =  0,
		MAIN_EXECUTABLE = -1,
		FLAT_LOOKUP     = -2,
		WEAK_LOOKUP     = -3,
	}

	[Flags]
	[CCode (cprefix = "GUM_DARWIN_BIND_")]
	public enum DarwinBindSymbolFlags {
		WEAK_IMPORT         = 0x1,
		NON_WEAK_DEFINITION = 0x8,
	}

	public const int DARWIN_EXPORT_KIND_MASK;

	[CCode (cprefix = "GUM_DARWIN_EXPORT_")]
	public enum DarwinExportSymbolKind {
		REGULAR,
		THREAD_LOCAL,
		ABSOLUTE
	}

	[Flags]
	[CCode (cprefix = "GUM_DARWIN_EXPORT_")]
	public enum DarwinExportSymbolFlags {
		WEAK_DEFINITION   = 0x04,
		REEXPORT          = 0x08,
		STUB_AND_RESOLVER = 0x10,
	}

	[CCode (has_type_id = false)]
	public struct DarwinPort : uint {
		[CCode (cname = "GUM_DARWIN_PORT_NULL")]
		public const DarwinPort NULL;
	}

	[CCode (has_type_id = false)]
	public struct DarwinPageProtection : int {
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-x86/gumx86writer.h", ref_function = "gum_x86_writer_ref", unref_function = "gum_x86_writer_unref", has_type_id = false)]
	public class X86Writer {
		public Gum.Address pc;

		public X86Writer (void * code_address);

		public void reset (void * code_address);
		public void set_target_cpu (Gum.CpuType cpu_type);

		public void * cur ();
		public uint offset ();

		public bool flush ();

		public bool put_label (void * id);

		public static bool can_branch_directly_between (Gum.Address from, Gum.Address to);
		public void put_call_address_with_arguments (Gum.CallingConvention conv, Gum.Address func, uint n_args, ...);
		public void put_call_address_with_arguments_array (Gum.CallingConvention conv, Gum.Address func, [CCode (array_length_pos = 2.1, array_length_type = "guint")] Gum.Argument[] args);
		public bool put_call_address (Gum.Address address);
		public bool put_call_reg (Gum.X86Reg reg);
		public bool put_jmp_address (Gum.Address address);
		public bool put_jmp_reg (Gum.X86Reg reg);
		public void put_jcc_short_label (Gum.X86Insn instruction_id, void * label_id, Gum.BranchHint hint);

		public bool put_add_reg_imm (Gum.X86Reg reg, ssize_t imm_value);
		public bool put_lock_cmpxchg_reg_ptr_reg (Gum.X86Reg dst_reg, Gum.X86Reg src_reg);

		public bool put_xor_reg_reg (Gum.X86Reg dst_reg, Gum.X86Reg src_reg);

		public bool put_mov_reg_reg (Gum.X86Reg dst_reg, Gum.X86Reg src_reg);
		public bool put_mov_reg_u32 (Gum.X86Reg dst_reg, uint32 imm_value);
		public bool put_mov_reg_u64 (Gum.X86Reg dst_reg, uint64 imm_value);
		public void put_mov_reg_address (Gum.X86Reg dst_reg, Gum.Address address);
		public bool put_mov_reg_offset_ptr_reg (Gum.X86Reg dst_reg, ssize_t dst_offset, Gum.X86Reg src_reg);
		public bool put_mov_reg_reg_offset_ptr (Gum.X86Reg dst_reg, Gum.X86Reg src_reg, ssize_t src_offset);

		public bool put_lea_reg_reg_offset (Gum.X86Reg dst_reg, Gum.X86Reg src_reg, ssize_t src_offset);

		public void put_push_u32 (uint32 imm_value);
		public bool put_push_reg (Gum.X86Reg reg);
		public bool put_pop_reg (Gum.X86Reg reg);
		public void put_pushax ();
		public void put_popax ();

		public bool put_test_reg_reg (Gum.X86Reg reg_a, Gum.X86Reg reg_b);

		public void put_nop ();
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-x86/gumx86relocator.h", ref_function = "gum_x86_relocator_ref", unref_function = "gum_x86_relocator_unref", has_type_id = false)]
	public class X86Relocator {
		public int ref_count;

		public void * capstone;

		public uint8 * input_start;
		public uint8 * input_cur;
		public Gum.Address input_pc;
		public void ** input_insns;
		public Gum.X86Writer output;

		public uint inpos;
		public uint outpos;

		public bool eob;
		public bool eoi;

		public X86Relocator (void * input_code, Gum.X86Writer output);

		public void reset (void * input_code, Gum.X86Writer output);

		public uint read_one (out void * instruction = null);

		public void * peek_next_write_insn ();
		public void * peek_next_write_source ();
		public void skip_one ();
		public bool write_one ();
		public void write_all ();
	}

	[CCode (cheader_filename = "gum/arch-x86/gumx86writer.h", cprefix = "GUM_X86_", has_type_id = false)]
	public enum X86Reg {
		EAX,
		ECX,
		EDX,
		EBX,
		ESP,
		EBP,
		ESI,
		EDI,
		R8D,
		R9D,
		R10D,
		R11D,
		R12D,
		R13D,
		R14D,
		R15D,
		EIP,
		RAX,
		RCX,
		RDX,
		RBX,
		RSP,
		RBP,
		RSI,
		RDI,
		R8,
		R9,
		R10,
		R11,
		R12,
		R13,
		R14,
		R15,
		RIP,
		XAX,
		XCX,
		XDX,
		XBX,
		XSP,
		XBP,
		XSI,
		XDI,
		XIP,
		NONE,
	}

	[CCode (cname = "x86_insn", cheader_filename = "gum/arch-x86/gumx86writer.h", cprefix = "X86_INS_", has_type_id = false)]
	public enum X86Insn {
		JE,
		JNE,
	}

	[CCode (cheader_filename = "gum/gumdefs.h", cprefix = "GUM_", has_type_id = false)]
	public enum BranchHint {
		NO_HINT,
		LIKELY,
		UNLIKELY,
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-arm/gumarmwriter.h", ref_function = "gum_arm_writer_ref", unref_function = "gum_arm_writer_unref", has_type_id = false)]
	public class ArmWriter {
		public int ref_count;
		public bool flush_on_destroy;

		public Gum.OS target_os;

		public uint32 * base;
		public uint32 * code;
		public Gum.Address pc;

		public ArmWriter (void * code_address);

		public void reset (void * code_address);

		public void * cur ();
		public uint offset ();

		public bool flush ();

		public bool put_label (void * id);

		public void put_call_address_with_arguments (Gum.Address func, uint n_args, ...);

		public void put_branch_address (Gum.Address address);

		public bool can_branch_directly_between (Gum.Address from, Gum.Address to);
		public void put_b_label (void * label_id);

		public bool put_push_regs (uint n, ...);
		public bool put_pop_regs (uint n, ...);
		public bool put_vpush_range (Gum.ArmReg first_reg, Gum.ArmReg last_reg);
		public bool put_vpop_range (Gum.ArmReg first_reg, Gum.ArmReg last_reg);

		public bool put_ldr_reg_address (Gum.ArmReg reg, Gum.Address address);
		public void put_ldr_reg_reg (Gum.ArmReg dst_reg, Gum.ArmReg src_reg);
		public bool put_ldr_reg_reg_offset (Gum.ArmReg dst_reg, Gum.ArmReg src_reg, ssize_t src_offset);

		public void put_str_reg_reg (Gum.ArmReg src_reg, Gum.ArmReg dst_reg);
		public bool put_str_reg_reg_offset (Gum.ArmReg src_reg, Gum.ArmReg dst_reg, ssize_t dst_offset);

		public void put_mov_reg_reg (Gum.ArmReg dst_reg, Gum.ArmReg src_reg);
		public void put_mov_reg_cpsr (Gum.ArmReg reg);
		public void put_mov_cpsr_reg (Gum.ArmReg reg);
		public bool put_add_reg_u16 (Gum.ArmReg dst_reg, uint16 val);
		public bool put_add_reg_reg_imm (Gum.ArmReg dst_reg, Gum.ArmReg left_reg, uint32 right_value);
		public bool put_sub_reg_u16 (Gum.ArmReg dst_reg, uint16 val);
		public bool put_sub_reg_reg_imm (Gum.ArmReg dst_reg, Gum.ArmReg left_reg, uint32 right_value);
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-arm/gumarmrelocator.h", ref_function = "gum_arm_relocator_ref", unref_function = "gum_arm_relocator_unref", has_type_id = false)]
	public class ArmRelocator {
		public int ref_count;

		public void * capstone;

		public uint8 * input_start;
		public uint8 * input_cur;
		public Gum.Address input_pc;
		public Gum.ArmWriter output;

		public uint inpos;
		public uint outpos;

		public bool eob;
		public bool eoi;

		public ArmRelocator (void * input_code, Gum.ArmWriter output);

		public void reset (void * input_code, Gum.ArmWriter output);

		public uint read_one (out void * instruction = null);

		public void write_all ();
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-arm/gumthumbwriter.h", ref_function = "gum_thumb_writer_ref", unref_function = "gum_thumb_writer_unref", has_type_id = false)]
	public class ThumbWriter {
		public int ref_count;
		public bool flush_on_destroy;

		public Gum.OS target_os;

		public uint16 * base;
		public uint16 * code;
		public Gum.Address pc;

		public ThumbWriter (void * code_address);

		public void reset (void * code_address);

		public void * cur ();
		public uint offset ();

		public bool flush ();

		public bool put_label (void * id);

		public void put_call_address_with_arguments (Gum.Address func, uint n_args, ...);

		public void put_branch_address (Gum.Address address);

		public bool can_branch_directly_between (Gum.Address from, Gum.Address to);
		public void put_b_label (void * label_id);

		public bool put_push_regs (uint n_regs, ...);
		public bool put_pop_regs (uint n_regs, ...);
		public bool put_vpush_range (Gum.ArmReg first_reg, Gum.ArmReg last_reg);
		public bool put_vpop_range (Gum.ArmReg first_reg, Gum.ArmReg last_reg);

		public bool put_ldr_reg_address (Gum.ArmReg reg, Gum.Address address);
		public void put_ldr_reg_reg (Gum.ArmReg dst_reg, Gum.ArmReg src_reg);
		public bool put_ldr_reg_reg_offset (Gum.ArmReg dst_reg, Gum.ArmReg src_reg, size_t src_offset);

		public void put_str_reg_reg (Gum.ArmReg src_reg, Gum.ArmReg dst_reg);
		public bool put_str_reg_reg_offset (Gum.ArmReg src_reg, Gum.ArmReg dst_reg, size_t dst_offset);

		public void put_mov_reg_reg (Gum.ArmReg dst_reg, Gum.ArmReg src_reg);
		public void put_mov_reg_cpsr (Gum.ArmReg reg);
		public void put_mov_cpsr_reg (Gum.ArmReg reg);

		public bool put_add_reg_imm (Gum.ArmReg dst_reg, ssize_t imm_value);
		public bool put_add_reg_reg_imm (Gum.ArmReg dst_reg, Gum.ArmReg left_reg, ssize_t right_value);
		public bool put_sub_reg_imm (Gum.ArmReg dst_reg, ssize_t imm_value);
		public bool put_sub_reg_reg_imm (Gum.ArmReg dst_reg, Gum.ArmReg left_reg, ssize_t right_value);
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-arm/gumthumbrelocator.h", ref_function = "gum_thumb_relocator_ref", unref_function = "gum_thumb_relocator_unref", has_type_id = false)]
	public class ThumbRelocator {
		public int ref_count;

		public void * capstone;

		public uint8 * input_start;
		public uint8 * input_cur;
		public Gum.Address input_pc;
		public Gum.ThumbWriter output;

		public uint inpos;
		public uint outpos;

		public bool eob;
		public bool eoi;

		public ThumbRelocator (void * input_code, Gum.ThumbWriter output);

		public void reset (void * input_code, Gum.ThumbWriter output);

		public uint read_one (out void * instruction = null);

		public void write_all ();
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-arm64/gumarm64writer.h", ref_function = "gum_arm64_writer_ref", unref_function = "gum_arm64_writer_unref", has_type_id = false)]
	public class Arm64Writer {
		public int ref_count;
		public bool flush_on_destroy;

		public Gum.OS target_os;
		public Gum.PtrauthSupport ptrauth_support;
		[CCode (cname = "sign")]
		public Gum.PtrauthSignFunc sign_impl;

		public uint32 * base;
		public uint32 * code;
		public Gum.Address pc;

		public Arm64Writer (void * code_address);

		public void reset (void * code_address);

		public void * cur ();
		public uint offset ();
		public void skip (uint n_bytes);

		public bool flush ();

		public bool put_label (void * id);

		public void put_call_address_with_arguments (Gum.Address func, uint n_args, ...);
		public void put_call_address_with_arguments_array (Gum.Address func, [CCode (array_length_pos = 1.1, array_length_type = "guint")] Gum.Argument[] args);
		public void put_call_reg_with_arguments (Gum.Arm64Reg reg, uint n_args, ...);
		public void put_call_reg_with_arguments_array (Gum.Arm64Reg reg, [CCode (array_length_pos = 1.1, array_length_type = "guint")] Gum.Argument[] args);

		public void put_branch_address (Gum.Address address);

		public bool can_branch_directly_between (Gum.Address from, Gum.Address to);
		public bool put_b_imm (Gum.Address address);
		public void put_b_label (void * label_id);
		public void put_b_cond_label (Gum.Arm64ConditionCode cc, void * label_id);
		public bool put_bl_imm (Gum.Address address);
		public void put_bl_label (void * label_id);
		public bool put_br_reg (Gum.Arm64Reg reg);
		public bool put_br_reg_no_auth (Gum.Arm64Reg reg);
		public bool put_blr_reg (Gum.Arm64Reg reg);
		public bool put_blr_reg_no_auth (Gum.Arm64Reg reg);
		public void put_ret ();
		public bool put_ret_reg (Gum.Arm64Reg reg);
		public bool put_cbz_reg_imm (Gum.Arm64Reg reg, Gum.Address target);
		public bool put_cbnz_reg_imm (Gum.Arm64Reg reg, Gum.Address target);
		public void put_cbz_reg_label (Gum.Arm64Reg reg, void * label_id);
		public void put_cbnz_reg_label (Gum.Arm64Reg reg, void * label_id);
		public bool put_tbz_reg_imm_imm (Gum.Arm64Reg reg, uint bit, Gum.Address target);
		public bool put_tbnz_reg_imm_imm (Gum.Arm64Reg reg, uint bit, Gum.Address target);
		public void put_tbz_reg_imm_label (Gum.Arm64Reg reg, uint bit, void * label_id);
		public void put_tbnz_reg_imm_label (Gum.Arm64Reg reg, uint bit, void * label_id);

		public bool put_push_reg_reg (Gum.Arm64Reg reg_a, Gum.Arm64Reg reg_b);
		public bool put_pop_reg_reg (Gum.Arm64Reg reg_a, Gum.Arm64Reg reg_b);
		public void put_push_all_x_registers ();
		public void put_pop_all_x_registers ();
		public void put_push_all_q_registers ();
		public void put_pop_all_q_registers ();

		public bool put_ldr_reg_address (Gum.Arm64Reg reg, Gum.Address address);
		public bool put_ldr_reg_u32 (Gum.Arm64Reg reg, uint32 val);
		public bool put_ldr_reg_u64 (Gum.Arm64Reg reg, uint64 val);
		public bool put_ldr_reg_u32_ptr (Gum.Arm64Reg reg, Gum.Address src_address);
		public bool put_ldr_reg_u64_ptr (Gum.Arm64Reg reg, Gum.Address src_address);
		public uint put_ldr_reg_ref (Gum.Arm64Reg reg);
		public void put_ldr_reg_value (uint ref, Gum.Address value);
		public bool put_ldr_reg_reg (Gum.Arm64Reg dst_reg, Gum.Arm64Reg src_reg);
		public bool put_ldr_reg_reg_offset (Gum.Arm64Reg dst_reg, Gum.Arm64Reg src_reg, size_t src_offset);
		public bool put_ldr_reg_reg_offset_mode (Gum.Arm64Reg dst_reg, Gum.Arm64Reg src_reg, ssize_t src_offset, Gum.Arm64IndexMode mode);
		public bool put_ldrsw_reg_reg_offset (Gum.Arm64Reg dst_reg, Gum.Arm64Reg src_reg, size_t src_offset);
		public bool put_adrp_reg_address (Gum.Arm64Reg reg, Gum.Address address);
		public bool put_str_reg_reg (Gum.Arm64Reg src_reg, Gum.Arm64Reg dst_reg);
		public bool put_str_reg_reg_offset (Gum.Arm64Reg src_reg, Gum.Arm64Reg dst_reg, size_t dst_offset);
		public bool put_str_reg_reg_offset_mode (Gum.Arm64Reg src_reg, Gum.Arm64Reg dst_reg, ssize_t dst_offset, Gum.Arm64IndexMode mode);
		public bool put_ldp_reg_reg_reg_offset (Gum.Arm64Reg reg_a, Gum.Arm64Reg reg_b, Gum.Arm64Reg reg_src, ssize_t src_offset, Gum.Arm64IndexMode mode);
		public bool put_stp_reg_reg_reg_offset (Gum.Arm64Reg reg_a, Gum.Arm64Reg reg_b, Gum.Arm64Reg reg_dst, ssize_t dst_offset, Gum.Arm64IndexMode mode);
		public bool put_mov_reg_reg (Gum.Arm64Reg dst_reg, Gum.Arm64Reg src_reg);
		public void put_mov_reg_nzcv (Gum.Arm64Reg reg);
		public void put_mov_nzcv_reg (Gum.Arm64Reg reg);
		public bool put_uxtw_reg_reg (Gum.Arm64Reg dst_reg, Gum.Arm64Reg src_reg);
		public bool put_add_reg_reg_imm (Gum.Arm64Reg dst_reg, Gum.Arm64Reg left_reg, size_t right_value);
		public bool put_add_reg_reg_reg (Gum.Arm64Reg dst_reg, Gum.Arm64Reg left_reg, Gum.Arm64Reg right_reg);
		public bool put_sub_reg_reg_imm (Gum.Arm64Reg dst_reg, Gum.Arm64Reg left_reg, size_t right_value);
		public bool put_sub_reg_reg_reg (Gum.Arm64Reg dst_reg, Gum.Arm64Reg left_reg, Gum.Arm64Reg right_reg);
		public bool put_and_reg_reg_imm (Gum.Arm64Reg dst_reg, Gum.Arm64Reg left_reg, uint64 right_value);
		public bool put_tst_reg_imm (Gum.Arm64Reg reg, uint64 imm_value);
		public bool put_cmp_reg_reg (Gum.Arm64Reg reg_a, Gum.Arm64Reg reg_b);

		public bool put_xpaci_reg (Gum.Arm64Reg reg);

		public void put_nop ();
		public void put_brk_imm (uint16 imm);

		public void put_instruction (uint32 insn);
		public bool put_bytes ([CCode (array_length_type = "guint")] uint8[] data);

		public Gum.Address sign (Gum.Address value);
	}

	[Compact]
	[CCode (cheader_filename = "gum/arch-arm64/gumarm64relocator.h", ref_function = "gum_arm64_relocator_ref", unref_function = "gum_arm64_relocator_unref", has_type_id = false)]
	public class Arm64Relocator {
		public int ref_count;

		public void * capstone;

		public uint8 * input_start;
		public uint8 * input_cur;
		public Gum.Address input_pc;
		public void ** input_insns;
		public Gum.Arm64Writer output;

		public uint inpos;
		public uint outpos;

		public bool eob;
		public bool eoi;

		public Arm64Relocator (void * input_code, Gum.Arm64Writer output);

		public void reset (void * input_code, Gum.Arm64Writer output);

		public uint read_one (out void * instruction = null);

		public void * peek_next_write_insn ();
		public void * peek_next_write_source ();
		public void skip_one ();
		public bool write_one ();
		public void write_all ();

		public static bool can_relocate (void * address, uint min_bytes, Gum.RelocationScenario scenario, out uint maximum = null, out Gum.Arm64Reg available_scratch_reg = null);
		public static uint relocate (void * from, uint min_bytes, void * to);
	}

	[CCode (cname = "arm_reg", cprefix = "ARM_REG_")]
	public enum ArmReg {
		R0,
		R1,
		R2,
		R3,
		R4,
		R5,
		R6,
		R7,
		R8,
		R9,
		R10,
		R11,
		R12,
		SP,
		LR,
		PC,
		Q0,
		Q7,
		Q8,
		Q15,
	}

	[CCode (cname = "arm64_reg", cprefix = "ARM64_REG_")]
	public enum Arm64Reg {
		INVALID,
		FFR,
		FP,
		LR,
		NZCV,
		SP,
		VG,
		WSP,
		WZR,
		XZR,
		ZA,
		B0,
		B1,
		B2,
		B3,
		B4,
		B5,
		B6,
		B7,
		B8,
		B9,
		B10,
		B11,
		B12,
		B13,
		B14,
		B15,
		B16,
		B17,
		B18,
		B19,
		B20,
		B21,
		B22,
		B23,
		B24,
		B25,
		B26,
		B27,
		B28,
		B29,
		B30,
		B31,
		D0,
		D1,
		D2,
		D3,
		D4,
		D5,
		D6,
		D7,
		D8,
		D9,
		D10,
		D11,
		D12,
		D13,
		D14,
		D15,
		D16,
		D17,
		D18,
		D19,
		D20,
		D21,
		D22,
		D23,
		D24,
		D25,
		D26,
		D27,
		D28,
		D29,
		D30,
		D31,
		H0,
		H1,
		H2,
		H3,
		H4,
		H5,
		H6,
		H7,
		H8,
		H9,
		H10,
		H11,
		H12,
		H13,
		H14,
		H15,
		H16,
		H17,
		H18,
		H19,
		H20,
		H21,
		H22,
		H23,
		H24,
		H25,
		H26,
		H27,
		H28,
		H29,
		H30,
		H31,
		P0,
		P1,
		P2,
		P3,
		P4,
		P5,
		P6,
		P7,
		P8,
		P9,
		P10,
		P11,
		P12,
		P13,
		P14,
		P15,
		Q0,
		Q1,
		Q2,
		Q3,
		Q4,
		Q5,
		Q6,
		Q7,
		Q8,
		Q9,
		Q10,
		Q11,
		Q12,
		Q13,
		Q14,
		Q15,
		Q16,
		Q17,
		Q18,
		Q19,
		Q20,
		Q21,
		Q22,
		Q23,
		Q24,
		Q25,
		Q26,
		Q27,
		Q28,
		Q29,
		Q30,
		Q31,
		S0,
		S1,
		S2,
		S3,
		S4,
		S5,
		S6,
		S7,
		S8,
		S9,
		S10,
		S11,
		S12,
		S13,
		S14,
		S15,
		S16,
		S17,
		S18,
		S19,
		S20,
		S21,
		S22,
		S23,
		S24,
		S25,
		S26,
		S27,
		S28,
		S29,
		S30,
		S31,
		W0,
		W1,
		W2,
		W3,
		W4,
		W5,
		W6,
		W7,
		W8,
		W9,
		W10,
		W11,
		W12,
		W13,
		W14,
		W15,
		W16,
		W17,
		W18,
		W19,
		W20,
		W21,
		W22,
		W23,
		W24,
		W25,
		W26,
		W27,
		W28,
		W29,
		W30,
		X0,
		X1,
		X2,
		X3,
		X4,
		X5,
		X6,
		X7,
		X8,
		X9,
		X10,
		X11,
		X12,
		X13,
		X14,
		X15,
		X16,
		X17,
		X18,
		X19,
		X20,
		X21,
		X22,
		X23,
		X24,
		X25,
		X26,
		X27,
		X28,
		Z0,
		Z1,
		Z2,
		Z3,
		Z4,
		Z5,
		Z6,
		Z7,
		Z8,
		Z9,
		Z10,
		Z11,
		Z12,
		Z13,
		Z14,
		Z15,
		Z16,
		Z17,
		Z18,
		Z19,
		Z20,
		Z21,
		Z22,
		Z23,
		Z24,
		Z25,
		Z26,
		Z27,
		Z28,
		Z29,
		Z30,
		Z31,
		ZAB0,
		ZAD0,
		ZAD1,
		ZAD2,
		ZAD3,
		ZAD4,
		ZAD5,
		ZAD6,
		ZAD7,
		ZAH0,
		ZAH1,
		ZAQ0,
		ZAQ1,
		ZAQ2,
		ZAQ3,
		ZAQ4,
		ZAQ5,
		ZAQ6,
		ZAQ7,
		ZAQ8,
		ZAQ9,
		ZAQ10,
		ZAQ11,
		ZAQ12,
		ZAQ13,
		ZAQ14,
		ZAQ15,
		ZAS0,
		ZAS1,
		ZAS2,
		ZAS3,
		V0,
		V1,
		V2,
		V3,
		V4,
		V5,
		V6,
		V7,
		V8,
		V9,
		V10,
		V11,
		V12,
		V13,
		V14,
		V15,
		V16,
		V17,
		V18,
		V19,
		V20,
		V21,
		V22,
		V23,
		V24,
		V25,
		V26,
		V27,
		V28,
		V29,
		V30,
		V31,
		IP0,
		IP1,
		X29,
		X30,
	}

	[CCode (cname = "arm64_cc", cprefix = "ARM64_CC_")]
	public enum Arm64ConditionCode {
		INVALID,
		EQ,
		NE,
		HS,
		LO,
		MI,
		PL,
		VS,
		VC,
		HI,
		LS,
		GE,
		LT,
		GT,
		LE,
		AL,
		NV,
	}

	[CCode (cprefix = "GUM_INDEX_")]
	public enum Arm64IndexMode {
		POST_ADJUST,
		SIGNED_OFFSET,
		PRE_ADJUST,
	}
}
