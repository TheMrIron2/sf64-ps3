    .text
    .set noreorder
    .set noat
    .align 2
    .macro dispatch index
    .globl PspScratchpad_Dispatch\index
    .type PspScratchpad_Dispatch\index, @function
PspScratchpad_Dispatch\index:
    lui $v0, %hi(gPspScratchpadTargets + 4 * \index)
    lw $v0, %lo(gPspScratchpadTargets + 4 * \index)($v0)
    jr $v0
    nop
    .size PspScratchpad_Dispatch\index, .-PspScratchpad_Dispatch\index
    .endm
    dispatch 0
    dispatch 1
    dispatch 2
