SECTIONS {
  . = 0;

  .payload : ALIGN(4096)
  {
    KEEP(*(.text.entrypoint))
    *(.text*)
    *(.rodata*)
    *(.data*)
  }

  .dynstr : { *(.dynstr) }
  .dynsym : { *(.dynsym) }
  .gnu.hash : { *(.gnu.hash) }
  .hash : { *(.hash) }
  .note.gnu.build-id : { *(.note.gnu.build-id) }
  .note.android.ident : { *(.note.android.ident) }
  .got : ALIGN(64) { *(.got*) . = ALIGN(64); }
}
