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

	#elf file header
	.byte 0x7f, 'E', 'L', 'F'
	.byte 1, 2, 1, 0	# 32-bit, big-endian, version, padding
	.byte 0, 0, 0, 0	# padding...
	.byte 0, 0, 0, 0
	.word 2			# file type (EXEC)
	.word 20		# architecture (PPC)
	.int 1			# ELF version (1)
	.int _start		# entry point
	.int 0x34		# program header table offset
	.int 0			# section header table offset
	.int 0			# processor flags
	.word 0x34		# ELF header size
	.word 0x20		# program header table entry size
	.word 1			# number of program header table entries
	.word 0x28		# section header table entry size
	.word 0			# number of section header table entries
	.word 0			# section header string table index

#offset 0x34 into file
	#program header table entry
	.int 1		# type (PT_LOAD)
	.int 0x54	# file offset
	.int 0x10000054	# virtual address
	.int 0		# physical address (unused)
	.int end - begining	# size in file
	.int 1024*1024	# size in memory (1MB) IF YOU CHANGE THIS CHANGE RSTACK ADDRESS
	.int 7		# flags (read/write/execute)
	.int 0x10000	# alignment (64K boundary)

#code starts here (address 0x10000054, offset 0x54 into file)
begining:

	.text

	.global _start

	.include "macros.s"
	
_start:
	#build.pl will set most registers using this space:
	.int 0,0,0,0,0,0,0,0,0,0,0,0,0,0

	#setup some registers
	lis rstack, 0x1010 #put stacks at end of memory block (1MB in)
	addi stack, rstack, -1024
	mr flushed_to, here
	mr src_cur, src
	laddr color_jump_reg, color_table

	.macro dict_entry offset word
		laddr r3, \word
		stw r3, \offset(dict)
	.endm

	# put addresses for core words in dictionary
	dict_entry 0x020, word_load
	dict_entry 0x040, word_w_comma
	dict_entry 0x260, word_flush
	dict_entry 0x280, word_semicolon
	dict_entry 0x0c0, word__rlm_a
	dict_entry 0x0e0, word_a_comma
	dict_entry 0x0a0, word__drop
	dict_entry 0x080, word__dup
	dict_entry 0x100, word__lit
	dict_entry 0x100, word__lit
	dict_entry 0x240, color_table

	# first time: put 0 as block number
	li tos, 0
word_load:
	# rpush link register
	mflr r3
	rpush r3
	# push src-cur
	rpush src_cur
	# push count
	rpush block_position
	# pop block number off stack
	# compute block address
	mulli src_cur, tos, 1024
	pop
	add src_cur, src_cur, src
	lhz block_position, 1022(src_cur)
	addi src_cur, src_cur, -2

	# compile tokens
interpret:
	addi block_position, block_position, -1

	lhzu r3, 2(src_cur)
	rlwinm r0, r3, 2, 26, 29
	rlwinm r3, r3, 1, 15, 26
	# we pass the dictionary offset to color_* in r3

	add r0, color_jump_reg, r0
	mtctr r0
	bctrl # jump into the color_table table
	
	# loop
	cmpwi block_position, 0
	bne interpret

	# pop count
	rpop block_position
	# pop src_cur
	rpop src_cur

	# return
	rpop r3
	mtlr r3
	blr


color_table:
	b color_execute       # 0
	b color_compile       # 1
	b color_const         # 2
	b color_const_lit     # 3
	b color_tic           # 4
	blr                   # 5
	b color_const_def     # 6
	b color_define        # 7

color_execute:
	lwzx r4, r3, dict
	cmpwi r4, 0
	beq word_not_defined_error
	mtctr r4
	bctr

word__lit:
	mr r3, tos
	pop
lit:
	lis r4, 0x95ed #stwu tos, -4(stack)
	ori r4, r4, 0xfffc
	compile r4
	srwi r4, r3, 16 # assemble: lis
	addis r4, r4, 0x3de0
	compile r4
	clrlwi r4, r3, 16 # assemble: ori
	addis r4, r4, 0x61ef
	compile r4
	blr

color_define:
	stwx here, r3, dict # set pointer in dictionary
	
	#prolog:
	lis r3, 0x7c08 #mflr r0
	ori r3, r3, 0x02a6
	compile r3
	lis r3, 0x940e #stwu r0, -4(rstack)
	ori r3, r3, 0xfffc
	compile r3
	blr
	
color_compile:
	lwzx r4, r3, dict
	cmpwi r4, 0
	beq word_not_defined_error
	subf r4, here, r4
	clrlwi r4, r4, 6
	ori r4, r4, 1 # set link bit (so it's bl not b)
	oris r4, r4, 0x4800
	compile r4
	blr
	
	
