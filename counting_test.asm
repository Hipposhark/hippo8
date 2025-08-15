.include "macros.asm" ; include "macros.asm"

RESET:
        MOV RA, #5
        CLC

LOOP:
        JMP LOOPUP

LOOPUP: 
        JSR SUBROUTINE_TEST
        INC RA
        JNC LOOPUP
        CLC
        JMP LOOPDN

LOOPDN: 
        JSR SUBROUTINE_TEST
        DEC RA
        JNZ LOOPDN
        JMP LOOPUP

SUBROUTINE_TEST:
        OUT RA
        RTS






    