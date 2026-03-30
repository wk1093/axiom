.section .data

msg: .asciz "Goodbye, World!\n"

.section .text
.global _start
_start:
        mov x0, sp          // Pass the stack pointer to our 'main'
        bl main             // Call your compiled code
        
        // --- The Exit Syscall ---
        // After main returns, we must exit or the CPU will execute garbage
        mov x8, #93         // exit(64-bit) syscall number
        svc #0

        ret

// --- A Minimal puts ---
// x0: pointer to string
puts:
        stp x29, x30, [sp, #-16]!
        mov x29, sp
        
        // 1. Find string length (or pass it in)
        // 2. x0 = 1 (stdout)
        // 3. x1 = buffer pointer
        // 4. x2 = length
        // 5. x8 = 64 (write)
        mov x1, x0          // buffer
        // we need to save x0 before calling strlen, as it will be overwritten
        sub sp, sp, #16     // Make space on stack for x0
        str x0, [sp]        // Save x0 on stack
        bl strlen           // You'll need a simple loop for this
        ldr x1, [sp]        // Restore x0 (buffer pointer) into x1
        add sp, sp, #16     // Restore stack pointer
        mov x2, x0          // length returned by strlen
        mov x0, #1          // stdout
        mov x8, #64         // write syscall
        svc #0
        
        ldp x29, x30, [sp], #16
        ret

// --- A Minimal strlen ---
// x0: pointer to string
// returns: length in x0
strlen:
        stp x29, x30, [sp, #-16]!
        mov x29, sp

        mov x1, x0          // Save string pointer
strlen_loop:
        ldrb w2, [x1]       // Load byte at current position
        cbz w2, strlen_done // If it's zero, we're done
        add x1, x1, #1      // Move to next byte
        b strlen_loop
strlen_done:
        sub x0, x1, x0      // Length = current pointer - start pointer
        ldp x29, x30, [sp], #16
        ret


main:
        stp x29, x30, [sp, #-16]!
        mov x29, sp

        adrp x0, msg
        add x0, x0, :lo12:msg
        bl puts

        mov x0, #0          // Return 0 from main
        ldp x29, x30, [sp], #16
        ret