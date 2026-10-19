//===============================================================================
// Copyright (c) 2007-2024  Advanced Micro Devices, Inc. All rights reserved.
// Copyright (c) 2004-2006 ATI Technologies Inc.
//===============================================================================
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//

#include "dxtc_v11_compress.h"

#if defined(_WIN32)

// Raises priority of G at expense of B - seems slightly better than no munging
#define AXIS_MUNGE


#define XMMCONST(name, val)\
    __declspec(align(16)) const float name[4] = { val, val, val, val };
#define XMMICONST(name, val)\
    __declspec(align(16)) const unsigned long name[4] = { val, val, val, val };
#define XMM4CONST(name, v0, v1, v2, v3)\
    __declspec(align(16)) const float name[4] = { v0, v1, v2, v3 };
#define XMM4ICONST(name, v0, v1, v2, v3)\
    __declspec(align(16)) const unsigned long name[4] = { v0, v1, v2, v3 };


XMMCONST(zero, 0);
XMMCONST(one, 1.0f);
XMMCONST(one_over_16, (1.0f/16.0f));
XMM4CONST(one_over_16_x_255_zeros, (1.0f/(16.0f*255.0f)), 0, 0, 0);
XMMICONST(clearsign, 0x7fffffff);
XMMICONST(signbit, 0x80000000);
XMMCONST(half, 0.5f);
XMMCONST(c255, 255.0f);
XMMCONST(c3, 3.0f);
XMMCONST(lots, 10000.0f);
XMMCONST(minuslots, -10000.0f);

XMMICONST(mask_green, 0xfc);
XMMICONST(mask_blue_red, 0xf8);

XMMICONST(clamp_0, 0);
XMMICONST(clamp_255, 255);

XMMICONST(green_rounding, (0x80>>6));
XMMICONST(blue_red_rounding, (0x80>>5));

XMM4ICONST(rb_sign_bits, 0x80000000, 0, 0x80000000, 0);

XMMCONST(stepsize, 0.95f);
XMMCONST(onethird, 1.0f/3.0f);
XMMCONST(maxerror_init, 10000000.0f);
XMMCONST(maxerror_epsilon, 5.0f);

XMM4CONST(b_half, 0.5f, 1.0f, 1.0f, 1.0f);
XMM4CONST(b_2x, 2.0f, 1.0f, 1.0f, 1.0f);

XMMICONST(invert, 0xffffffff);

XMMCONST(split_point, 2.0f/3.0f);
XMMCONST(deviation_point, 1.0f/3.0f);
XMMCONST(split_point_r300, 5.0f/8.0f);
XMMCONST(deviation_point_r300, 1.0f/4.0f);

XMMICONST(_0f0f0f0f0f0f0f0f, 0x0f0f0f0f);
XMMICONST(_0707070707070707, 0x07070707);
XMMICONST(_000f000f000f000f, 0x000f000f);
XMMICONST(_00f000f000f000f0, 0x00f000f0);

#define SHUFFLE_SELECT(a, b, c, d) (a | (b<<2) | (c<<4) | (d<<6))


#define PARALLEL_ADD_XMM7        \
{                                \
    __asm movaps xmm6, xmm7            \
    __asm movaps xmm5, xmm7            \
    __asm shufps xmm7, xmm7, SHUFFLE_SELECT(2, 3, 2, 3)    \
    __asm addps xmm7, xmm6                                \
    __asm shufps xmm5, xmm5, SHUFFLE_SELECT(1, 1, 1, 1)    \
    __asm addps xmm7, xmm5            \
}

#define AVERAGE        xmm0
#define AXIS        xmm1

BYTE expandtable[32] = 
{
    0x00, 0x01, 0x04, 0x05,
    0x10, 0x11, 0x14, 0x15,
    0x40, 0x41, 0x44, 0x45,
    0x50, 0x51, 0x54, 0x55,
    0x00, 0x02, 0x08, 0x0a,
    0x20, 0x22, 0x28, 0x2a,
    0x80, 0x82, 0x88, 0x8a,
    0xa0, 0xa2, 0xa8, 0xaa,
};

#endif //_WIN32

#if !defined(_WIN64) && defined(_WIN32)

#pragma warning( push )
#pragma warning(disable:4100)

