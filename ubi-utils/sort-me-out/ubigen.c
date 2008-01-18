/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * An utility to generate UBI images.
 *
 * Authors: Oliver Lohmann
 *          Artem Bityutskiy
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mtd/ubi-header.h>

#include "ubigen.h"
#include "common.h"

#define PROGRAM_VERSION "1.4"
#define PROGRAM_NAME    "ubigen"

struct args {
	FILE *fp_in;
	FILE *fp_out;
	int peb_size;
	int id;
	int min_io_size;
	int type;
	int sub_page_size;
	int alignment;
	int vid_hdr_offs;
	int ec;
	int ubi_ver;
};

struct args args = {
	.fp_in = NULL,
	.fp_out = NULL,
	.peb_size = -1,
	.id = -1,
	.min_io_size = -1,
	.type = UBI_VID_DYNAMIC,
	.sub_page_size = -1,
	.alignment = 1,
	.vid_hdr_offs = 0,
	.ec = 0,
	.ubi_ver = 0,
};

static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
" - a tool for adding UBI headers to a binary image.\n"
"Note, the images generated by this program are not ready to be used\n"
"because they do not contain the volume table. If not sure about one\n"
"of the parameters, do not specify and let the utility to use default\n"
"values.";

static const char *optionsstr =
"-i, --infile=<filename>       the input file\n"
"-o, --outfile=<filename>      the output file (default is stdout)\n"
"-b, --peb-size=<bytes>        size of the physical eraseblock of the flash this\n"
"                              UBI image is created for in bytes, kilobytes (KiB),\n"
"                              or megabytes (MiB) (mandatory parameter)\n"
"-I, --vol-id=<num>            volume ID (mandatory parameter)\n"
"-m, --min-io-size=<bytes>     minimum input/output unit size of the flash in bytes\n"
"                              kilobytes (KiB), or megabytes (MiB) (mandatory\n"
"                              parameter); e.g. this is NAND page size in case of\n"
"                              NAND flash\n"
"-t, --type=<static|dynamic>   volume type: dynamic or static (default is dynamic)\n"
"-s, --sub-page-size=<bytes>   minimum input/output unit used for UBI headers, e.g.\n"
"                              sub-page size in case of NAND flash (equivalent to\n"
"                              the minimum input/output unit size by default)\n"
"-a, --alignment=<bytes>       volume alignment in bytes, kilobytes (KiB), or\n"
"                              megabytes (MiB) (default is 1)\n"
"-O, --vid-hdr-offset=<num>    offset if the VID header from start of the physical\n"
"                              eraseblock (default is the second minimum I/O unit\n"
"                              or sub-page, if it was specified)\n"
"-e, --erase-counter=<num>     the erase counter value to put to EC headers\n"
"                              (default is 0)\n"
"-x, --ubi-ver=<num>            UBI version number to put to EC headers\n"
"                              (default is 1)\n"
"-h, --help                    print help message\n"
"-V, --version                 print program version";

static const char *usage =
"Usage: " PROGRAM_NAME " -i <input file> -o <output file> -b <PEB size>\n"
"              -I <volume ID> -m <min I/O unit size> [-s <sub-page size>]\n"
"              [-a <alignment>] [-O <volume ID header offset>]\n"
"              [-e <erase counter value>] [-x <UBI version>] [-h] [-V]";

