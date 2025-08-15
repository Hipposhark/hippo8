; assembler macros

.macro THISISATESTMACRO
    NOP
    HLT
.endm

.macro INC RX
    ADD RX, #1
.endm

.macro DEC RX
    SUB RX, #1
.endm

