bits 32
section .text
        align 4
        dd 0x1BADB002
        dd 0x00
        dd - (0x1BADB002 + 0x00)

global start
global keyboard_handler
global timer_handler
global read_port
global write_port
global load_idt

extern kmain
extern keyCall
extern timer

read_port:
	mov edx, [esp + 4]
	in al, dx
	ret

write_port:
	mov edx, [esp + 4]    
	mov al, [esp + 4 + 4]  
	out dx, al  
	ret

load_idt:
	mov edx, [esp + 4]
	lidt [edx]
	sti
	ret

timer_handler:
    call timer
    iretd

keyboard_handler:                 
	call keyCall
	iretd

start:
	cli
	call kmain
	hlt