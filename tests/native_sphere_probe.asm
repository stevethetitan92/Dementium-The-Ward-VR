OPTION CASEMAP:NONE
EXTERN sphereTestTarget:QWORD
.code
sphereTestDiagnostic PROC
    jmp qword ptr [sphereTestTarget]
sphereTestDiagnostic ENDP
clobberSphere PROC
    xor edx,edx
    xor r8d,r8d
    xor r9d,r9d
    xor r10d,r10d
    xor r11d,r11d
    mov eax,3f000000h
    movd xmm0,eax
    ret
clobberSphere ENDP
; probe(function, {planes,center,radius}, output)
probeSphere PROC FRAME
    sub rsp,48h
    .allocstack 48h
    .endprolog
    mov [rsp+20h],r8
    mov rax,rcx
    mov rcx,[rdx]
    movss xmm2,dword ptr [rdx+10h]
    mov rdx,[rdx+8]
    mov r8,12345678h
    mov r9,23456789h
    mov r10,3456789ah
    mov r11,456789abh
    call rax
    mov rax,[rsp+20h]
    mov [rax],rdx
    mov [rax+8],r8
    mov [rax+10h],r9
    mov [rax+18h],r10
    mov [rax+20h],r11
    movss dword ptr [rax+28h],xmm0
    add rsp,48h
    ret
probeSphere ENDP
END
