.section .data

prompt: .asciz "Enter a number: "
format: .asciz "%d"
hello: .asciz "Hello, %d!\n"

.section .text
.global main
main:
        stp x29, x30, [sp, #-16]!
        mov x29, sp

        mov x1, x0
        mov x0, x3

        adr x0, prompt
        bl printf
        sub sp, sp, #16
        adr x0, format
        mov x1, sp
        bl scanf

        ldr w1, [sp]
        adr x0, hello
        bl printf

        add sp, sp, #16

        mov x0, #0
        ldp x29, x30, [sp], #16
        ret