color_const:
	push
	lwzx tos, r3, dict
	blr

color_const_lit:
	lwzx r3, r3, dict
	b lit

color_tic:
	push
	add tos, r3, dict
	blr
	
color_const_def:
	stwx tos, r3, dict
	pop
	blr
	
	
word_semicolon: #taken from fpos
	# compile this:
	#lwz r0, 0(rstack)
	#addi r0, r0, 4
	#mtlr r0
	#blr
	#or, change the last bl to:
	#lwz r0, 0(rstack)
	#addi r0, r0, 4
	#mtlr r0
	#b ...
	lwz r6, -4(here)
	srwi r4, r6, 26
	cmpwi r4, 18 #if the last instruction is a branch

	lis r3, 0x800e
	# low bits are 0
	lis r4, 0x39ce
	ori r4, r4, 0x0004
	lis r5, 0x7c08
	ori r5, r5, 0x03a6
	beq 0f
	lis r6, 0x4e80 #blr
	ori r6, r6, 0x0020
	b 1f
0:	addi r6, r6, -13 #we moved it 4 instructions
	#clrrwi r6, r6, 1
	addi here, here, -4
1:	stw r3, 0(here)
	stw r4, 4(here)
	stw r5, 8(here)
	stw r6, 12(here)
	addi here, here, 16
	# fall through to flush


# CACHE FLUSHING

# First why and when this is nessesary:
#
# PPC has seperate instruction and data caches. This makes things a little
# tricky when you assemble some instructions, store them in memory and then
# execute them because when you store them, they do not nessesarily make it
# into the instruction cache (or even main memory) before you go to execute
# them, unless you do the following:
#
# 1) flush the data cache block containing the instruction (dcbf)
# 2) sync (not sure why.)
# 3) invalidate the instruction cache containing that instruction (icbi)
# 4) wait for this to complete (sync isync)
# If you are flushing more than one cache block (which is only 32 bytes) you
# can do steps 1-3 above on each block, and do step 4 only once after.


# HOW word_flush WORKS

# The routine below (word_flush) flushes the memmory from flushed_to
# (inclusive) to here (exclusive) and takes no other arguments.
#
# It finds the address of the begining of the first cache-block we need to
# flush by rounding flushed_to down (and stores that in r3)
#
# The number of blocks to flush is calculated by the difference between r3 and
# here rounded up.


# word_semicolon falls through here
word_flush: #taken from fpos
	clrrwi r3, flushed_to, 5
	mr flushed_to, here
	subf r5, r3, here
	addi r5, r5, 0x1f
	srwi. r5, r5, 5
	beqlr # if 0 length to flush then return
	mtctr r5
0:	dcbf 0, r3
	sync
	icbi 0, r3
	addi r3, r3, 0x20
	bdnz 0b
	sync
	isync
	blr

word__rlm_a:
	#assemble an rlwimi instruction in a
	lis a, 0x51f0 #opcode
	rlwinm tos, tos, 1, 26, 30
	or a, a, tos
	pop
	rlwinm tos, tos, 6, 21, 25
	or a, a, tos
	pop
	rlwinm tos, tos, 11, 16, 20
	or a, a, tos
	pop
	stw a, 0(here)
	addi here, here, 4
	b word__drop

word_w_comma:
	stw tos, 0(here)
	addi here, here, 4
	pop
	blr

word_a_comma:
	stw a, 0(here)
	addi here, here, 4
	blr

word__drop:
	# compile:
	#   pop
	#
	# 81ed 0000 39ad 0004
	lis r0, 0x81ed
	#ori r0, r0 0x0000
	lis r3, 0x39ad
	ori r3, r3, 0x0004
	stw r0, 0(here)
	stw r3, 4(here)
	addi here, here, 8
	blr

word__dup:
	# compile
	#   push
	#
	# 95ed fffc
	lis r0, 0x95ed
	ori r0, r0, 0xfffc
	stw r0, 0(here)
	addi here, here, 4
	blr



word_interpret_not_defined:
	trap
word_save_not_defined:
	trap
word_not_defined_error:
	li r3, 1 #stdout
	lhz r8, 0(src_cur)
	rlwinm r4, r8, 1, 15, 26
	add r4, r4, dict
	addi r4, r4, 16
	li r5, 16
	li r0, 4
	sc
	trap
invalid_color:
	trap
	li r3, 219
	b exit
sys_error:
	mr r3, r0
exit:
	li r0, 1
	sc
end:
