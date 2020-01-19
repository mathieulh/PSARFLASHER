#function to call a syscall in kernel mode on 3.0+, this will take a pointer to a syscall,
#manually resolve it to an addr, and jalr to that addr
.set noreorder

#in kmode_funcs.s (ie fd = CallSyscallInKernel(&sceIoOpen,"ms0:/a.bin",PSP_O_WRONLY,0777);)
#int CallSyscallInKernel(unsigned long *func,...);

.global CallSyscallInKernel
.ent CallSyscallInKernel
CallSyscallInKernel:
    addiu $sp, -16
    sw    $ra, 16($sp)
#    sw    $k1, 12($sp) #Don't mess with k1
    sw    $t0, 8($sp)
    sw    $t1, 4($sp)
    sw    $s0, 0($sp)

    move  $v0, $a0
    lw    $t0, 0($v0)
    li    $t1, 0x54C  #syscall 0x15
    beq   $t0, $t1, error_ret
    lw    $v0, 4($v0) #get syscall opcode
    beqz  $v0, error_ret
    nop

    cfc0  $t0, $12    #first syscall table
    lw    $t0, 0($t0) #real syscall table
    lw    $t1, 4($t0) #offset to subtract from syscall opcode
    sll   $t1, 4      #t1 is now in opcode format
    sub   $v0, $t1

    addiu $t0, 16     #t0 points to first entry in the syscall table
    addiu $v0, -0xC   #get syscall number from opcode
    srl   $v0, 4      #$v0 is now an index to the syscall table starting at t0
    addu  $t0, $v0    #get syscall table addr containing function addr

    lw    $t5, 0($t0) #get the function addr that the syscall number refers to

    lw    $t0, 8($sp) #restore 1st two regs
    lw    $t1, 4($sp)

    move  $a0, $a1    #move all of the argument regs over one
    move  $a1, $a2
    move  $a2, $a3
    move  $a3, $t0
    move  $t0, $t1
    move  $t1, $t2
    move  $t2, $t3
    move  $t3, $t4 #gcc only allows a maximum of 8 arg regs,
                   #so the max we can pass to a syscall is 7,
                   #unless you call this in asm and set t4 for the last arg
    jalr  $t5
    nop

ret:
    lw    $s0, 0($sp)
#    lw    $k1, 12($sp) #Don't mess with k1
    lw    $ra, 16($sp)


    # incorporate setkernelpc functionality in here
    lui   $t4, 0x8000
    nop
    or    $ra, $t4



    jr    $ra
    addiu $sp, 16
error_ret:
    lui   $v0, 0x8002 #SCE_KERNEL_ERROR_LIBRARY_NOT_YET_LINKED
    b     ret
    ori   $v0, 0x13a
.end CallSyscallInKernel
