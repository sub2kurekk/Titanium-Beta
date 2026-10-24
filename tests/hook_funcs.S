# Test functions for hook_test.cpp. Each takes (rdi, rsi) and returns a value in
# rax. They differ in how their prologue looks, which is what the trampoline
# builder has to cope with. All are plain SysV leaf functions, padded so there
# is always enough code after the prologue.
        .intel_syntax noprefix
        .text

        .macro PAD
        .rept 24
        nop
        .endr
        .endm

# MSVC-style prologue: many pushes, then a 8-byte lea and a 7-byte sub, so the
# 14th byte falls in the middle of an instruction.
        .globl f_pushes
        .p2align 4
f_pushes:
        push rbp
        push rbx
        push r12
        push r13
        push r14
        push r15
        lea rbp, [rsp-0x100]
        sub rsp, 0x200
        lea rax, [rdi+rdi*2]
        add rax, rsi
        add rax, 7
        add rsp, 0x200
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        pop rbp
        ret
        PAD

# mov/sub with a 15-byte prefix where instruction 4 straddles byte 14.
        .globl f_straddle
        .p2align 4
f_straddle:
        mov qword ptr [rsp-64], rbx
        mov qword ptr [rsp-72], rsi
        push rdi
        sub rsp, 32
        lea rax, [rdi+rsi*4]
        add rax, 11
        add rsp, 32
        pop rdi
        mov rbx, qword ptr [rsp-64]
        ret
        PAD

# RIP-relative operands inside the prefix: both lea and a mov load.
        .globl f_riprel
        .p2align 4
f_riprel:
        lea rax, [rip+datum1]
        mov rcx, qword ptr [rip+datum2]
        add rax, rcx
        add rax, rdi
        mov rdx, qword ptr [rip+datum1]
        sub rax, rdx
        add rax, rsi
        ret
        PAD

# call rel32 as the very first instruction.
        .globl f_call
        .p2align 4
f_call:
        call helper_five
        add rax, rdi
        add rax, rsi
        mov rcx, rax
        lea rax, [rcx+rcx]
        ret
        PAD

        .p2align 4
helper_five:
        mov eax, 5
        ret
        PAD

# CET prologue (endbr64) and frame pointer setup.
        .globl f_endbr
        .p2align 4
f_endbr:
        endbr64
        push rbp
        mov rbp, rsp
        mov rax, rdi
        imul rax, rax, 13
        add rax, rsi
        pop rbp
        ret
        PAD

# SSE instructions and a movabs in the prefix.
        .globl f_sse
        .p2align 4
f_sse:
        movq xmm0, rdi
        movq xmm1, rsi
        paddq xmm0, xmm1
        mov r8, 0x1122334455667788
        movq rax, xmm0
        xor rax, r8
        ret
        PAD

# 66-prefixed and REX-prefixed instructions.
        .globl f_prefixes
        .p2align 4
f_prefixes:
        xchg ax, ax
        mov r9w, 0x1234
        movzx eax, r9w
        mov r10, rdi
        add r10, rsi
        xor r11d, r11d
        lea rax, [rax+r10+3]
        ret
        PAD

# ---- functions the builder must REFUSE to hook ----

# short relative branch in the prefix
        .globl f_rel8
        .p2align 4
f_rel8:
        test rdi, rdi
        jz 1f
        mov rax, rsi
        ret
1:      mov rax, rdi
        ret
        PAD

# starts with an unconditional jump (already hooked / thunk)
        .globl f_thunk
        .p2align 4
f_thunk:
        jmp helper_five
        PAD

# too short: returns inside the 14 byte window
        .globl f_short
        .p2align 4
f_short:
        mov rax, rdi
        ret
        PAD

        .data
        .p2align 3
        .globl datum1
datum1: .quad 0x1000
        .globl datum2
datum2: .quad 0x20
        .att_syntax prefix
        .section .note.GNU-stack,"",@progbits
