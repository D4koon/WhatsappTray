.code
;To get a registervalue, we need to write the needed register in rax because after 'ret' the function returns whatever is in the rax-register

ReturnRdx proc
	mov rax, rdx
	ret
ReturnRdx endp

ReturnRcx proc
	mov rax, rcx
	ret
ReturnRcx endp

ReturnRdi proc
	mov rax, rdi
	ret
ReturnRdi endp

ReturnR8 proc
	mov rax, r8
	ret
ReturnR8 endp

ReturnR9 proc
	mov rax, r9
	ret
ReturnR9 endp

end