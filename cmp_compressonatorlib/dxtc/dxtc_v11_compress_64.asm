;===============================================================================
; Copyright (c) 2004-2006 ATI Technologies Inc.
;===============================================================================


AXIS_MUNGE                equ 1
PROGRESSIVE_REFINEMENT    equ 1

.DATA

ALIGN 16


EXTERN zero                : XMMWORD
EXTERN one                : XMMWORD
EXTERN one_over_16        : XMMWORD
EXTERN one_over_16_x_255_zeros    : XMMWORD
EXTERN clearsign        : XMMWORD
EXTERN signbit            : XMMWORD
EXTERN half                : XMMWORD
EXTERN c255                : XMMWORD
EXTERN c3                : XMMWORD
EXTERN lots                : XMMWORD
EXTERN minuslots        : XMMWORD

EXTERN mask_green        : MMWORD
EXTERN mask_blue_red    : MMWORD
EXTERN clamp_0            : MMWORD
EXTERN clamp_255        : MMWORD
EXTERN green_rounding    : MMWORD
EXTERN blue_red_rounding: MMWORD

EXTERN rb_sign_bits        : XMMWORD

EXTERN stepsize            : XMMWORD
EXTERN onethird            : XMMWORD

EXTERN maxerror_init    : DWORD
EXTERN maxerror_epsilon    : DWORD

EXTERN b_half            : XMMWORD
EXTERN b_2x                : XMMWORD

EXTERN invert            : XMMWORD


EXTERN split_point        : XMMWORD
EXTERN deviation_point    : XMMWORD


EXTERN expandtable        : DWORD



EXTERN expandtable        : DWORD


EXTERN _0f0f0f0f0f0f0f0f : MMWORD
EXTERN _0707070707070707 : MMWORD
EXTERN _000f000f000f000f : MMWORD
EXTERN _00f000f000f000f0 : MMWORD


.CODE


PARALLEL_ADD_3D        MACRO reg, tmp1, tmp2
    movaps tmp1,reg
    movhlps tmp2,reg
    shufps reg,reg, 055h
    addss tmp1,tmp2
    addss reg,tmp1

    ENDM

SHUFFLE_SELECT        MACRO a,b,c,d
    LOCAL Value
    IF (a gt 3) or (b gt 3) or (c gt 3) or (d gt 3)
        .ERR
        EXITM <0>
    ENDIF
    Value = ((a) OR (b SHL 2) OR (c SHL 4) OR (d SHL 6))
    EXITM %Value
    ENDM



; x64 uses register calling conventions
; The first four parameters are put in rcx, rdx, r8, r9 (floats would be in xmm0-3)
; rax, r10, r11, xmm4 and xmm5 are volatile in addition to the above - all others must be saved

; void __cdecl DXTCCompressBlockSSE(DWORD *block_32, DWORD *block_dxtc);
; block_dxtc == rdx, how convenient!



DXTCV11CompressBlockSSE PROC 

            push rsi    
            push rdi
            push rbx
            push rbp

            ; Set up a 16-byte aligned storage space pointer
            mov rbp, rsp
            and rbp, NOT 0fh

SAVED_REGS            equ 3
TMP_REGSAVE            equ (SAVED_REGS*16)

            ; Any xmm regs over 5 need to be saved here as well
            movaps [rbp-TMP_REGSAVE-( 0*16)], xmm6
            movaps [rbp-TMP_REGSAVE-( 1*16)], xmm7
            movaps [rbp-TMP_REGSAVE-( 2*16)], xmm8


; Other locals
TMP_RGB             equ (TMP_REGSAVE+(16*16))
TMP_POS_ON_AXIS        equ (TMP_RGB+(16*4))

TMP_CLUSTERPOS        equ (TMP_POS_ON_AXIS+16)
TMP_OLDLEFT            equ (TMP_CLUSTERPOS+16)
TMP_OLDRIGHT        equ (TMP_OLDLEFT+16)
TMP_RIGHTSAVE        equ (TMP_OLDRIGHT+16)
TMP_AXISSAVE        equ (TMP_RIGHTSAVE+16)
TMP_AVGSAVE         equ (TMP_AXISSAVE+16)

TMP_CLUSTERS        equ (TMP_AVGSAVE+(16*4))

TMP_MAXERROR        equ (TMP_CLUSTERS+16)
TMP_SWAP            equ (TMP_MAXERROR+4)


        ; Convert the pixel values to float and find their average position

            xorps xmm0, xmm0        ; average (r, g, b)

            ; The input data is in rcx
            lea rdi, [rbp-TMP_RGB]
            mov eax, 16
            xorps xmm2,xmm2

            average_loop:
                movd xmm7, dword ptr [rcx]
                punpcklbw xmm7,xmm2
                punpcklwd xmm7,xmm2
                cvtdq2ps xmm7,xmm7
                add rcx, 4

