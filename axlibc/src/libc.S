.section .data

newline: .asciz "\n"

.section .text

.global strlen
strlen:
            mov x1, x0
strlen_loop:
            ldrb w2, [x1]
            cbz w2, strlen_done
            add x1, x1, #1
            b strlen_loop
strlen_done:
            sub x0, x1, x0
            ret

.global puts
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

.global _start
_start:
            bl main
            mov x8, #93
            svc #0
