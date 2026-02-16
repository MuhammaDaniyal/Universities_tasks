.globl main

.text
main:
    # Load some numbers to work with
    li t0, 15          # t0 = 15
    li t1, 5           # t1 = 5
    li t2, 3           # t2 = 3
    
    # 1. ADDITION
    add t3, t0, t1     # t3 = t0 + t1 = 15 + 5 = 20
    mv a0, t3          # Move result to a0 to see it
    li a7, 93          # Exit syscall
    ecall              # Program exits here! Nothing after runs
    
    # NOTHING BELOW HERE WILL EXECUTE
    # We need a SEPARATE program for each operation