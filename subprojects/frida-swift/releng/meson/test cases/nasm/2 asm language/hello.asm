;  hello.asm  a first program for nasm for Linux, Intel, gcc
;
; assemble:    nasm -f elf -l hello.lst  hello.asm
; link:        gcc -o hello  hello.o
; run:            hello
; output is:    Hello World

%include "config.asm"

%ifdef FOO
%define RETVAL HELLO
%endif

    SECTION .data        ; data section
msg:    db "Hello World",10    ; the string to print, 10=cr
len:    equ $-msg        ; "$" means "here"
                ; len is a value, not an address

    SECTION .text        ; code section
        global main        ; make label available to linker
main:                ; standard  gcc  entry point

    mov    edx,len        ; arg3, length of string to print
    mov    ecx,msg        ; arg2, pointer to string
    mov    ebx,1        ; arg1, where to write, screen
    mov    eax,4        ; write sysout command to int 80 hex
    int    0x80        ; interrupt 80 hex, call kernel

    mov    ebx,RETVAL    ; exit code, 0=normal
    mov    eax,1        ; exit command to kernel
    int    0x80        ; interrupt 80 hex, call kernel
