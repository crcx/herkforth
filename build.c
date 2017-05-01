/* Copyright (C) 2004 Jason Woofenden
 
   This file is part of herkforth.
 
   herkforth is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
 
   herkforth is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with herkforth; see the file COPYING.  If not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



// register numbers
#define R_HERE 17
#define R_DICT_END 19
#define R_DICT 20
#define R_DATA 23
#define R_DATA_END 24
#define R_SOURCE 25
#define R_SOURCE_END 26

#ifndef true
	#define true -1
#endif
#ifndef false
	#define false -1
#endif

// data types
#define TYPE_NONE 0
#define TYPE_STRING 1

#define COLOR_EXECUTE 0
#define COLOR_COMPILE 1
#define COLOR_NUMBER 2
#define COLOR_LITERAL 3
#define COLOR_TIC 4
#define COLOR_NOOP 5
#define COLOR_DEFINE_CONSTANT 6
#define COLOR_DEFINE 7
// the following colors are not real herkforth colors, they are the various types that use COLOR_NOOP
#define COLOR_VARIABLE 8
#define COLOR_DATA 9

#define FLAGS_WORD     0x00
#define FLAGS_NUMBER   0xc0
#define FLAGS_VARIABLE 0x90
#define FLAGS_DATA     0x88
#define FLAGS_CONSTANT 0xa0

#define LIT_FLAGS(a) ((a) << 24 | 0x20000)
#define WORD_FLAGS(a) ((a) << 24)

uint32_t color_to_dict_flags[] = {WORD_FLAGS(FLAGS_WORD),
                                  WORD_FLAGS(FLAGS_WORD),
				  LIT_FLAGS(FLAGS_NUMBER),
				  LIT_FLAGS(FLAGS_NUMBER),
				  0,
				  0,
				  LIT_FLAGS(FLAGS_CONSTANT),
				  WORD_FLAGS(FLAGS_WORD),
// the following colors are not real herkforth colors, they are the various types that use COLOR_NOOP
                                  LIT_FLAGS(FLAGS_VARIABLE),
				  LIT_FLAGS(FLAGS_DATA)
				 };

void laddr(uint32_t *out, uint32_t reg, uint32_t address) {
	out[0] = 0x3c000000 | (address >>16) | (reg << 21);
	out[1] = 0x60000000 | (address & 0xffff) | (reg << 21) | (reg << 16);
}

typedef struct {
	union {
		uint32_t cfa;
		uint32_t value;
	};
	union {
		uint32_t flags;
		struct {
			uint8_t type;
			uint8_t color_mod;
			uint16_t unused_flags;
		};
	};
	uint32_t variable_value;
	uint16_t source_block;
	uint16_t source_token;
	union {
		uint32_t wname[4];
		int8_t name[16];
	};
} dict_entry;

///////////////////////////////////////////////
///////////   GLOBALS
///////////////////////////////////////////////
#define dict_new (dict[dict_count])
#define dict_new_str (&(dict[dict_count].name[0]))
#define dict_new_wstr (&(dict[dict_count].wname[0]))
dict_entry *dict;
uint32_t dict_count;
uint16_t *src;
uint32_t src_count;
uint32_t *data;
uint32_t data_count;
int8_t *input_buffer;
int input_buffer_size;
int input_buffer_cur;
int which_block;
int word_length;
int NUM_BLOCKS;



void die(char *error_message) {
	fprintf(stderr, "error in blocks/%03d while processing \"%s\" before byte %d:  %s\n", which_block, dict_new_str, input_buffer_cur, error_message);
	exit(2);
}

uint32_t dict_append(int color) {
	// the name string is already in the dictionary becaues that's where we read it in
	dict_new.flags = color_to_dict_flags[color];
	return dict_count++;
}

uint32_t find(uint32_t *name) {
	int i;
	uint32_t w1, w2, w3, w4;
	w1 = name[0];
	w2 = name[1];
	w3 = name[2];
	w4 = name[3];

	for(i = 0; i < dict_count; ++i) {
		if(dict[i].wname[0] == w1) {
			if(dict[i].wname[1] == w2) {
				if(dict[i].wname[2] == w3) {
					if(dict[i].wname[3] == w4) {
						return i;
					}
				}
			}
		}
	}
	
	return -1;
}

uint32_t find_comma() {
	memcpy(dict_new_str + 33, dict_new_str, 15);
	dict_new_str[32] = ',';
	return find(&(dict[dict_count + 1].wname[0]));
}

void fix_data(uint32_t data_offset) {
	int i;

	for(i = 0; i < dict_count; ++i) {
		if(dict[i].type == FLAGS_DATA) {
			dict[i].value += data_offset;
		}
	}
}

void fix_variables(uint32_t a) {
	int i;

	a += 8; // point a at variable_value field

	for(i = 0; i < dict_count; ++i, a += 32) {
		if(dict[i].type == FLAGS_VARIABLE) {
			dict[i].value = a;
		}
	}
}

void src_append(int entry, int color) {
	src[src_count++] = (entry << 4) | color;
}

void data_append(uint32_t in) {
	data[data_count++] = in;
}

int word() {
	int8_t *out;

	int i;
	int8_t c;

	out = dict_new_str;
	
	// eat white space
	for(i = 0, c = 1; i < 16; ++i) {
		if(input_buffer_cur == input_buffer_size) {
			return 0;
		}
		c = input_buffer[input_buffer_cur++];
		if(c != '\n' && c != '\t' && c != ' ') {
			break;
		}
	}

	// clear out existing chars from dict
	dict_new.wname[0] = 0;
	dict_new.wname[1] = 0;
	dict_new.wname[2] = 0;
	dict_new.wname[3] = 0;

	*out++ = c;
	for(; i < 16; ++i) {
		c = input_buffer[input_buffer_cur++];
		if(c == '\n' || c == '\t' || c == ' ') {
			word_length = out - dict_new_str;
			return 1;
		}
		*out++ = c;
		if(input_buffer_cur == input_buffer_size) {
			word_length = out - dict_new_str;
			return 1;
		}
	}

	die("Word too long");
	return 0;
}

int8_t to_digit(int8_t in) {
	if(in <= '9') return in - '0';
	return in - 'a' + 10;
}

void parse_string() {
	int8_t *out;
	uint32_t *count;

	int8_t c;

	data[data_count++] = TYPE_STRING;
	count = &(data[data_count++]);
	out = (int8_t*)&data[data_count];
	
	if(input_buffer_cur == input_buffer_size) {
		die("tried to read string but file ends where string should start");
	}

	for(;;) {
		c = input_buffer[input_buffer_cur++];
		if(c == '\n' || input_buffer_cur == input_buffer_size) {
			*count = out - (int8_t*)&data[data_count];
			data_count += ((*count) + 3) / 4;
			return;
		} else
		if (c == '~') {
			c = input_buffer[input_buffer_cur++];
			c = (to_digit(c) << 4) + to_digit(input_buffer[input_buffer_cur++]);
		}

		*out++ = c;
	}
}

void ignore_string() {
	int8_t c = 0;

	while(c != '\n' && input_buffer_cur < input_buffer_size) {
		c = input_buffer[input_buffer_cur++];
	}
}

uint32_t parse_hex_number() {
	int i;
	uint32_t number;
	int8_t c, *in;
	
	number = 0;
	in = dict_new_str;
	for(i = 1; i < word_length; ++i) {
		c = in[i];
		if(c > '9') {
			c -= 87;
		} else {
			c -= '0';
		}
		number *= 16;
		number += c;
	}

	return number;
}


uint32_t parse_number() {
	int i;
	int32_t number;
	int8_t c, *in;
	int32_t negative;
	
	in = dict_new_str;
	if(in[0] == '$') {
		return parse_hex_number();
	}


	i = 0;
	negative = 1;
	if(in[0] == '-') {
		if(word_length == 1) {
			// just a minus sign, not a negative number
			die("Word not found");
		}
		negative = -1;
		i = 1;
	}

	number = 0;

	for(; i < word_length; ++i) {
		c = in[i];
		if(c > '9' || c < '0') {
			die("Word not found");
		}
		number *= 10;
		number += c - '0';
	}

	number *= negative;

	return number;
}

uint32_t new_number_or_die() {
	dict_new.value = parse_number();
	return dict_append(COLOR_NUMBER);
}






#define INPUT_BUFFER_SIZE 50120

int main(int argc, char *argv[])
{
	int fd;
	int i, color, tmp_color, entry;
	uint16_t block_length;
	uint16_t *src_blocks;

	char* buffer;
	uint32_t code_length, dict_pad, new_elf_length;
	int infile;
	int outfile;
	int kernel_left;
	uint32_t *regloads;
	uint32_t a;

	NUM_BLOCKS = argc - 1;

	data = (uint32_t*) malloc(4096);
	data_count = 4;

	// initialize globals
	dict = (dict_entry*) malloc(1024 * NUM_BLOCKS);
	dict_count = 0;
	input_buffer = malloc(INPUT_BUFFER_SIZE);
	src_blocks = (uint16_t*) malloc(1024 * NUM_BLOCKS);
	memset(dict, 0, 1024 * NUM_BLOCKS);
	memset(src_blocks, 0, 1024 * NUM_BLOCKS);


	// read/parse dictionary file
	fd = open("blocks/dictionary", O_RDONLY);
	input_buffer_size = read(fd, input_buffer, 100000);
	close(fd);
	dict_count = 0;
	input_buffer_cur = 0;
	while(input_buffer_cur < input_buffer_size) {
		word(); // read name
		dict_count++; // new entry
		// other fields:
		word();
		dict[dict_count - 1].cfa = parse_number();
		word();
		dict[dict_count - 1].flags = parse_number();
		word();
		dict[dict_count - 1].variable_value = parse_number();
	}


	// read/parse data file
	fd = open("blocks/data", O_RDONLY);
	input_buffer_size = read(fd, input_buffer, 100000);
	close(fd);
	input_buffer_cur = 0;
	while(input_buffer_cur < input_buffer_size) {
		word(); // read name
		entry = find(dict_new_wstr);
		dict[entry].value = data_count * 4 + 8;
		parse_string();
	}

	
	for(which_block = 0; which_block < NUM_BLOCKS; ++which_block) {
		src = &src_blocks[which_block * 512];
		src_count = 0;

		i = open(argv[which_block + 1], O_RDONLY);
		if(which_block % 2) {
			block_length = read(i, src, 1020);
			src[511] = block_length;
			close(i);
			continue;
		}
			
			
		input_buffer_size = read(i, input_buffer, INPUT_BUFFER_SIZE);
		input_buffer_cur = 0;
		close(i);


		color = COLOR_EXECUTE;
		while(word()) {
			//fprintf(stderr, "%s ", dict_new_str);

			// handle color change symbols
			if(dict_new_str[0] == '[') {
				if(dict_new_str[1] != 0) die("Error with \"[\": There must be a space after color change tokens");
				color = COLOR_EXECUTE;
				continue;
			}
			if(dict_new_str[0] == ']') {
				if(dict_new_str[1] != 0) die("Error with \"]\": There must be a space after color change tokens");
				color = COLOR_COMPILE;
				continue;
			}
			if(dict_new_str[0] == ':') {
				if(dict_new_str[1] != 0) die("Error with \":\": There must be a space after color change tokens");
				color = COLOR_DEFINE;
				continue;
			}
			if(dict_new_str[0] == '`') {
				if(dict_new_str[1] != 0) die("Error with \"`\": There must be a space after color change tokens");
				color = COLOR_TIC;
				continue;
			}
			if(dict_new_str[0] == '\'') {
				if(dict_new_str[1] != 0) die("Error with \"'\": There must be a space after color change tokens");
				color = COLOR_DEFINE_CONSTANT;
				continue;
			}
			if(dict_new_str[0] == '&') {
				if(dict_new_str[1] != 0) die("Error with \"&\": There must be a space after color change tokens");
				word();
				entry = find(dict_new_wstr);

				src_append(entry, COLOR_NOOP);
				continue;
			}
			if(dict_new_str[0] == '"') {
				if(dict_new_str[1] != 0) die("Error with \"\"\": There must be a space after color change tokens");
				word();
				entry = find(dict_new_wstr);

				src_append(entry, COLOR_NOOP);
				continue;
			}

			entry = -1;
			tmp_color = color;

			if(color == COLOR_COMPILE) {
				entry = find_comma();
				if(entry != -1) {
					tmp_color = COLOR_EXECUTE;
				}
			}

			if(entry == -1) {
				entry = find(&(dict[dict_count].wname[0]));
				if(entry != -1 && tmp_color <= COLOR_COMPILE) {
					tmp_color += dict[entry].color_mod;
				}
			}

			if(entry == -1) {
				if(tmp_color == COLOR_DEFINE || tmp_color == COLOR_DEFINE_CONSTANT) {
					entry = dict_append(tmp_color);
				} else {
					entry = new_number_or_die();
					tmp_color += 2;
				}
			}

			// set the source entry in dictionary
			if(tmp_color == COLOR_DEFINE || tmp_color == COLOR_DEFINE_CONSTANT) {
				dict[entry].source_block = which_block;
				dict[entry].source_token = src_count;
			}

			src_append(entry, tmp_color);

			color %= 2; // switch from const-def/define to execute/compile
		}
		src[510] = 0; // cursor position
		src[511] = src_count;
	}

	// fwrite(dict, 32, dict_count, stdout);
	// fwrite(src_blocks, 1024, NUM_BLOCKS, stdout);


	infile = open("kernel.s.out", O_RDONLY);
	outfile = open("herkforth000", O_WRONLY | O_CREAT, 0755);

	// reuse input_buffer
	buffer = input_buffer;

	// copy the first bit of the ELF header as is
	read(infile, buffer, 0x44);
	write(outfile, buffer, 0x44);

	read(infile, (char*)&code_length, 4);

	// get the number of bytes reqired to allign the dictionary (after the kernel code) on a 32 byte boundry
	dict_pad = (32 - ((0x10000054 + code_length) % 32)) % 32;

	new_elf_length = code_length + dict_pad + (dict_count * 32) + (data_count * 4) + (NUM_BLOCKS * 1024);

	// write out new source length
	write(outfile, (char*)&new_elf_length, 4);

	// read in last 12 bytes of ELF header, padding where our 7 register loads need to go, and the asm kernel.
	kernel_left = read(infile, buffer, INPUT_BUFFER_SIZE);

	regloads = (uint32_t*)buffer;
	regloads += 3;

	// create instructions to load the registers needed by the kernel
	a = 0x10000054 + code_length + dict_pad;
	laddr(regloads, R_DICT, a);
	fix_variables(a);
	a += dict_count * 32;
	laddr(regloads + 2, R_DICT_END, a);
	fix_data(a);
	laddr(regloads + 4, R_DATA, a);
	a += data_count * 4;
	laddr(regloads + 6, R_DATA_END, a);
	laddr(regloads + 8, R_SOURCE, a);
	a += NUM_BLOCKS * 1024;
	laddr(regloads + 10, R_SOURCE_END, a);
	a += 3; // TODO delete this and the following line
	a -= a % 4;
	laddr(regloads + 12, R_HERE, a);

	// write the rest of the elf file, our register loads, and the asm kernel to our new file
	write(outfile, buffer, kernel_left);

	
	/////////////////////////////////////////////////////////////////////////
	//            Write the DICTIONARY, DATA and SOURCE sections           //
	/////////////////////////////////////////////////////////////////////////

	// align dictionary
	if(dict_pad) {
		memset(buffer, 0, dict_pad);
		write(outfile, buffer, dict_pad);
	}
	
	// write dictionary
	write(outfile, dict, 32 * dict_count);

	// write data section
	write(outfile, data, 4 * data_count);

	// write source section
	write(outfile, src_blocks, 1024 * NUM_BLOCKS);

	close(outfile);

	// TODO os.chmod(outfile.name, 0733)
	
	// print statistics:
	if(1) {
		int total_tokens;
		printf("%d\tDictionary Entries\n", dict_count);
		total_tokens = 0;
		for(i = 0; i < NUM_BLOCKS; i += 2) {
			total_tokens += (src_blocks + (i * 512))[511];
		}
		printf("%d\tSource Tokens\n", total_tokens);
		printf("%d\tData Words\n", data_count);
		if(0) {
			printf("Block Lengths:\n");
			for(i = 0; i < NUM_BLOCKS; ++i)
			{
				printf("\t%d\t%d\n", i, (src_blocks + (i * 512))[511]);
			}
		}
	}
	return 0;
}