void __declspec(naked) __cdecl DXTCV11CompressBlockSSE(DWORD *block_32, DWORD *block_dxtc)
{
    // *block_32 *block_dxtc (VS2010) compiler generates warning C4100: unreferenced formal parameter
    __asm
    {
        {
            push esi
            push edi
            push ebx
            push ebp
#define SP_BLOCK_32        20
#define SP_BLOCK_DXTC    24

            // Set up a 16-byte aligned storage space pointer
            mov ebp, esp
            and ebp, ~0x0f
#define EBP_UNIQUES            (16*16)
#define EBP_POS_ON_AXIS        (EBP_UNIQUES+(16*4))
#define EBP_DIST_FROM_AXIS    (EBP_POS_ON_AXIS+(16*4))

#define EBP_VALUES            (EBP_DIST_FROM_AXIS+16)
#define EBP_OLDLEFT            (EBP_VALUES+16)
#define EBP_OLDRIGHT        (EBP_OLDLEFT+16)
#define EBP_RIGHTSAVE        (EBP_OLDRIGHT+16)

#define EBP_MAXERROR        (EBP_RIGHTSAVE+4)
#define EBP_ERROR            (EBP_MAXERROR+4)

#define EBP_SWAP            (EBP_ERROR+4)


            // Find the array of unique pixel values and sum them to find their average position

            xorps AVERAGE, AVERAGE        // average (r, g, b)

            lea edi, [ebp-EBP_UNIQUES]
            mov esi, [esp+SP_BLOCK_32]
            mov ecx, 16
            pxor mm2, mm2
            {
            average_unique_loop:
                movd mm0, [esi]
                add esi, 4
                punpcklbw mm0, mm2
                movq mm1, mm0
                punpckhwd mm0, mm2
                punpcklwd mm1, mm2
                cvtpi2ps xmm7, mm0
                movlhps xmm7, xmm7
                cvtpi2ps xmm7, mm1


                // colourspace conversion
                // Most of the options in the C compressor are not supported here
#ifdef AXIS_MUNGE
                movaps xmm6, xmm7
                shufps xmm6, xmm6, SHUFFLE_SELECT(1, 1, 1, 1)    // G
                addss xmm7, xmm6
                mulps xmm7, [b_half]
#endif

                movaps [edi], xmm7
                add edi, 16
                addps AVERAGE, xmm7

                sub ecx, 1
                jne average_unique_loop
            }

            // Compute average of the uniques
            mulps AVERAGE, [one_over_16]
        }


        {
            // For each component, reflect points about the average so all lie on the same side
            // of the average, and compute the new average - this gives a second point that defines the axis
            // To compute the sign of the axis sum the positive differences of G for each of R and B (the
            // G axis is always positive in this implementation

            xorps AXIS, AXIS        // v (r, g, b)
            xorps xmm2, xmm2        // rg_pos, rb_pos, bg_pos
            lea edi, [ebp-EBP_UNIQUES]
            mov ecx, 16
            {
            find_axis_loop:
                movaps xmm7, [edi]        // R G B value
                add edi, 16
                subps xmm7, AVERAGE        // centred
                movaps xmm6, xmm7
                movaps xmm5, xmm7

                andps xmm7, [clearsign]    // fabs (r, g, b)
                addps AXIS, xmm7        // direction of axis

                shufps xmm6, xmm6, SHUFFLE_SELECT(0, 2, 2, 3)    // B R R 0
                shufps xmm5, xmm5, SHUFFLE_SELECT(1, 0, 1, 3)    // G B G 0

                cmpnltps xmm6, [zero]    // R/B > 0?
                andps xmm6, xmm5        // insert the G or B value for those channels which are positive
                addps xmm2, xmm6        // bg_pos rb_pos rg_pos

                sub ecx, 1
                jne find_axis_loop
            }

            mulps AXIS, [one_over_16]


            // Handle RB_pos - RB pos is used if RG_pos and BG_pos are both zero.
            movaps xmm5, xmm2    // duplicate the pos across these three
            movaps xmm6, xmm2
            movaps xmm7, xmm2
            shufps xmm5, xmm5, SHUFFLE_SELECT(1, 3, 3, 3)    // RB_pos 0 ->
            shufps xmm6, xmm6, SHUFFLE_SELECT(2, 2, 2, 2)    // RG_pos ->
            shufps xmm7, xmm7, SHUFFLE_SELECT(0, 0, 0, 0)    // BG_pos ->
            orps xmm6, xmm7
            cmpneqps xmm6, [zero]    // so check for any non-zero in RG_pos or BG_pos
            andps xmm2, xmm6        // Mask out RG_pos in current if we need to the current
            xorps xmm6, [invert]
            andps xmm5, xmm6
            orps xmm2, xmm5            // insert RB pos instead


            // Change the sign of the R and B portions of the axis appropriately
            cmpltps xmm2, [zero]
            andps xmm2, [rb_sign_bits]
            xorps AXIS, xmm2            // Flip the sign of the axis if the r/g or b/g tests indicate a negative slope


        }

        // Axis projection and remapping

        {
            // Normalise the axis for simplicity of future calculation
            movaps xmm7, AXIS
            mulps xmm7, xmm7

            PARALLEL_ADD_XMM7

            // low of xmm7 is the DP result
            // If this is 0 we haven't actually got an axis, and we can't rsq it,
            // so mask the output to 0 in this case. This generates an acceptable result
            movaps xmm2, xmm7
            cmpneqps xmm2, [zero]

#if 1        // RSQRT with Newton-Raphson. This can be omitted for even faster encoding performance, but quality
            // and consistency improves with it in on certain images. It's not a large cost so leave it in.
            rsqrtps xmm3, xmm7
            andps xmm3, xmm2

            movaps    xmm2, xmm3
            mulps    xmm3, xmm7
            mulps    xmm3, xmm2
            mulps    xmm2, [half]
            movaps    xmm7, [c3]
            subps    xmm7, xmm3
            mulps    xmm7, xmm2
#else
            // No Newton-Raphson method
            rsqrtps xmm7, xmm7
            andps xmm7, xmm2
#endif

            shufps xmm7, xmm7, SHUFFLE_SELECT(0, 0, 0, 0)

            // Normalise
            mulps AXIS, xmm7
        }

#define LEFT xmm2
#define RIGHT xmm3
        // Map the axis
        {
            lea edi, [ebp-EBP_UNIQUES]
            lea edx, [ebp-EBP_POS_ON_AXIS]
            mov ecx, 16
            movaps LEFT, [lots]
            movaps RIGHT, [minuslots]
            xorps xmm4, xmm4            // axis mapping error
            {
            map_axis_loop:
                movaps xmm7, [edi]
                subps xmm7, AVERAGE
                mulps xmm7, AXIS

                PARALLEL_ADD_XMM7
                movss [edx], xmm7
                add edx, 4

                // xmm7 == pos_on_axis

                minss LEFT, xmm7        // calculate left
                maxss RIGHT, xmm7        // calculate right

                add edi, 16

                sub ecx, 1
                jne map_axis_loop
            }
        }


        shufps LEFT, LEFT, SHUFFLE_SELECT(0, 0, 0, 0)
        shufps RIGHT, RIGHT, SHUFFLE_SELECT(0, 0, 0, 0)


        {
            // Now we have a good axis and the basic information about how the points are mapped
            // to it

            // Our initial guess is to represent the endpoints accurately, by moving the average
            // to the centre and recalculating the point positions along the line

            // Calculate centre
            movaps xmm7, LEFT
            addps xmm7, RIGHT
            mulps xmm7, [half]

            // Offset all the axis positions to the centre
            lea edi, [ebp-EBP_POS_ON_AXIS]
            movaps xmm5, [edi]
            movaps xmm6, [edi+16]
            subps xmm5, xmm7
            subps xmm6, xmm7
            movaps [edi], xmm5
            movaps [edi+16], xmm6
            movaps xmm5, [edi+32]
            movaps xmm6, [edi+48]
            subps xmm5, xmm7
            subps xmm6, xmm7
            movaps [edi+32], xmm5
            movaps [edi+48], xmm6

            // Offset left, right and average to centre
            subps LEFT, xmm7
            subps RIGHT, xmm7

            mulps xmm7, AXIS
            addps AVERAGE, xmm7
        }


#define PROGRESSIVE_REFINEMENT
#ifdef PROGRESSIVE_REFINEMENT
        {
            // Attempt a (simple) progressive refinement step to reduce noise in the
            // output image by trying to find a better overall match for the endpoints
            // than the first-guess solution found so far (which is just to take the ends.

            // The method is to move the endpoints inwards until a local minima is found.
            // This provides quite a significant improvement in image quality.

            mov eax, [maxerror_init]
            mov [ebp-EBP_MAXERROR], eax

            movaps [ebp-EBP_OLDLEFT], LEFT
            movaps [ebp-EBP_OLDRIGHT], RIGHT

            lea edx, [expandtable]
            lea edi, [ebp-EBP_VALUES]

            {
            next_refinement_loop:

                movaps [ebp-EBP_RIGHTSAVE], RIGHT

                mov eax, 0
                mov [ebp-EBP_ERROR], eax

                lea ecx, split_point

                movss xmm7, LEFT
                addss xmm7, RIGHT
                mulss xmm7, [half]
                movss [edi], LEFT
                movss [edi+4], RIGHT
                movss xmm5, RIGHT
                movss xmm6, xmm7
                subss xmm5, xmm7        // right-centre
                movss xmm4, xmm7
                movss RIGHT, xmm5
                mulss xmm5, [ecx+16]    //[deviation_point]
                subss xmm6, xmm5
                addss xmm4, xmm5
                movss [edi+8], xmm6
                movss [edi+12], xmm4


                mulps RIGHT, [ecx]        //[split_point]

                mov ecx, 16
                lea esi, [ebp-EBP_POS_ON_AXIS]
                {
                next_builderror_loop:
                    movss xmm4, [esi]
                    add esi, 4
                    movaps xmm5, xmm4
                    movaps xmm6, xmm4
                    andps xmm4, [clearsign]
                    cmpltss xmm4, RIGHT        // < division means 2
                    cmpnltss xmm5, xmm7        // >= centre means add 1

                    movmskps eax, xmm4
                    movmskps ebx, xmm5

                    movzx eax, byte ptr[edx+eax+16]
                    movzx ebx, byte ptr[edx+ebx]
                    or eax, ebx
                    and eax, 3

                    subss xmm6, [edi+4*eax]
                    mulss xmm6, xmm6
                    addss xmm6, [ebp-EBP_ERROR]
                    movss [ebp-EBP_ERROR], xmm6

                    sub ecx, 1
                    jne next_builderror_loop
                }

                movaps RIGHT, [ebp-EBP_RIGHTSAVE]

#if 1
                mov eax, [ebp-EBP_ERROR]
                mov ebx, [ebp-EBP_MAXERROR]
                cmp eax, ebx
                jge refinement_done
#else
                movss xmm5, [ebp-EBP_MAXERROR]
                cmpltss xmm5, xmm6
                movss [ebp-EBP_ERROR], xmm5
                mov eax, [ebp-EBP_ERROR]
                test eax, eax
                jnz refinement_done
#endif

                subss xmm6, [maxerror_epsilon]
                movss [ebp-EBP_MAXERROR], xmm6

                movaps [ebp-EBP_OLDLEFT], LEFT
                movaps [ebp-EBP_OLDRIGHT], RIGHT

                mulps LEFT, [stepsize]
                mulps RIGHT, [stepsize]

                jmp next_refinement_loop

            refinement_done:
                movaps LEFT, [ebp-EBP_OLDLEFT]
                movaps RIGHT, [ebp-EBP_OLDRIGHT]
            }
        }
#endif


        {
            // Calculate the high and low output colour values

            // Involved in this is a rounding procedure which is undoubtedly slightly twitchy. A
            // straight rounded average is not correct, as the decompressor 'unrounds' by replicating
            // the top bits to the bottom.

            // In order to take account of this process, we don't just apply a straight rounding correction,
            // but base our rounding on the input value (a straight rounding is actually pretty good in terms of
            // error measure, but creates a visual colour and/or brightness shift relative to the original image)
            // The method used here is to apply a centre-biased rounding dependent on the input value, which was
            // (mostly by experiment) found to give minimum MSE while preserving the visual characteristics of
            // the image.
            // rgb = (average_rgb + (left|right)*v_rgb);
            movaps xmm6, LEFT
            movaps xmm7, RIGHT
            mulps xmm6, AXIS
            mulps xmm7, AXIS
            addps xmm6, AVERAGE
            addps xmm7, AVERAGE

#ifdef AXIS_MUNGE
            // Scale the B component, then subtract the green component resultant in each
            movaps xmm4, xmm6
            movaps xmm5, xmm7
            mulps xmm6, [b_2x]
            mulps xmm7, [b_2x]
            shufps xmm4, xmm4, SHUFFLE_SELECT(1, 1, 1, 1)
            shufps xmm5, xmm5, SHUFFLE_SELECT(1, 1, 1, 1)
            subss xmm6, xmm4
            subss xmm7, xmm5
#endif

            // Rearrange so B and R are in the same register half (they both use 5-bit rounding)
            shufps xmm6, xmm6, SHUFFLE_SELECT(0, 2, 1, 3)    // B R G
            shufps xmm7, xmm7, SHUFFLE_SELECT(0, 2, 1, 3)

            // Convert to integer (by truncation, as C code does)
            cvttps2pi mm0, xmm6
            cvttps2pi mm1, xmm7
            movhlps xmm6, xmm6
            movhlps xmm7, xmm7
            cvttps2pi mm2, xmm6
            cvttps2pi mm3, xmm7

            // mm0/1 is blue/red, mm2/3 is green

            // This isn't quite the same as the C algorithm, but should generate the same result
            // if the input range is ensured to be 0-255

            // This code could be heavily interleaved, but for P4 it's not worth the hassle - the
            // P4 reordering range of 15 instructions will let it do the job for us

            pmaxsw mm0, [clamp_0]        // Note: faster to do these max/min in MMX than float XMM - better reordering opportunities
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
            // No need to clamp here, with the input in 0-255 range it can never be outside at the end
            pand mm0, [mask_blue_red]
            pand mm1, [mask_blue_red]

            // Separate out R and B as they will need separate shifts later
            pshufw mm4, mm0, SHUFFLE_SELECT(2, 3, 2, 3)    // extract R    (this is an SSE, not MMX, instruction)
            pshufw mm5, mm1, SHUFFLE_SELECT(2, 3, 2, 3)    // also R

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


            // Convert the 8-bit values to final RGB565 colours in mm0 and mm1
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

            // mm0 and mm1 are c0 and c1


            // Need to compare c0/c1 for sign and flip and set swap if required - and handle colour equality as well....
            mov edi, [esp+SP_BLOCK_DXTC]
#if 1
            pxor mm5, mm5
            punpcklwd mm0, mm5    // unpack c0/c1 to DWORD's as pcmp is a signed comparison
            punpcklwd mm1, mm5
            movq mm2, mm0
            movq mm3, mm0
            movq mm4, mm0
            pcmpgtd mm2, mm1
            pxor mm2, [invert]    // Need less than, so flip the result
            movd [ebp-EBP_SWAP], mm2    // Set the swap flag (used below) appropriately)
            // mm2 is the mask to indicate flipping is needed

            pcmpeqd mm4, mm1
            movd ebx, mm4        // ebx is the equality flag, plenty of time for this slow move to resolve

            punpcklwd mm0, mm1    // 'normal' order
            punpcklwd mm1, mm3    // reversed order
            pand mm1, mm2
            pandn mm2, mm0
            por mm1, mm2        // one of the two, selected by mm2
            movd [edi], mm1        // write the result
#else
            // No compare simple version
            punpcklwd mm0, mm1
            mov [ebp-EBP_SWAP], 0
            xor ebx, ebx
            movd [edi], mm0

//            punpcklwd mm1, mm0
//            movq mm0, mm1
//            mov dword ptr[ebp-EBP_SWAP], 0xffffffff
#endif
        }

        // Clear the output bitmasks
        add edi, 4
        mov dword ptr [edi], 0

        // If the values are equal, the bit selector is 0 because the two colours are
        // the same (which implies transparent)
        // This seems the easiest way to do it, and will only rarely break branch prediction on
        // typical images.
        test ebx, ebx
        jnz all_done


        {
            // Final clustering, creating the 2-bit values that define the output

            lea ecx, split_point

            movaps xmm7, RIGHT
            mulps RIGHT, [ecx]            // split point
            addps xmm7, LEFT
            mulps xmm7, [half]            // centre (probably 0)

            lea esi, [ebp-EBP_POS_ON_AXIS]
            lea edx, [expandtable]
            movss xmm6, [ebp-EBP_SWAP]
            shufps xmm6, xmm6, 0
            mov ecx, 4
            {
            next_bit_loop:    // Do 4 at once
                movaps xmm4, [esi]        // Read the four pos_on_axis entries
                add esi, 16
                movaps xmm5, xmm4
                andps xmm4, [clearsign]
                cmpltps xmm4, RIGHT        // < division means 2
                cmpnltps xmm5, xmm7        // >= centre means add 1

                xorps xmm5, xmm6        // Swap the order if we had to flip our colours

                movmskps eax, xmm4
                movmskps ebx, xmm5

                movzx eax, byte ptr[edx+eax+16]
                movzx ebx, byte ptr[edx+ebx]
                or eax, ebx
                mov byte ptr [edi], al
                add edi, 1

                sub ecx, 1
                jne next_bit_loop
            }

        }



        all_done:
        emms

        pop ebp
        pop ebx
        pop edi
        pop esi
        ret
    }


}

