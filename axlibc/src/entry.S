.section .text

.global _start
.type _start, %function
_start:
            bl main
            mov x8, #93
            svc #0
.size _start, .-_start