struct option long_options[] = {
	{ .name = "infile",         .has_arg = 1, .flag = NULL, .val = 'i' },
	{ .name = "outfile",        .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "peb-size",       .has_arg = 1, .flag = NULL, .val = 'b' },
	{ .name = "vol-id",         .has_arg = 1, .flag = NULL, .val = 'I' },
	{ .name = "min-io-size",    .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "type",           .has_arg = 1, .flag = NULL, .val = 't' },
	{ .name = "sub-page-size",  .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "alignment",      .has_arg = 1, .flag = NULL, .val = 'a' },
	{ .name = "vid-hdr-offset", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "erase-counter",  .has_arg = 1, .flag = NULL, .val = 'e' },
	{ .name = "ubi-ver",        .has_arg = 1, .flag = NULL, .val = 'x' },
	{ .name = "help",           .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",        .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;
		char *endp;

		key = getopt_long(argc, argv, "i:o:b:I:m:t:s:a:O:e:x:hV",
				  long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'o':
			args.fp_out = fopen(optarg, "wb");
			if (!args.fp_out) {
				errmsg("cannot open file \"%s\"", optarg);
				return -1;
			}
			break;

		case 'i':
			args.fp_in = fopen(optarg, "rb");
			if (!args.fp_in) {
				errmsg("cannot open file \"%s\"", optarg);
				return -1;
			}
			break;

		case 'b':
			args.peb_size = strtoull(optarg, &endp, 0);
			if (endp == optarg || args.peb_size <= 0) {
				errmsg("bad physical eraseblock size: \"%s\"", optarg);
				return -1;
			}
			if (*endp != '\0') {
				int mult = ubiutils_get_multiplier(endp);

				if (mult == -1) {
					errmsg("bad size specifier: \"%s\" - "
					       "should be 'KiB', 'MiB' or 'GiB'", endp);
					return -1;
				}
				args.peb_size *= mult;
			}
			break;

		case 'm':
			args.min_io_size = strtoull(optarg, &endp, 0);
			if (endp == optarg || args.min_io_size <= 0) {
				errmsg("bad min. I/O unit size: \"%s\"", optarg);
				return -1;
			}
			if (*endp != '\0') {
				int mult = ubiutils_get_multiplier(endp);

				if (mult == -1) {
					errmsg("bad size specifier: \"%s\" - "
					       "should be 'KiB', 'MiB' or 'GiB'", endp);
					return -1;
				}
				args.min_io_size *= mult;
			}
			break;

		case 'e':
			args.ec = strtoul(optarg, &endp, 0);
			if (endp == optarg || args.ec < 0) {
				errmsg("bad erase counter value: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'I':
			args.id = strtoul(optarg, &endp, 0);
			if (endp == optarg || args.id < 0) {
				errmsg("bad volume ID: \"%s\"", optarg);
				return -1;
			}
			break;

		case 't':
			if (!strcmp(optarg, "dynamic"))
				args.type = UBI_VID_DYNAMIC;
			else if (!strcmp(optarg, "static"))
				args.type = UBI_VID_STATIC;
			else {
				errmsg("bad volume type: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'x':
			args.ubi_ver = strtoul(optarg, &endp, 0);
			if (endp == optarg || args.ubi_ver < 0) {
				errmsg("bad UBI version: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'O':
			args.vid_hdr_offs = strtoul(optarg, &endp, 0);
			if (endp == optarg || args.vid_hdr_offs < 0) {
				errmsg("bad VID header offset: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'h':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(EXIT_SUCCESS);

		case ':':
			errmsg("parameter is missing");
			return -1;

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (!args.fp_in) {
		errmsg("input file was not specified (use -h for help)");
		return -1;
	}

	if (!args.fp_out)
		args.fp_out = stdout;

	if (args.id < 0) {
		errmsg("wolume ID was not specified (use -h for help)");
		return -1;
	}

	if (args.peb_size < 0) {
		errmsg("physical eraseblock size was not specified "
		       "(use -h for help)");
		return -1;
	}

	if (args.min_io_size < 0) {
		errmsg("min. I/O unit size was not specified "
		       "(use -h for help)");
		return -1;
	}

	if (args.sub_page_size < 0)
		args.sub_page_size = args.min_io_size;

	return 0;
}


int main(int argc, char * const argv[])
{
	int err;
	ubi_info_t u;
	struct stat file_info;
	off_t input_len = 0; /* only used in static volumes */

	ubigen_init();

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	if (fstat(fileno(args.fp_in), &file_info) != 0) {
		fprintf(stderr, "Cannot fetch file size "
				"from input file.\n");
	}
	input_len = file_info.st_size;

	err = ubigen_create(&u, (uint32_t)args.id, args.type,
			args.peb_size, args.ec, args.alignment,
			args.ubi_ver, args.vid_hdr_offs, 0 ,input_len,
			args.fp_in, args.fp_out);

	if  (err) {
		fprintf(stderr, "Cannot create UBI info handler err: %d\n", err);
		return -1;
	}

	err = ubigen_write_complete(u);
	if  (err != 0) {
		fprintf(stderr, "Error converting input data.\n");
		return -1;
	}

	err = ubigen_destroy(&u);
	return err;
}
