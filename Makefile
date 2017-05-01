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


.PHONY: snip clean all

CFLAGS+=-Wall

ALL=herkforth000

all: $(ALL)

kernel.s.out: kernel.s macros.s
	as -o kernel.s.o kernel.s
	ld --oformat binary -o kernel.s.out kernel.s.o
	@rm kernel.s.o

herkforth000: kernel.s.out build blocks/??? blocks/dictionary blocks/data
	./build blocks/???

tags: gentags.pl blocks/???
	perl gentags.pl blocks/??? | sort -u > tags

snip:
	@as -o snip.o snip.s
	@ld --oformat=binary -o snip snip.o
	@xxd snip | colrm 1 9 | colrm 40
	@rm -f snip snip.o

clean:
	rm -f *.o kernel.s.* build herkforth??? core snip tags
