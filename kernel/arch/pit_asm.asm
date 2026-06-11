global pit_handler_asm
extern pit_handler

pit_handler_asm:
    pusha
    call pit_handler
    popa
    iret
