.code

EXTERN Logger:PROC
EXTERN hookPtr:QWORD

callLogger PROC
    ; Сохраняем 15 регистров + 1 для выравнивания стека (всего 16)
    push r15 ; Просто для выравнивания
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    ; Shadow space (32 байта) - теперь стек выровнен идеально
    sub rsp, 20h        
    call Logger
    add rsp, 20h

    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    pop r15 ; Выкидываем заглушку
    
    ; Прыжок
    mov rax, hookPtr
    jmp rax
callLogger ENDP

END