# register s0 will have the answer
# register s1 will have the number

.globl main

.text
main:
    li s0, 1
    li s1, 10

    addi sp, sp, -4
    sw ra, (0)sp
    j factorial

    mv a0, s0
    li a7, 93
    ecall


factorial:
    j ra