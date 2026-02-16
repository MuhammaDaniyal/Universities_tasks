# minimal.s - Test if qemu works
.globl main

.text
main:
    li a0, 42         # return 42
    li a7, 93         # exit syscall
    ecall