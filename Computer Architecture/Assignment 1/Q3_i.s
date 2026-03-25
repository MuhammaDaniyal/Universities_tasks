# x = a( b + c × d − e)
# s0 = a = 3, s1 = b = 2, s2 = c = 4, s3 = d = 6 , s5 = e = 6
# s6 = x = ?

.data

.text
.global main
main:

    # Load constants (if not already in registers)
    addi s0, zero, 3    # a = 3
    addi s1, zero, 2    # b = 2
    addi s2, zero, 4    # c = 4
    addi s3, zero, 6    # d = 6
    addi s5, zero, 6    # e = 6

    # Calculation
    mul  t0, s2, s3     # t0 = c * d (4 * 6 = 24)
    add  t1, s1, t0     # t1 = b + (c * d) (2 + 24 = 26)
    sub  t2, t1, s5     # t2 = (b + c * d) - e (26 - 6 = 20)
    mul  s6, s0, t2     # s6 = x = a * t2 (3 * 20 = 60)

    mv a0, s6                    # return 0
    li a7, 93                   # syscall 93 is 'exit'
    ecall
