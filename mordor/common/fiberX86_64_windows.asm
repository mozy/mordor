.code
fiber_switchContext proc
  push 		RBP
  mov  		RBP, RSP
  push 		RBX
  push 		RSI
  push 		RDI
  push 		R12
  push 		R13
  push 		R14
  push 		R15
  sub		RSP, 0A8h
  stmxcsr	[RSP+0A4h]
  fstcw		[RSP+0A0h]  
  movdqu    [RSP+90h], XMM6
  movdqu    [RSP+80h], XMM7
  movdqu    [RSP+70h], XMM8
  movdqu    [RSP+60h], XMM9
  movdqu    [RSP+50h], XMM10
  movdqu    [RSP+40h], XMM11
  movdqu    [RSP+30h], XMM12
  movdqu    [RSP+20h], XMM13
  movdqu    [RSP+10h], XMM14
  movdqu    [RSP], XMM15
  fwait;

  mov  		[RCX], RSP
  mov  		RSP, RDX

  movdqu	XMM15, [RSP]
  movdqu	XMM14, [RSP+10h]
  movdqu	XMM13, [RSP+20h]
  movdqu	XMM12, [RSP+30h]
  movdqu	XMM11, [RSP+40h]
  movdqu	XMM10, [RSP+50h]
  movdqu	XMM9, [RSP+60h]
  movdqu	XMM8, [RSP+70h]
  movdqu	XMM7, [RSP+80h]
  movdqu	XMM6, [RSP+90h]
  fldcw		[RSP+0A0h]
  ldmxcsr	[RSP+0A4h];
  add  		RSP, 0A8h
  pop  		R15
  pop  		R14
  pop  		R13
  pop  		R12
  pop  		RDI
  pop  		RSI
  pop  		RBX
  pop  		RBP

  ret
fiber_switchContext endp

end
