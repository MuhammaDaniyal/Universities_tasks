# x = a + b (c − d)/e
# s0 = a = 42, s1 = b =2, s2 = c = 14, s3 = d = 8 , s3 = e = 2
# s6 = x = ?

.data

.text
.global main
main:

    # Load constants
    addi s0, zero, 42   # a = 42
    addi s1, zero, 2    # b = 2
    addi s2, zero, 14   # c = 14
    addi s3, zero, 8    # d = 8
    addi s5, zero, 2    # e = 2 (Note: source mentions s3=e, likely a typo for s5)

    # Calculation
    sub  t0, s2, s3     # t0 = c - d (14 - 8 = 6)
    mul  t1, s1, t0     # t1 = b * (c - d) (2 * 6 = 12)
    div  t2, t1, s5     # t2 = [b(c - d)] / e (12 / 2 = 6)
    add  s6, s0, t2     # s6 = x = a + 6 (42 + 6 = 48)

    mv a0, s6                    # return 0
    li a7, 93                   # syscall 93 is 'exit'
    ecall
