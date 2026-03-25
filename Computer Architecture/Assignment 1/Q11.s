.data
# Defining the array with 10 integers as provided in the C code [cite: 266]
arr: .word 12, -5, 23, 7, -18, 34, 0, -2, 15, 9

.text
.globl main
main:
    la   s0, arr          # Load base address of array into s0
    lw   s1, 0(s0)        # s1 = smallest = arr[0] 
    lw   s2, 0(s0)        # s2 = largest = arr[0] 
    
    addi t0, zero, 1      # t0 = i = 1 (loop counter) 
    addi t1, zero, 10     # t1 = size = 10 [cite: 266]

loop:
    # Check loop condition: if i >= 10, exit 
    bge  t0, t1, end_loop 
    
    # Calculate address of arr[i]: s0 + (i * 4)
    slli t2, t0, 2        # t2 = i * 4 (offset)
    add  t3, s0, t2       # t3 = address of arr[i]
    lw   t4, 0(t3)        # t4 = arr[i]
    
    # Check if arr[i] < smallest 
    bge  t4, s1, check_largest
    mv   s1, t4           # smallest = arr[i] 

check_largest:
    # Check if arr[i] > largest 
    ble  t4, s2, next_iteration
    mv   s2, t4           # largest = arr[i] 

next_iteration:
    addi t0, t0, 1        # i++ 
    j    loop

end_loop:
    # At this point:
    # s1 contains the smallest value (-18)
    # s2 contains the largest value (34)
    
    li   a7, 10           # Exit program
    ecall
