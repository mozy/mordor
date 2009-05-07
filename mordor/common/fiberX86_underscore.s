.globl _fiber_switchContext
_fiber_switchContext:
  push %ebp
  mov %esp, %ebp
  push %eax
  push %ebx
  push %ecx
  push %esi
  push %edi

  movl 8(%ebp), %eax
  mov %esp, (%eax)
  movl 12(%ebp), %esp

  pop %edi
  pop %esi
  pop %ecx
  pop %ebx
  pop %eax
  pop %ebp

  ret

.globl _getEbx
_getEbx:
  mov %ebx, %eax
  ret
