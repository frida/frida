  .text
  .align 2

#ifdef __APPLE__
  .globl _gum_stalker_follow_me
_gum_stalker_follow_me:
#else
  .globl gum_stalker_follow_me
  .type gum_stalker_follow_me, %function
gum_stalker_follow_me:
#endif
  stp x29, x30, [sp, -16]!
  mov x29, sp
  mov x3, x30
#ifdef __APPLE__
  bl __gum_stalker_do_follow_me
#else
  bl _gum_stalker_do_follow_me
#endif
  ldp x29, x30, [sp], 16
  br x0

#ifdef __APPLE__
  .globl _gum_stalker_activate
_gum_stalker_activate:
#else
  .globl gum_stalker_activate
  .type gum_stalker_activate, %function
gum_stalker_activate:
#endif
  stp x29, x30, [sp, -16]!
  mov x29, sp
  mov x2, x30
#ifdef __APPLE__
  bl __gum_stalker_do_activate
#else
  bl _gum_stalker_do_activate
#endif
  ldp x29, x30, [sp], 16
  br x0

#ifdef __APPLE__
  .globl _gum_stalker_deactivate
_gum_stalker_deactivate:
#else
  .globl gum_stalker_deactivate
  .type gum_stalker_deactivate, %function
gum_stalker_deactivate:
#endif
  stp x29, x30, [sp, -16]!
  mov x29, sp
  mov x1, x30
#ifdef __APPLE__
  bl __gum_stalker_do_deactivate
#else
  bl _gum_stalker_do_deactivate
#endif
  ldp x29, x30, [sp], 16
  br x0
