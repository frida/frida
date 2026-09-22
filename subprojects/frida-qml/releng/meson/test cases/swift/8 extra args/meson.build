project('extra args', 'swift')

trace_fname = 'trace.json'

lib = static_library('main',
  'main.swift',
  swift_args: [
    '-emit-loaded-module-trace',
    '-emit-loaded-module-trace-path', '../' + trace_fname
  ],
)

custom_target('trace_check',
  output: trace_fname,
  command: ['cat', '@OUTPUT@'],
  depends: lib,
  build_always: true,
)
