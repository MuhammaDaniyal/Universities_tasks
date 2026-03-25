.data
arr: .word 12, -5, 23, 7, -18, 34, 0, -2, 15, 9
smallest: .word 0
largest:  .word 0

.text
.globl main
main:
    la a0, arr              # a0 = arr base address 
    lw t0, 0(a0)            # Load arr[0] 
    
    la a3, smallest         # Load address of smallest
    sw t0, 0(a3)            # *smallest = arr[0] 
    
    la a4, largest          # Load address of largest
    sw t0, 0(a4)            # *largest = arr[0] 
    
    li a1, 10               # size = 10 
    li a2, 1                # index = 1 
    
    jal ra, findMinMax      # Call function 
    
    li a7, 10               # Exit program
    ecall

# ########## findMinMax (Recursive) ##########
findMinMax:
    # Base Case: if (index == size) return 
    beq a2, a1, done        
    
    # Save return address and registers on stack 
    addi sp, sp, -16
    sw ra, 0(sp)
    sw s0, 4(sp)
    
    # Get arr[index]
    slli t0, a2, 2          # t0 = index * 4
    add  t1, a0, t0         # t1 = address of arr[index]
    lw   s0, 0(t1)          # s0 = arr[index]
    
    # Check smallest: if (arr[index] < *smallest) 
    lw   t2, 0(a3)          # t2 = *smallest
    bge  s0, t2, skip_min
    sw   s0, 0(a3)          # *smallest = arr[index] 

skip_min:
    # Check largest: if (arr[index] > *largest) 
    lw   t3, 0(a4)          # t3 = *largest
    ble  s0, t3, skip_max
    sw   s0, 0(a4)          # *largest = arr[index] 

skip_max:
    # Prepare for recursion: findMinMax(arr, size, index + 1, smallest, largest) [cite: 297]
    addi a2, a2, 1          # index + 1
    jal ra, findMinMax      # Recursive call
    
    # Restore stack and return
    lw ra, 0(sp)
    lw s0, 4(sp)
    addi sp, sp, 16

done:
    jr ra                   # Return to caller