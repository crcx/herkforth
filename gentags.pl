#!/usr/bin/perl

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




# This program finds all the words being defined in the sources, and creates a
# tags file for vim.
#
# To run, simply type "make tags". You will only have to regenerate the tags
# for new definitions, or if you move a definition to a different block
#
# In case you're not farmilliar with tags, this makes it so when you're editing
# the forth source you can:
#
# 1) put the cursor on any word and press ctrl-] and it will jump you to the
# definition.
#
# 2) after you've done this however many times, you can press ctrl-T to go
# back. (This works much like the "back" button in most web browsers.)


my $cur_arg = 0;
my $a, $b, $ch;

# Parse every other file passed on the command line.
# (Every other to skip shadow blocks when you just pass blocks/*
while ($cur_arg < ($#ARGV + 1)) {
	open FD, "<$ARGV[$cur_arg]";

	while($a = <FD>) {
		chop $a;

		# find the first word being defined on this line
		if($a =~ s/^[^':&]*[':&] ([^ ]+) .*$/\1/) {
			# $a is the word being defined
			print "$a	$ARGV[$cur_arg]	/\\<$a\\>\n";

			# print a definition for macros
			if($a =~ s/^,//) {
				print "$a	$ARGV[$cur_arg]	/\\<,$a\\>\n";
			}
		}
	}

	close FD;
	
	$cur_arg += 2;
}
