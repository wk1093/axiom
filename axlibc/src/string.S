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
