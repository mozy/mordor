.globl fiber_switchContext
fiber_switchContext:
  push %rbp
  mov %rsp, %rbp
  push %rbx
  push %r12
  push %r13
  push %r14
  push %r15
  sub $4, %rsp;
  stmxcsr (%rsp);
  sub $4, %rsp;
  fstcw (%rsp);
  fwait;
  sub $8, %rsp;

  mov %rsp, (%rdi)
  mov %rsi, %rsp

  add $8, %rsp;
  fldcw (%rsp);
  add $4, %rsp;
  ldmxcsr (%rsp);
  add $4, %rsp;
  pop %r15
  pop %r14
  pop %r13
  pop %r12
  pop %rbx
  pop %rbp

  ret
