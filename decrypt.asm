section .data
    filename db "system_data.dat", 0
    key      db 0x42                ; Samma nyckel som i din C-kod
    buffer_size equ 1024

section .bss
    buffer resb buffer_size         ; Plats för att läsa in filinnehåll

section .text
    global _start

_start:
    ; 1. Öppna filen (sys_open)
    mov rax, 2          ; syscall nummer för sys_open
    mov rdi, filename   ; filnamn
    mov rsi, 0          ; O_RDONLY (endast läsning)
    mov rdx, 0
    syscall

    ; Spara file descriptor (fd) i r12
    mov r12, rax

    ; 2. Läs från filen (sys_read)
    mov rax, 0          ; syscall nummer för sys_read
    mov rdi, r12        ; fd
    mov rsi, buffer     ; buffer att skriva till
    mov rdx, buffer_size
    syscall

    ; Spara antal lästa bytes i r13
    mov r13, rax

    ; 3. Dekryptera (XOR-loop)
    mov rcx, 0          ; räknare (index)
decrypt_loop:
    cmp rcx, r13        ; Har vi gått igenom alla bytes?
    je print_result
    
    mov al, [buffer + rcx] ; Ladda en byte från bufferten
    xor al, [key]          ; XOR med 0x42
    mov [buffer + rcx], al ; Spara tillbaka i bufferten
    
    inc rcx
    jmp decrypt_loop

print_result:
    ; 4. Skriv ut till terminalen (sys_write)
    mov rax, 1          ; syscall nummer för sys_write
    mov rdi, 1          ; stdout
    mov rsi, buffer
    mov rdx, r13        ; antal bytes att skriva
    syscall

    ; 5. Stäng filen (sys_close)
    mov rax, 3
    mov rdi, r12
    syscall

    ; 6. Avsluta programmet (sys_exit)
    mov rax, 60
    xor rdi, rdi
    syscall
