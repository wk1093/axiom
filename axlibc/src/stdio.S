.section .data

newline: .asciz "\n"

.section .text

.global puts
.type puts, %function
puts:
            stp x20, x30, [sp, #-16]!
            mov x29, sp

            sub sp, sp, #16
            str x0, [sp]
            bl strlen
            ldr x1, [sp]
            add sp, sp, #16

            mov x2, x0
            mov x0, #1
            mov x8, #64
            svc #0

            mov x8, #64
            mov x0, #1
            adr x1, newline
            mov x2, #1
            svc #0

            ldp x29, x30, [sp], #16
            ret
.size puts, .-puts