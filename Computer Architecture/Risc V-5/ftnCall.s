# ftnCall.s - Simple function call example
.globl main

.text
main:
    # Call a simple function with argument 5
    li a0, 5           # First argument
    jal ra, double     # Call function 'double', return address stored in ra
    
    # After return, result is in a0
    li a7, 93          # Exit syscall
    ecall

# Simple function that doubles its argument
double:
    add a0, a0, a0     # a0 = a0 + a0 (double it)
    jr ra              # Return to caller (address in ra)