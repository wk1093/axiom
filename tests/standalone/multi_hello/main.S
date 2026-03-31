// main.S — entry point and main function for the multi-object hello test.
// puts() is defined in utils.S; this tests cross-file bl (R_AARCH64_CALL26)
// and same-file adr to .data (R_AARCH64_ADR_PREL_LO21).

.section .data
msg: .asciz "Hello from two objects!\n"

.section .text
.global _start
_start:
        bl main
        // x0 holds main's return value; use it as the exit code
        mov x8, #93         // exit syscall
        svc #0

.global main
main:
        stp x29, x30, [sp, #-16]!
        mov x29, sp

        adr x0, msg         // x0 = &msg  (ADR_PREL_LO21 reloc)
        bl puts              // puts(msg)  (CALL26 reloc → utils.S)

        mov x0, #0
        ldp x29, x30, [sp], #16
        ret
