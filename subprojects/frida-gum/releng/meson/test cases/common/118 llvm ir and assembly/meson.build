project('llvm-ir', 'c', 'cpp')

if meson.backend() == 'xcode'
  error('MESON_SKIP_TEST: asm not supported with the Xcode backend. Patches welcome.')
endif

cpu = host_machine.cpu_family()
supported_cpus = ['arm', 'aarch64', 'x86', 'x86_64']

foreach lang : ['c', 'cpp']
  cc = meson.get_compiler(lang)
  cc_id = cc.get_id()
  ## Build a trivial executable with mixed LLVM IR source
  if cc_id == 'clang'
    e = executable('square_ir_' + lang, 'square.ll', 'main.' + lang)
    test('test IR square' + lang, e)
  endif
  ## Build a trivial executable with mixed assembly source
  # This also helps test whether cc.symbols_have_underscore_prefix() is working
  # properly. This is done by assembling some assembly into an object that will
  # provide the unsigned_squared() symbol to main.c/cpp. This requires the
  # C symbol mangling to be known in advance.
  if cc.symbols_have_underscore_prefix()
    uscore_args = ['-DMESON_TEST__UNDERSCORE_SYMBOL']
    message('underscore is prefixed')
  else
    uscore_args = []
    message('underscore is NOT prefixed')
  endif
  square_base = 'square-' + cpu
  square_impl = square_base + '.S'
  # MSVC cannot directly compile assembly files, so we pass it through the
  # cl.exe pre-processor first and then assemble it with ml.exe or armasm.exe
  # assembler.  Then we can link it into the executable.
  if cc.get_argument_syntax() == 'msvc'
    cl = cc.cmd_array()
    if cpu == 'x86'
      asmcmd = 'ml'
    elif cpu == 'x86_64'
      asmcmd = 'ml64'
    elif cpu == 'aarch64'
      asmcmd = 'armasm64'
    elif cpu == 'arm'
      asmcmd = 'armasm'
    else
      error('Unsupported cpu family: "' + cpu + '"')
    endif
    ml = find_program(asmcmd, required: false)
    if not ml.found()
      error('MESON_SKIP_TEST: Microsoft assembler (ml/armasm) not found')
    endif
    # Preprocess file (ml doesn't support pre-processing)
    # Force the input to be C (/Tc) because ICL otherwise assumes it's an object (.obj) file
    preproc_name = lang + square_base + '.i'
    square_preproc = custom_target(lang + square_impl + 'preproc',
        input : square_impl,
        output : preproc_name,
        command : [cl, '/nologo', '/EP', '/P', '/Fi' + preproc_name, '/Tc', '@INPUT@'] + uscore_args)
    # Use assembled object file instead of the original .S assembly source
    if asmcmd.startswith('armasm')
      square_impl = custom_target(lang + square_impl,
          input : square_preproc,
          output : lang + square_base + '.obj',
          command : [ml, '-nologo', '-o', '@OUTPUT@', '@INPUT@'])
    else
      square_impl = custom_target(lang + square_impl,
          input : square_preproc,
          output : lang + square_base + '.obj',
          command : [ml, '/nologo', '/safeseh', '/Fo', '@OUTPUT@', '/c', '@INPUT@'])
    endif
  endif
  if supported_cpus.contains(cpu)
    e = executable('square_asm_' + lang, square_impl, 'main.' + lang,
        c_args : uscore_args, cpp_args : uscore_args)
    test('test ASM square' + lang, e)
  elif cc_id != 'clang'
    error('MESON_SKIP_TEST: Unsupported cpu: "' + cpu + '", and LLVM not found')
  endif
endforeach
