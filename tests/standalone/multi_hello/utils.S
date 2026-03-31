// utils.S — puts and strlen, linked separately from main.S.
// Tests that the linker correctly resolves cross-file symbols in both directions:
//   main.S  →  puts   (defined here)
//   puts    →  strlen (defined here, same-file bl)

.section .text

// puts(x0: *str) — write null-terminated string to stdout via write(2) syscall.
.global puts
puts:
        stp x29, x30, [sp, #-16]!
        mov x29, sp

        // Save the buffer pointer across the strlen call.
        sub sp, sp, #16
        str x0, [sp]

        bl strlen           // strlen(x0) → x0 = length  (CALL26, same file)

        ldr x1, [sp]        // x1 = buffer pointer
        add sp, sp, #16

        mov x2, x0          // x2 = length
        mov x0, #1          // x0 = stdout fd
        mov x8, #64         // write syscall
        svc #0

        ldp x29, x30, [sp], #16
        ret

// strlen(x0: *str) → x0: length
.global strlen
strlen:
        mov x1, x0          // x1 = walking pointer (x0 = start, kept for subtraction)
strlen_loop:
        ldrb w2, [x1]       // byte = *x1
        cbz w2, strlen_done // if null terminator, done
        add x1, x1, #1      // advance pointer
        b strlen_loop
strlen_done:
        sub x0, x1, x0      // length = end − start
        ret
