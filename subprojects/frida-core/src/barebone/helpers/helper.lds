SECTIONS {
  .text : {
    *(.text.entrypoint)
    *(.text*)
  }
  .rodata : {
    *(.rodata*)
  }
  .data.rel.ro : {
    *(.data.rel.ro*)
  }
  .got : {
    *(.got*)
  }
  .panic_info : ALIGN(1024) {
    *(.bss.panic_info)
  }
  .bss : {
    *(.bss*)
  }
}
