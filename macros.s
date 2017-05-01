#  Copyright (C) 2004 Jason Woofenden
#
#  This file is part of herkforth.
#
#  herkforth is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  herkforth is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with herkforth; see the file COPYING.  If not, write to the
#  Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
#  MA 02111-1307, USA.

	.set stack, 13
	.set rstack, 14
	.set tos, 15
	.set a, 16
	.set here, 17
	.set src_cur, 18
	.set dict_end, 19
	.set dict, 20
	.set color_jump_reg, 21
	.set flushed_to, 22

	.set data, 23
	.set data_end, 24
	.set src, 25
	.set src_end, 26
	.set cur_block, 27
	.set block_position, 28
	.set bb, 29 # b register in forth


	# newish versions of gas have an option -mregnames which does this for
	# you, but I have it here for campatibility with older versions:
	.set r0, 0
	.set r1, 1
	.set r2, 2
	.set r3, 3
	.set r4, 4
	.set r5, 5
	.set r6, 6
	.set r7, 7
	.set r8, 8
	.set r9, 9
	.set r10, 10
	.set r11, 11
	.set r12, 12
	.set r13, 13
	.set r14, 14
	.set r15, 15
	.set r16, 16
	.set r17, 17
	.set r18, 18
	.set r19, 19
	.set r20, 20
	.set r21, 21
	.set r22, 22
	.set r23, 23
	.set r24, 24
	.set r25, 25
	.set r26, 26
	.set r27, 27
	.set r28, 28
	.set r29, 29
	.set r30, 30
	.set r31, 31
	

	.macro compile reg
		stw \reg, 0(here)
		addi here, here, 4
	.endm

	.macro rpop, reg
		lwz \reg, 0(rstack)
		addi rstack, rstack, 4
	.endm

	.macro rpush, reg
		stwu \reg, -4(rstack)
	.endm

	.macro push
		stwu tos, -4(stack)
	.endm

	.macro pop
		lwz tos, 0(stack)
		addi stack, stack, 4
	.endm

	.macro laddr reg addr
		lis \reg, \addr@ha
		ori \reg, \reg, \addr@l
	.endm

	.macro print_err str len
		li r0, 4
		li r3, 2 # stderr
		lis r4, \str@ha
		ori r4, r4, \str@l
		li r5, \len
		sc
	.endm
