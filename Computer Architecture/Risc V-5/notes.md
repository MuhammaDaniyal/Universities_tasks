📝 PROGRAM STRUCTURE
assembly
# Comments start with #
.globl main              # Make main visible to linker

.text                    # Code section starts here
main:                    # Program entry point
    # Instructions go here
    
    # Exit program
    li a0, 0            # Return value (0 = success)
    li a7, 93           # 93 = exit syscall
    ecall               # Do the syscall

.data                    # Data section (variables)
    # Variables go here



🎯 REGISTERS (The 7 You Need Now)
Register	Name	Purpose
t0, t1, t2, t3, t4, t5, t6	Temporary	Scratch registers - use for calculations
a0, a1	Arguments	Return value goes in a0. Also function arguments
a7	Syscall	Syscall number (93=exit, 64=write)
sp	Stack Pointer	Stack - points to top of stack (don't touch yet)
ra	Return Address	Where to return (for functions)
Remember: t0-t6 are your workhorses. a0 is what gets returned.

🎯 REGISTERS (The 7 You Need Now)
Register	Name	Purpose
t0, t1, t2, t3, t4, t5, t6	Temporary	Scratch registers - use for calculations
a0, a1	Arguments	Return value goes in a0. Also function arguments
a7	Syscall	Syscall number (93=exit, 64=write)
sp	Stack Pointer	Stack - points to top of stack (don't touch yet)
ra	Return Address	Where to return (for functions)
Remember: t0-t6 are your workhorses. a0 is what gets returned.

