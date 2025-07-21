.include "macros.asm" ; include "macros.asm"

MOV RA, #0
CLC

LOOPUP: OUT RA
        INC RA
        JNC LOOPUP

LOOPDN: OUT RA
        DEC RA
        JNZ LOOPDN
        JMP LOOPUP




    