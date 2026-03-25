# x = a + (b ÷ c x d)^2
# s0 = a = 3, s1 = b = 10, s2 = c = 5, s3 = d = 20 , s6 = x = ?

.data

.text
.global main
main:

    # Load constants
    addi s0, zero, 3    # a = 3
    addi s1, zero, 10   # b = 10
    addi s2, zero, 5    # c = 5
    addi s3, zero, 20   # d = 20

    # Calculation
    div  t0, s1, s2     # t0 = b / c (10 / 5 = 2)
    mul  t1, t0, s3     # t1 = (b / c) * d (2 * 20 = 40)
    mul  t2, t1, t1     # t2 = (40)^2 (1600)
    add  s6, s0, t2     # s6 = x = a + 1600 (1603)

    mv a0, s6                    # return 0
    li a7, 93                   # syscall 93 is 'exit'
    ecall
