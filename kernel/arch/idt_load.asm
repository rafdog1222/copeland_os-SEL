global idt_load
global isr_stub_asm

idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret

isr_stub_asm:
    pusha
    popa
    iret
