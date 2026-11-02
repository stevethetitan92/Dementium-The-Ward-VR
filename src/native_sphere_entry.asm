; Native 7B7F0 is called with compiler-known register preservation beyond the
; Windows ABI: RDX and R8-R11 are untouched. A46xx keeps its manager in R10
; across the call and dereferences it at A4931. Keep that native contract when
; entering C++, without changing XMM0 (the visibility result).
OPTION CASEMAP:NONE
EXTERN sphereTestDiagnostic:PROC
.code
nativeSphereEntry PROC FRAME
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    mov [rsp+20h], rdx
    mov [rsp+28h], r8
    mov [rsp+30h], r9
    mov [rsp+38h], r10
    mov [rsp+40h], r11
    call sphereTestDiagnostic
    mov rdx, [rsp+20h]
    mov r8, [rsp+28h]
    mov r9, [rsp+30h]
    mov r10, [rsp+38h]
    mov r11, [rsp+40h]
    add rsp, 48h
    ret
nativeSphereEntry ENDP
END
