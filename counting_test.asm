.include "macros.asm" ; include "macros.asm"

MOV RA, #5
CLC

LOOPUP: 
        OUT RA
        INC RA
        JNC LOOPUP
        CLC
        JMP LOOPDN

LOOPDN: 
        OUT RA
        DEC RA
        JNZ LOOPDN
        JMP LOOPUP




    