.include "macros.asm" ; include "macros.asm"


LCD_PORT = 0b00
SPI_PORT = 0b01
PS2_PORT = 0b10

LCD_ENABLE          = 0b00010000
LCD_RW              = 0b00100000
LCD_RS              = 0b01000000
display_light_state = 0b10000000 ; note: variable is located in memory in memory

RESET:

    ; MOV RA, #0b00110000 ; 
    ; DISPLAY_SEND_BYTE

    ; ; MOV RA, #0b00110000
    ; ; DISPLAY_SEND_BYTE

    ; ; MOV RA, #0b00100000
    ; ; DISPLAY_SEND_BYTE

    ; ; MOV RA, #0b00101000 ; screen mode, two lines, small font
    ; ; DISPLAY_SEND_BYTE

    ; ; MOV RA, #0b00000001 ; clear screen
    ; ; DISPLAY_SEND_BYTE

    ; ; MOV RA, #0b00001111 ; display on, cursor on, cursor blink
    ; ; DISPLAY_SEND_BYTE


    ; reference datasheet example "4-Bit Operation, 8-Digit × 1-Line Display Example with Internal Reset"
    MOV RA, #0b0010 
    JSR DISPLAY_SEND_HALF_BYTE ; set to 4-bit operation

    ; MOV RA, #0b00100000 
    MOV RA, #0b00101000 
    JSR DISPLAY_SEND_FULL_BYTE ; set to 4-bit operation, 2-line display, 5x8 dot char. font

    MOV RA, #0b00001111 
    JSR DISPLAY_SEND_FULL_BYTE ; turns on display, cursor, and blink

    MOV RA, #0b00000110        ; increment & shift cursor (don't shift display)
    JSR DISPLAY_SEND_FULL_BYTE

    ;
    MOV RA, #0b01001000 ; 'H'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01100101 ; 'e'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01101100 ; 'l'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01101100 ; 'l'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01101111 ; 'o'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b00100000 ; ' '
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01010111 ; 'W'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01101111 ; 'o'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01110010 ; 'r'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01101100 ; 'l'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b01100100 ; 'd'
    JSR DISPLAY_PRINT_CHAR
    
    MOV RA, #0b00100001 ; '!'
    JSR DISPLAY_PRINT_CHAR

LOOP:
    JMP LOOP



DISPLAY_SEND_HALF_BYTE: ; half byte to send is in lower A reg.
    PSF
    SII
    
    AND RA, #0x0F
    OR  RA, #display_light_state
    OUT #LCD_PORT, RA
    OR  RA, #LCD_ENABLE
    OUT #LCD_PORT, RA
    XOR RA, #LCD_ENABLE
    OUT #LCD_PORT, RA

    ; restore interrupt inhibit state
    PPF 

    RTS

; https://youtu.be/CZbrbMHxtxY?t=1179
DISPLAY_SEND_FULL_BYTE: ; full byte to send is in A reg
    ; save interrupt inhibit state
    PSF
    SII

    PSH RA ; save RA cuz we are about to modify the byte for 4-bit mode
    ; send upper nibble
    LSR RA
    LSR RA
    LSR RA
    LSR RA
    AND RA, #0x0F

    OR  RA, #display_light_state
    OUT #LCD_PORT, RA

    OR  RA, #LCD_ENABLE ; enable on
    OUT #LCD_PORT, RA
    XOR RA, #LCD_ENABLE ; enable off
    OUT #LCD_PORT, RA

    ;send lower nibble
    POP RA
    AND RA, #0x0F
    OR  RA, #display_light_state
    OUT #LCD_PORT, RA
    OR  RA, #LCD_ENABLE
    OUT #LCD_PORT, RA
    XOR RA, #LCD_ENABLE
    OUT #LCD_PORT, RA

    ; restore interrupt inhibit state
    PPF 
    RTS


DISPLAY_RECEIVE_BYTE:
    RTS

DISPLAY_PRINT_CHAR: ; character code byte to send is in A reg
    PSF
    SII

    PSH RA ; save RA cuz we are about to modify the byte for 4-bit mode
    ; send upper nibble
    LSR RA
    LSR RA
    LSR RA
    LSR RA
    AND RA, #0x0F

    OR  RA, #display_light_state
    OR  RA, #LCD_RS
    OUT #LCD_PORT, RA
    OR  RA, #LCD_ENABLE ; enable on
    OUT #LCD_PORT, RA
    XOR RA, #LCD_ENABLE ; enable off
    OUT #LCD_PORT, RA

    ;send lower nibble
    POP RA
    AND RA, #0x0F
    OR  RA, #display_light_state
    OR  RA, #LCD_RS
    OUT #LCD_PORT, RA
    OR  RA, #LCD_ENABLE
    OUT #LCD_PORT, RA
    XOR RA, #LCD_ENABLE
    OUT #LCD_PORT, RA

    ; restore interrupt inhibit state
    PPF 
    RTS

    RTS



; DISPLAY_CHAR: ; character code byte to send is in A reg
;     STC
;     CMP RA, #8 ; backspace character?
;     JZS DISPLAY_BACKSPACE
    
;     STC
;     CMP RA, #10 ; new line character?
;     JZS .new_line

;     STC
;     CMP RA, #13 ; carriage return character?
;     JZS .end

;     JSR DISPLAY_SEND_DATA_BYTE

;     MOV RA, display_cursor_col
;     CLC
;     ADD RA, #1
;     STC
;     CMP RA, #LCD_NUM_COLS
;     JZS .col_end
;     MOV display_cursor_col, RA

;     RTS
; .end: 
;     RTS
; .col_end:
;     JSR DISPLAY_MOVE_CURSOR_LEFT_INTERN
; .new_line:
;     PSH RB
;     PSH RC
;     PSH RD
;     JSR DISPLAY_NEW_LINE
;     JSR DISPLAY_WAIT_NOT_BUSY
;     PUL RD
;     PUL RC
;     PUL_RB
;     RTS