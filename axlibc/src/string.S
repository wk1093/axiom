.section .text
.global strlen
.type strlen, %function
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
.size strlen, .-strlen

.global strcmp
.type strcmp, %function
strcmp:
            mov x1, x0
            mov x2, x1
            mov x3, x0
strcmp_loop:
            ldrb w4, [x1]
            ldrb w5, [x2]
            cmp w4, w5
            bne strcmp_diff
            cbz w4, strcmp_done
            add x1, x1, #1
            add x2, x2, #1
            b strcmp_loop
strcmp_diff:
            subs w0, w4, w5
            ret
strcmp_done:
            mov w0, #0
            ret
.size strcmp, .-strcmp