void __declspec(naked) __cdecl DXTCV11CompressBlockSSEMinimal(DWORD *block_32, DWORD *block_dxtc)
{
#define SP_BLOCK_32         20
#define SP_BLOCK_DXTC       24
#define EBP_UNIQUES         (16*16)
#define EBP_POS_ON_AXIS     (EBP_UNIQUES+(16*4))
#define EBP_DIST_FROM_AXIS  (EBP_POS_ON_AXIS+(16*4))
#define EBP_VALUES          (EBP_DIST_FROM_AXIS+16)
#define EBP_OLDLEFT         (EBP_VALUES+16)
#define EBP_OLDRIGHT        (EBP_OLDLEFT+16)
#define EBP_RIGHTSAVE       (EBP_OLDRIGHT+16)
#define EBP_MAXERROR        (EBP_RIGHTSAVE+4)
#define EBP_ERROR           (EBP_MAXERROR+4)
#define EBP_SWAP            (EBP_ERROR+4)

// *block_32 *block_dxtc (VS2010) compiler generates warning C4100: unreferenced formal parameter
__asm
{
    push esi
    push edi
    push ebx
    push ebp

    // Set up a 16-byte aligned storage space pointer
    mov ebp, esp
    and ebp, ~0x0f

    // init to 0
    xorps AVERAGE, AVERAGE        // average (r, g, b)

    // -------------------------------------------------------------------------------------
    // (3) Find the array of unique pixel values and sum them to find their average position
    // -------------------------------------------------------------------------------------
    {
    lea        edi,[ebp - EBP_UNIQUES]
    mov        esi,[esp + SP_BLOCK_32]
    mov        ecx, 16
    pxor    mm2, mm2

average_unique_loop :
    {
    movd        mm0 ,[esi]
    add         esi , 4
    punpcklbw   mm0 , mm2
    movq        mm1 , mm0
    punpckhwd   mm0 , mm2
    punpcklwd   mm1 , mm2
    cvtpi2ps    xmm7, mm0
    movlhps     xmm7, xmm7
    cvtpi2ps    xmm7, mm1

        // colourspace conversion
    movaps[edi], xmm7
    add            edi, 16
    addps        AVERAGE, xmm7

    sub ecx, 1
    jne average_unique_loop
    }

    // Compute average of the uniques
    mulps AVERAGE,[one_over_16]
    }

    // -------------------------------------------------------------------------------------
    // (4) For each component, reflect points about the average so all lie on the same side
    // of the average, and compute the new average - this gives a second point that defines the axis
    // To compute the sign of the axis sum the positive differences of G for each of R and B (the
    // G axis is always positive in this implementation
    // -------------------------------------------------------------------------------------
    { 

            xorps    AXIS, AXIS                // v (r, g, b)
                xorps    xmm2, xmm2                // rg_pos, rb_pos, bg_pos
                lea        edi, [ebp - EBP_UNIQUES]
                mov        ecx, 16

                find_axis_loop:
            {
                movaps    xmm7, [edi]            // R G B value
                    add        edi, 16
                    subps    xmm7, AVERAGE            // centred
                    movaps    xmm6, xmm7
                    movaps    xmm5, xmm7

                    andps    xmm7, [clearsign]        // fabs (r, g, b)
                    addps    AXIS, xmm7            // direction of axis

                    shufps    xmm6, xmm6, SHUFFLE_SELECT(0, 2, 2, 3)    // B R R 0
                    shufps    xmm5, xmm5, SHUFFLE_SELECT(1, 0, 1, 3)    // G B G 0

                    cmpnltps xmm6, [zero]        // R/B > 0?
                    andps    xmm6, xmm5            // insert the G or B value for those channels which are positive
                    addps    xmm2, xmm6            // bg_pos rb_pos rg_pos

                    sub        ecx, 1
                    jne        find_axis_loop
            }


            mulps AXIS, [one_over_16]

                // Handle RB_pos - RB pos is used if RG_pos and BG_pos are both zero.
                movaps    xmm5, xmm2                                 // duplicate the pos across these three
                movaps    xmm6, xmm2
                movaps    xmm7, xmm2
                shufps    xmm5, xmm5, SHUFFLE_SELECT(1, 3, 3, 3)    // RB_pos 0 ->
                shufps    xmm6, xmm6, SHUFFLE_SELECT(2, 2, 2, 2)    // RG_pos ->
                shufps    xmm7, xmm7, SHUFFLE_SELECT(0, 0, 0, 0)    // BG_pos ->
                orps    xmm6, xmm7
                cmpneqps xmm6, [zero]                            // so check for any non-zero in RG_pos or BG_pos
                andps    xmm2, xmm6                                // Mask out RG_pos in current if we need to the current
                xorps    xmm6, [invert]
                andps    xmm5, xmm6
                orps    xmm2, xmm5                                    // insert RB pos instead

                // Change the sign of the R and B portions of the axis appropriately
                cmpltps xmm2, [zero]
                andps    xmm2, [rb_sign_bits]
                xorps    AXIS, xmm2                                // Flip the sign of the axis if the r/g or b/g tests indicate a negative slope
    }

    // -------------------------------------------------------------------------------------
    // (5) Axis projection and remapping
    // -------------------------------------------------------------------------------------
    {
        // Normalise the axis for simplicity of future calculation
        movaps        xmm7, AXIS
        mulps        xmm7, xmm7

        PARALLEL_ADD_XMM7

        // low of xmm7 is the DP result
        // If this is 0 we haven't actually got an axis, and we can't rsq it,
        // so mask the output to 0 in this case. This generates an acceptable result
        movaps        xmm2, xmm7
        cmpneqps    xmm2, [zero]
        rsqrtps        xmm7, xmm7
        andps        xmm7, xmm2
        shufps        xmm7, xmm7, SHUFFLE_SELECT(0, 0, 0, 0)

        // Normalise
        mulps AXIS, xmm7
    }

    #define LEFT  xmm2
    #define RIGHT xmm3

    // -------------------------------------------------------------------------------------
    // (6) Map the axis
    // -------------------------------------------------------------------------------------
    {
        lea        edi, [ebp - EBP_UNIQUES]
        lea        edx, [ebp - EBP_POS_ON_AXIS]
        mov        ecx, 16
        movaps    LEFT, [lots]
        movaps    RIGHT, [minuslots]
        xorps    xmm4, xmm4                // axis mapping error
        {
map_axis_loop:
                movaps    xmm7,[edi]
                subps    xmm7, AVERAGE
                mulps    xmm7, AXIS
                PARALLEL_ADD_XMM7
                movss[edx], xmm7
                add        edx, 4
                // xmm7 == pos_on_axis
                minss    LEFT,  xmm7        // calculate left
                maxss    RIGHT, xmm7        // calculate right
                add        edi, 16
                sub        ecx, 1
                jne         map_axis_loop
        }
    }

    shufps    LEFT, LEFT, SHUFFLE_SELECT(0, 0, 0, 0)
    shufps    RIGHT, RIGHT, SHUFFLE_SELECT(0, 0, 0, 0)

    // -------------------------------------------------------------------------------------
    // (7) Now we have a good axis and the basic information about how the points are mapped
    // to it
    // Our initial guess is to represent the endpoints accurately, by moving the average
    // to the centre and recalculating the point positions along the line
    // -------------------------------------------------------------------------------------
    { 
            // Calculate centre
            movaps    xmm7, LEFT
                addps    xmm7, RIGHT
                mulps    xmm7, [half]

                // Offset all the axis positions to the centre
                lea        edi, [ebp - EBP_POS_ON_AXIS]
                movaps    xmm5, [edi]
                movaps    xmm6, [edi + 16]
                subps    xmm5, xmm7
                subps    xmm6, xmm7
                movaps[edi], xmm5
                movaps[edi + 16], xmm6
                movaps    xmm5, [edi + 32]
                movaps    xmm6, [edi + 48]
                subps    xmm5, xmm7
                subps    xmm6, xmm7
                movaps[edi + 32], xmm5
                movaps[edi + 48], xmm6

                // Offset left, right and average to centre
                subps LEFT, xmm7
                subps RIGHT, xmm7

                mulps xmm7, AXIS
                addps AVERAGE, xmm7
     }

    // -------------------------------------------------------------------------------------
    // (8) Calculate the high and low output colour values
    // Involved in this is a rounding procedure which is undoubtedly slightly twitchy. A
    // straight rounded average is not correct, as the decompressor 'unrounds' by replicating
    // the top bits to the bottom.
    // In order to take account of this process, we don't just apply a straight rounding correction,
    // but base our rounding on the input value (a straight rounding is actually pretty good in terms of
    // error measure, but creates a visual colour and/or brightness shift relative to the original image)
    // The method used here is to apply a centre-biased rounding dependent on the input value, which was
    // (mostly by experiment) found to give minimum MSE while preserving the visual characteristics of
    // the image.
    // rgb = (average_rgb + (left|right)*v_rgb);
    // -------------------------------------------------------------------------------------
    {
        movaps    xmm6, LEFT
        movaps    xmm7, RIGHT
        mulps    xmm6, AXIS
        mulps    xmm7, AXIS
        addps    xmm6, AVERAGE
        addps    xmm7, AVERAGE

        // Rearrange so B and R are in the same register half (they both use 5-bit rounding)
        shufps    xmm6, xmm6, SHUFFLE_SELECT(0, 2, 1, 3)    // B R G
        shufps    xmm7, xmm7, SHUFFLE_SELECT(0, 2, 1, 3)

        // Convert to integer (by truncation, as C code does)
        cvttps2pi    mm0, xmm6
        cvttps2pi    mm1, xmm7
        movhlps        xmm6, xmm6
        movhlps        xmm7, xmm7
        cvttps2pi    mm2, xmm6
        cvttps2pi    mm3, xmm7

        // mm0/1 is blue/red, mm2/3 is green
        // This isn't quite the same as the C algorithm, but should generate the same result
        // if the input range is ensured to be 0-255
        // This code could be heavily interleaved, but for P4 it's not worth the hassle - the
        // P4 reordering range of 15 instructions will let it do the job for us

        pmaxsw    mm0, [clamp_0]        // Note: faster to do these max/min in MMX than float XMM - better reordering opportunities
        pmaxsw    mm1, [clamp_0]
        pminsw    mm0, [clamp_255]
        pminsw    mm1, [clamp_255]
        movq    mm6, mm0
        movq    mm7, mm1
        paddd    mm0, [blue_red_rounding]
        paddd    mm1, [blue_red_rounding]
        psrld    mm6, 5
        psrld    mm7, 5
        psubd    mm0, mm6
        psubd    mm1, mm7
        // No need to clamp here, with the input in 0-255 range it can never be outside at the end
        pand    mm0, [mask_blue_red]
        pand    mm1, [mask_blue_red]

        // Separate out R and B as they will need separate shifts later
        pshufw    mm4, mm0, SHUFFLE_SELECT(2, 3, 2, 3)    // extract R    (this is an SSE, not MMX, instruction)
        pshufw    mm5, mm1, SHUFFLE_SELECT(2, 3, 2, 3)    // also R

        pmaxsw    mm2, [clamp_0]
        pmaxsw    mm3, [clamp_0]
        pminsw    mm2, [clamp_255]
        pminsw    mm3, [clamp_255]
        movq    mm6, mm2
        movq    mm7, mm3
        paddd    mm2, [green_rounding]
        paddd    mm3, [green_rounding]
        psrld    mm6, 6
        psrld    mm7, 6
        psubd    mm2, mm6
        psubd    mm3, mm7
        pand    mm2, [mask_green]
        pand    mm3, [mask_green]

        // Convert the 8-bit values to final RGB565 colours in mm0 and mm1
        psrld    mm0, 3
        psrld    mm1, 3
        pslld    mm4, 8
        pslld    mm5, 8
        pslld    mm2, 3
        pslld    mm3, 3
        por        mm0, mm4
        por        mm1, mm5
        por        mm0, mm2
        por        mm1, mm3

        // mm0 and mm1 are c0 and c1

        // Need to compare c0/c1 for sign and flip and set swap if required - and handle colour equality as well....
        mov        edi, [esp + SP_BLOCK_DXTC]
        pxor    mm5, mm5
        punpcklwd mm0, mm5                // unpack c0/c1 to DWORD's as pcmp is a signed comparison
        punpcklwd mm1, mm5
        movq    mm2, mm0
        movq    mm3, mm0
        movq    mm4, mm0
        pcmpgtd mm2, mm1
        pxor    mm2, [invert]                // Need less than, so flip the result
        movd[ebp - EBP_SWAP], mm2        // Set the swap flag (used below) appropriately)
                                        // mm2 is the mask to indicate flipping is needed
        pcmpeqd mm4, mm1
        movd    ebx, mm4                    // ebx is the equality flag, plenty of time for this slow move to resolve
        punpcklwd mm0, mm1                // 'normal' order
        punpcklwd mm1, mm3                // reversed order
        pand    mm1, mm2
        pandn    mm2, mm0
        por        mm1, mm2                    // one of the two, selected by mm2
        movd[edi], mm1                    // write the result
    }


    // Clear the output bitmasks
    add edi, 4
    mov dword ptr[edi], 0

    // If the values are equal, the bit selector is 0 because the two colours are
    // the same (which implies transparent)
    // This seems the easiest way to do it, and will only rarely break branch prediction on
    // typical images.
    test    ebx, ebx
    jnz        all_done

    // -------------------------------------------------------------------------------------
    // (9) Final clustering, creating the 2-bit values that define the output
    // -------------------------------------------------------------------------------------
    {
        lea        ecx, split_point

        movaps    xmm7, RIGHT
        mulps    RIGHT,[ecx]                    // split point
        addps    xmm7, LEFT
        mulps    xmm7,[half]                    // centre (probably 0)

        lea        esi,[ebp - EBP_POS_ON_AXIS]
        lea        edx,[expandtable]
        movss    xmm6,[ebp - EBP_SWAP]
        shufps    xmm6, xmm6, 0
        mov        ecx, 4
        {
next_bit_loop:                            // Do 4 at once
            movaps        xmm4,[esi]            // Read the four pos_on_axis entries
            add            esi, 16
            movaps        xmm5, xmm4
            andps        xmm4,[clearsign]
            cmpltps        xmm4, RIGHT            // < division means 2
            cmpnltps    xmm5, xmm7            // >= centre means add 1
            xorps        xmm5, xmm6            // Swap the order if we had to flip our colours
            movmskps    eax, xmm4
            movmskps    ebx, xmm5
            movzx        eax, byte ptr[edx + eax + 16]
            movzx        ebx, byte ptr[edx + ebx]
            or            eax, ebx
            mov            byte ptr[edi], al
            add            edi, 1
            sub            ecx, 1
            jne            next_bit_loop
        }
    }

all_done:
    emms
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret

    } // _asm
}

#pragma warning( pop )

#endif // !_WIN64
