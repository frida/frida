.text

#if (defined (HAVE_WINDOWS) && defined (__i386__)) || defined (__APPLE__)
# define GUM_CSYM(x) _ ## x
#else
# define GUM_CSYM(x) x
#endif

.globl GUM_CSYM (gum_stalker_follow_me)
GUM_CSYM (gum_stalker_follow_me):
#ifdef i386
movl %esp, %eax
subl $12, %esp
pushl %eax
pushl (4 + 8)(%eax)
pushl (4 + 4)(%eax)
pushl (4 + 0)(%eax)
call GUM_CSYM (_gum_stalker_do_follow_me)
addl $28, %esp
ret
#else
mov %rsp, %rcx
jmp GUM_CSYM (_gum_stalker_do_follow_me)
#endif

.globl GUM_CSYM (gum_stalker_activate)
GUM_CSYM (gum_stalker_activate):
#ifdef i386
movl %esp, %eax
pushl %eax
pushl (4 + 4)(%eax)
pushl (4 + 0)(%eax)
call GUM_CSYM (_gum_stalker_do_activate)
addl $12, %esp
ret
#else
mov %rsp, %rdx
jmp GUM_CSYM (_gum_stalker_do_activate)
#endif

.globl GUM_CSYM (gum_stalker_deactivate)
GUM_CSYM (gum_stalker_deactivate):
#ifdef i386
movl %esp, %eax
subl $4, %esp
pushl %eax
pushl (4 + 0)(%eax)
call GUM_CSYM (_gum_stalker_do_deactivate)
addl $12, %esp
ret
#else
mov %rsp, %rsi
jmp GUM_CSYM (_gum_stalker_do_deactivate)
#endif
