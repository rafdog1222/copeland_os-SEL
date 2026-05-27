global keyboard_handler_asm
extern keyboard_handler

keyboard_handler_asm:
    pusha               ; qn save all registers
    call keyboard_handler
    popa                ; qn restore all registers
    iret                ; qn return from interrupt
