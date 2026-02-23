.data
true_msg:  .asciz "Condition is true\n"
true_len = . - true_msg  

false_msg: .asciz "Condition is false\n"
false_len = . - false_msg

.text
.global main
main:
    # Initialize variables
    li t0, 10         # a = 10
    li t1, 20         # b = 20
    li t2, 5          # c = 5

    # Evaluation: (a < b && b > c) || (a == c)
    blt t0, t1, check_b_gt_c   # if a < b, check b > c
    j check_a_eq_c             # else, skip to the OR part

check_b_gt_c:
    bgt t1, t2, is_true         # if b > c, the AND is true -> jump to print
    # If b <= c, the AND failed, fall through to check the OR part

check_a_eq_c:
    beq t0, t2, is_true         # if a == c, condition is true
    
is_false:
    li a0, 1                    # file descriptor 1 (stdout)
    la a1, false_msg            # buffer address
    li a2, false_len            # string length (including \n)
    li a7, 64                   # syscall 64 is 'write'
    ecall
    j exit_program

is_true:
    li a0, 1                    # file descriptor 1 (stdout)
    la a1, true_msg             # buffer address
    li a2, true_len             # string length (including \n)
    li a7, 64                   # syscall 64 is 'write'
    ecall

exit_program:
    li a0, 0                    # return 0
    li a7, 93                   # syscall 93 is 'exit'
    ecall
