---
short-description: Build targets for custom languages or corner-cases
...

# Custom build targets

While Meson tries to support as many languages and tools as possible,
there is no possible way for it to cover all corner cases. For these
cases it permits you to define custom build targets. Here is how one
would use it.

```meson
comp = find_program('custom_compiler')

infile = 'source_code.txt'
outfile = 'output.bin'

mytarget = custom_target('targetname',
  output : outfile,
  input : infile,
  command : [comp, '@INPUT@', '@OUTPUT@'],
  install : true,
  install_dir : 'subdir')
```

This would generate the binary `output.bin` and install it to
`${prefix}/subdir/output.bin`. Variable substitution works just like
it does for source generation.

See [Generating Sources](Generating-sources.md) for more information on this topic.

## Details on command invocation

Meson only permits you to specify one command to run. This is by
design as writing shell pipelines into build definition files leads to
code that is very hard to maintain. If your command requires multiple
steps you need to write a wrapper script that does all the necessary
work.

When doing this you need to be mindful of the following issues:

* do not assume that the command is invoked in any specific directory
* a target called `target` file `outfile` defined in subdir `subdir`
  must be written to `build_dir/subdir/foo.dat`
* if you need a subdirectory for temporary files, use
  `build_dir/subdir/target.dir`
