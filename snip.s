	.text

	.global _start

	.include "macros.s"
	
_start:
	rlwimi a, tos, 0, 0, 0
