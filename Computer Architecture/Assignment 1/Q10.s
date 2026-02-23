# int i = 1, j;
# for (i = 1; i <= 3; i++) {
#   j = 1;
#   cout<<”Outer loop iteration”;
#   while (j <= 2)
#   {
#       cout<<j;
#       j++;
#   }
# }

.data 
outer_msg: .asciz "Outer loop iteration\n"
outer_len = . - outer_msg

.text
.global main
main:

    li s0, 1        # i
    li s1, 3        # const 3
    li s3, 2        # const 2

    FOR_LOOP:
        bgt s0, s1, FOR_END
        addi s0, s0, 1

        li s2, 1    # j

        # print
        li a0, 1
        la a1, outer_msg
        li a2, outer_len
        li a7, 64
        ecall

            WHILE_LOOP:
                bgt s2, s3, WHILE_END

                li a0, 1       # Move j into a0
                mv a1, s2
                li a2, 1                 # print exactly 1 byte
                li a7, 1        # Syscall 1 is typically "print_int"
                ecall

                addi s2, s2, 1
                j WHILE_LOOP

            WHILE_END:

        j FOR_LOOP

        FOR_END:


    end:
