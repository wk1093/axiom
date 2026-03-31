.section .data

hello: .asciz "Hello, World!"

.section .text
.global main
main:
        stp x29, x30, [sp, #-16]!
        mov x29, sp

        adr x0, hello
        bl puts

        mov x0, #0
        ldp x29, x30, [sp], #16
        ret