IF AXIS_MUNGE
                ; colourspace conversion to increase weight of G at expense of B
                movaps xmm6, xmm7
                shufps xmm6, xmm6, SHUFFLE_SELECT(1, 1, 1, 1)    ; G
                addss xmm7, xmm6
                mulss xmm7, dword ptr [b_half]
ENDIF

                movaps [rdi], xmm7        ; save the value off in the RGB float array
                add edi, 16
                addps xmm0, xmm7        ; accumulate average

                sub eax, 1
                jne average_loop

            ; Compute average of the values
            mulps xmm0, [one_over_16]



        ; For each component, reflect points about the average so all lie on the same side
        ; of the average, and compute the new average - this gives a second point that defines the axis
        ; To compute the sign of the axis sum the positive differences of G for each of R and B (the
        ; G axis is always positive in this implementation

            xorps xmm1, xmm1        ; axis (r, g, b)
            xorps xmm2, xmm2        ; rg_pos, rb_pos, bg_pos
            lea rdi, [rbp-TMP_RGB]

            mov ecx, 16
            find_axis_loop:
                movaps xmm7, [rdi]        ; R G B value
                add rdi, 16
                subps xmm7, xmm0        ; subtract average - centred
                movaps xmm6, xmm7
                movaps xmm5, xmm7

                andps xmm7, [clearsign]    ; fabs (r, g, b)
                addps xmm1, xmm7        ; accumulate direction of axis

                shufps xmm6, xmm6, SHUFFLE_SELECT(0, 2, 2, 3)    ; B R R 0
                shufps xmm5, xmm5, SHUFFLE_SELECT(1, 0, 1, 3)    ; G B G 0

                cmpnltps xmm6, [zero]    ; R/B > 0?
                andps xmm6, xmm5        ; insert the G or B value for those channels which are positive
                addps xmm2, xmm6        ; bg_pos rb_pos rg_pos

                sub ecx, 1
                jne find_axis_loop


            ; Handle RB_pos - RB pos is used if RG_pos and BG_pos are both zero.
            movaps xmm5, xmm2    ; duplicate the pos across these three
            movaps xmm6, xmm2
            movaps xmm7, xmm2
            shufps xmm5, xmm5, SHUFFLE_SELECT(1, 3, 3, 3)    ; RB_pos 0 ->
            shufps xmm6, xmm6, SHUFFLE_SELECT(2, 2, 2, 2)    ; RG_pos ->
            shufps xmm7, xmm7, SHUFFLE_SELECT(0, 0, 0, 0)    ; BG_pos ->
            orps xmm6, xmm7
            cmpneqps xmm6, [zero]    ; so check for any non-zero in RG_pos or BG_pos
            andps xmm2, xmm6        ; Mask out RG_pos in current if we need to the current
            xorps xmm6, [invert]
            andps xmm5, xmm6
            orps xmm2, xmm5            ; insert RB pos instead


            ; Change the sign of the R and B portions of the axis appropriately
            cmpltps xmm2, [zero]
            andps xmm2, [rb_sign_bits]
            xorps xmm1, xmm2            ; Flip the sign of the axis if the r/g or b/g tests indicate a negative slope



        ; Axis projection and remapping

            ; Normalise the axis for simplicity of future calculation
            movaps xmm7, xmm1

            mulps xmm7, xmm7
            PARALLEL_ADD_3D xmm7    ,xmm6,xmm5

            ; low of xmm7 is the DP result
            ; If this is 0 we haven't actually got an axis, and we can't rsq it,
            ; so mask the output to 0 in this case. This generates an acceptable result
            movss xmm2, xmm7
            cmpneqss xmm2, dword ptr [zero]

            ; RSQRT with Newton-Raphson. This can be omitted for even faster encoding performance, but quality
            ; and consistency improves with it in on certain images. It's not a large cost so leave it in.
            rsqrtss xmm3, xmm7
            andps xmm3, xmm2

            movss    xmm2, xmm3
            mulss    xmm3, xmm7
            mulss    xmm3, xmm2
            mulss    xmm2, dword ptr [half]
            movss    xmm7, dword ptr [c3]
            subss    xmm7, xmm3
            mulss    xmm7, xmm2

            shufps xmm7, xmm7, SHUFFLE_SELECT(0, 0, 0, 0)

            ; Normalise
            mulps xmm1, xmm7


        ; Map the axis

            lea rsi, [rbp-TMP_RGB]
            lea rdi, [rbp-TMP_POS_ON_AXIS]
            mov ecx, 16
            movaps xmm2, [lots]         ; left
            movaps xmm3, [minuslots]    ; right

            map_axis_loop:
                movaps xmm7, [rsi]
                subps xmm7, xmm0    ; subtract average

                ; dot product with axis
                mulps xmm7, xmm1
                PARALLEL_ADD_3D xmm7    ,xmm6,xmm5

                ; xmm7 == pos_on_axis

                movss dword ptr [rdi], xmm7
                add rdi, 4

                minss xmm2, xmm7        ; calculate left
                maxss xmm3, xmm7        ; calculate right

                add rsi, 16

                sub ecx, 1
                jne map_axis_loop


        ; We have a good axis and the basic information about how the points are mapped to it
        ; We need to calculate the endpoints - the initial guess is to use the extremities.

            ; Left and right are used across all the simds
            shufps xmm2, xmm2, SHUFFLE_SELECT(0, 0, 0, 0)   ; left
            shufps xmm3, xmm3, SHUFFLE_SELECT(0, 0, 0, 0)   ; right


        ; To simplify further calculations, we offset everything such that the axis centre is at 0

            ; Calculate centre
            movaps xmm7, xmm2   ; left
            addps xmm7, xmm3    ; right
            mulps xmm7, [half]

            ; Offset all the axis positions to the centre
            lea rdi, [rbp-TMP_POS_ON_AXIS]
            movaps xmm5, [rdi]
            movaps xmm6, [rdi+16]
            subps xmm5, xmm7
            subps xmm6, xmm7
            movaps [rdi], xmm5
            movaps [rdi+16], xmm6
            movaps xmm5, [rdi+32]
            movaps xmm6, [rdi+48]
            subps xmm5, xmm7
            subps xmm6, xmm7
            movaps [rdi+32], xmm5
            movaps [rdi+48], xmm6

            ; Offset left, right and average to centre
            subps xmm2, xmm7    ; left
            subps xmm3, xmm7    ; right

            mulps xmm7, xmm1    ; convert to rgb by multiplying by axis
            addps xmm0, xmm7    ; average



IF PROGRESSIVE_REFINEMENT

        ; Attempt a (simple) progressive refinement step to reduce noise in the
        ; output image by trying to find a better overall match for the endpoints
        ; than the first-guess solution found so far (which is just to take the ends.

            ; The method is to move the endpoints inwards until a local minima is found.
            ; This provides quite a significant improvement in image quality.

            mov eax, [maxerror_init]
            mov [rbp-TMP_MAXERROR], eax

            movaps [rbp-TMP_OLDLEFT], xmm2            ; XXX register space expansion
            movaps [rbp-TMP_OLDRIGHT], xmm3            ; XXX register space expansion

            lea rdi, [rbp-TMP_CLUSTERPOS]

            next_refinement_loop:

                movaps [rbp-TMP_RIGHTSAVE], xmm3    ; XXX register space expansion

                xorps xmm8,xmm8                        ; Error - clear the error

                ; Update the array of cluster positions based on the new values of left and right
                movss xmm7, xmm2        ; left
                addss xmm7, xmm3        ; right
                mulss xmm7, dword ptr [half]
                movss dword ptr [rdi], xmm2       ; left
                movss dword ptr [rdi+4], xmm3     ; right
                movss xmm5, xmm3        ; right
                movss xmm6, xmm7
                subss xmm5, xmm7        ; right-centre
                movss xmm4, xmm7
                movss xmm3, xmm5
                mulss xmm5, dword ptr [deviation_point]
                subss xmm6, xmm5
                addss xmm4, xmm5
                movss dword ptr [rdi+8], xmm6
                movss dword ptr [rdi+12], xmm4

                mulss xmm3, dword ptr [split_point]


                ; Calculate the current error
                mov ecx, 16
                lea rsi, [rbp-TMP_POS_ON_AXIS]
                next_builderror_loop:
                    movss xmm4, dword ptr [rsi]
                    add rsi, 4
                    movaps xmm5, xmm4
                    movaps xmm6, xmm4
                    andps xmm4, [clearsign]
                    cmpltss xmm4, xmm3        ; < division means 2
                    cmpnltss xmm5, xmm7        ; >= centre means add 1

                    movmskps eax, xmm4
                    movmskps ebx, xmm5
                    lea eax, [ebx+2*eax]        ; rax == which cluster (top 32 bits are cleared by this)

                    subss xmm6, dword ptr [rdi+4*rax]     ; rdi = array of positions along axis of cluster
                    mulss xmm6, xmm6            ; square to use MSE and (conveniently eliminate sign)
                    addss xmm8, xmm6            ; Accumulate to the MSE

                    sub ecx, 1
                    jne next_builderror_loop


                movaps xmm3, [rbp-TMP_RIGHTSAVE]    ; restore the right XXX register space expansion

                ; Test and update the maximum error
                movss xmm5, dword ptr [rbp-TMP_MAXERROR]
                cmpltss xmm5, xmm8
                movmskps eax, xmm5
                test eax,1
                jnz refinement_done


                subss xmm6, [maxerror_epsilon]
                movss dword ptr [rbp-TMP_MAXERROR], xmm8

                movaps [rbp-TMP_OLDLEFT], xmm2      ; left    XXX register space expansion
                movaps [rbp-TMP_OLDRIGHT], xmm3     ; right XXX register space expansion

                ; step left and right in a bit
                mulps xmm2, [stepsize]    ; XXX promote to register to improve code density?
                mulps xmm3, [stepsize]

                jmp next_refinement_loop

            refinement_done:
                movaps xmm2, [rbp-TMP_OLDLEFT]      ; left
                movaps xmm3, [rbp-TMP_OLDRIGHT]     ; right
ENDIF

        ; Endpoints and axis are now valid so we have all the information we need to compress the block


        ; Calculate the high and low output colour values

            ; Involved in this is a complex rounding procedure.
            ; A straight rounded average is not correct, as the decompressor unrounds by replicating
            ; the top bits to the bottom.

            ; In order to take account of this process, we don't just apply a straight rounding correction,
            ; but base our rounding on the input value (a straight rounding is actually pretty good in terms of
            ; error measure, but creates a visual colour and/or brightness shift relative to the original image)
            ; The method used here is to apply a centre-biased rounding dependent on the input value, which was
            ; (mostly by experiment) found to give minimum MSE while preserving the visual characteristics of
            ; the image.
            ; rgb = (average_rgb + (left|right)*v_rgb);
            movaps xmm6, xmm2       ; left
            movaps xmm7, xmm3       ; right
            mulps xmm6, xmm1        ; axis
            mulps xmm7, xmm1
            addps xmm6, xmm0        ; average
            addps xmm7, xmm0

IF AXIS_MUNGE
            ; Scale the B component, then subtract the green component resultant in each
            movaps xmm4, xmm6
            movaps xmm5, xmm7
            mulps xmm6, [b_2x]        ; XXX promote to register to improve code density?
            mulps xmm7, [b_2x]
            shufps xmm4, xmm4, SHUFFLE_SELECT(1, 1, 1, 1)
            shufps xmm5, xmm5, SHUFFLE_SELECT(1, 1, 1, 1)
            subss xmm6, xmm4
            subss xmm7, xmm5
ENDIF

            ; Rearrange so B and R are in the same register half (they both use 5-bit rounding)
            shufps xmm6, xmm6, SHUFFLE_SELECT(0, 2, 1, 3)    ; B R G
            shufps xmm7, xmm7, SHUFFLE_SELECT(0, 2, 1, 3)

            ; Convert to integer (by truncation, as C code does)
            cvttps2pi mm0, xmm6
            cvttps2pi mm1, xmm7
            movhlps xmm6, xmm6
            movhlps xmm7, xmm7
            cvttps2pi mm2, xmm6
            cvttps2pi mm3, xmm7

            ; mm0/1 is blue/red, mm2/3 is green

            ; This isn't quite the same as the C algorithm, but should generate the same result
            ; if the input range is ensured to be 0-255

            ; This code could be heavily interleaved, but for P4 it's not worth the hassle - the
            ; P4 reordering range of 15 instructions will let it do the job for us

            ; XXX promote all these consts to registers to improve code density?
            pmaxsw mm0, [clamp_0]        ; Note: faster to do these max/min in MMX than float XMM - better reordering opportunities
            pmaxsw mm1, [clamp_0]
            pminsw mm0, [clamp_255]
            pminsw mm1, [clamp_255]
            movq mm6, mm0
            movq mm7, mm1
            paddd mm0, [blue_red_rounding]
            paddd mm1, [blue_red_rounding]
            psrld mm6, 5
            psrld mm7, 5
            psubd mm0, mm6
            psubd mm1, mm7
            ; No need to clamp here, with the input in 0-255 range it can never be outside at the end
            pand mm0, [mask_blue_red]
            pand mm1, [mask_blue_red]

            ; Separate out R and B as they will need separate shifts later
            pshufw mm4, mm0, SHUFFLE_SELECT(2, 3, 2, 3)    ; extract R    (this is an SSE, not MMX, instruction)
            pshufw mm5, mm1, SHUFFLE_SELECT(2, 3, 2, 3)    ; also R

            pmaxsw mm2, [clamp_0]
            pmaxsw mm3, [clamp_0]
            pminsw mm2, [clamp_255]
            pminsw mm3, [clamp_255]
            movq mm6, mm2
            movq mm7, mm3
            paddd mm2, [green_rounding]
            paddd mm3, [green_rounding]
            psrld mm6, 6
            psrld mm7, 6
            psubd mm2, mm6
            psubd mm3, mm7
            pand mm2, [mask_green]
            pand mm3, [mask_green]


            ; Convert the 8-bit values to final RGB565 colours in mm0 and mm1
            psrld mm0, 3
            psrld mm1, 3
            pslld mm4, 8
            pslld mm5, 8
            pslld mm2, 3
            pslld mm3, 3
            por mm0, mm4
            por mm1, mm5
            por mm0, mm2
            por mm1, mm3

            ; mm0 and mm1 are c0 and c1


            ; Need to compare c0/c1 for sign and flip and set swap if required - and handle colour equality as well....

            ; rdx contains the destination DXTC block (dx == DXTC)

            pxor mm5, mm5
            punpcklwd mm0, mm5    ; unpack c0/c1 to DWORD's as pcmp is a signed comparison
            punpcklwd mm1, mm5
            movq mm2, mm0
            movq mm3, mm0
            movq mm4, mm0
            pcmpgtd mm2, mm1
            pxor mm2, qword ptr [invert]    ; Need less than, so flip the result
            movd dword ptr [rbp-TMP_SWAP], mm2    ; Set the swap flag (used below) appropriately)
            ; mm2 is the mask to indicate flipping is needed

            pcmpeqd mm4, mm1
            movd ebx, mm4        ; ebx is the equality flag, plenty of time for this slow move to resolve

            punpcklwd mm0, mm1    ; 'normal' order
            punpcklwd mm1, mm3    ; reversed order
            pand mm1, mm2
            pandn mm2, mm0
            por mm1, mm2        ; one of the two, selected by mm2
            movd dword ptr [rdx], mm1        ; write the result


        ; Colour writes complete - do some housekeeping

            ; Clear the output bitmasks
            add rdx,4
            mov dword ptr [rdx], 0

            ; If the values are equal, the bit selector is 0 because the two colours are
            ; the same (which implies transparent)
            ; This seems the easiest way to do it, and will only rarely break branch prediction on
            ; typical images.
            test ebx, ebx
            jnz all_done

IF 0
            ; Sanity check
            movzx eax, word ptr [rdi-4]
            movzx ebx, word ptr [rdi-2]
            cmp eax, ebx
            jge fine
            int 3
            fine:
ENDIF



        ; Final clustering, creating the 2-bit values that define the output

            movaps xmm7, xmm3           ; right
            mulps xmm3, [split_point]
            addps xmm7, xmm2            ; left
            mulps xmm7, [half]            ; centre (probably 0)

            lea rsi, [rbp-TMP_POS_ON_AXIS]
            lea rdi, [expandtable]

            mov r8d,[rbp-TMP_SWAP]
            and r8d, 0fh

            ; Main cluster loop
            movaps xmm0,[clearsign]        ; Promote this outside the loop to improve code density
            mov ecx, 4
            next_bit_loop:    ; Do 4 at once
                movaps xmm4, [rsi]        ; Read the four pos_on_axis entries
                add esi, 16
                movaps xmm5, xmm4
                andps xmm4, xmm0        ; Clear the sign bits in this copy
                cmpltps xmm4, xmm3        ; < division means 2
                cmpnltps xmm5, xmm7        ; >= centre means add 1

                movmskps eax, xmm4
                movmskps ebx, xmm5
                xor ebx,r8d             ; Swap the order if we exchanged the colours
                mov al, byte ptr [rdi+rax+16]
                or al, byte ptr [rdi+rbx]
                mov byte ptr [rdx], al    ; rdx == block_dxtc+1

                add rdx, 1

                sub ecx, 1
                jne next_bit_loop

        ; Complete

        all_done:

            ; Restore changed regs and exit

            movaps xmm6, [rbp-TMP_REGSAVE-( 0*16)]
            movaps xmm7, [rbp-TMP_REGSAVE-( 1*16)]
            movaps xmm8, [rbp-TMP_REGSAVE-( 2*16)]

            pop rbp
            pop rbx
            pop rdi
            pop rsi
            ret
            

DXTCV11CompressBlockSSE     ENDP

END