# NanoVM
PoC lightweight x64 VM implementation

NanoVM is register based VM which also implements stack memory. The project also includes assembler with similiar syntax to x86 asm with intel syntax. Size of single instruction varies from 1 to 2 bytes + immediate value (up to 64 bit aka 8 bytes) if used. Opcode is encoded with 5 bits which limits the amount of different instructions to 32. Currently used opcodes (these might change when the project matures):

```Mov
Add
Sub
And
Or
Xor
Sar
Sal
Ror
Rol
Mul
Div
Mod
Cmp

Jz
Jnz
Jg
Js
Not
Inc
Dec
Ret

Call
Push
Pop
Halt
printi
prints
Memfind
Memset
Memcpy
Memcmp
```

memfind, memset, memcpy, memcmp are implemented as instructions to improve performance (e.g. further functions like handling strings can utilize these built in instructions).
Further documentation of the instruction format and different opcodes will be updated when those are more mature (I'm currently still changing those). For now see examples/