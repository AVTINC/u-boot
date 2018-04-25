/*
 * (C) Copyright 2014 Digi International, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <fuse.h>
#include <linux/errno.h>
#include "helper.h"
#include "hwid.h"
#ifdef CONFIG_OF_LIBFDT
#include <fdt_support.h>
#endif

__weak void board_print_hwid(u32 *hwid)
{
	int i;

	for (i = CONFIG_HWID_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", hwid[i]);
	printf("\n");
}

__weak void board_print_manufid(u32 *hwid)
{
	board_print_hwid(hwid);
}

__weak int manufstr_to_hwid(int argc, char *const argv[], u32 *val)
{
	printf("Undefined function for manufacturing string conversion\n");
	return -EPERM;
}

static int do_hwid(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 val[8];
	int ret, i;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "read") || !strcmp(op, "read_manuf")) {
		printf("Reading HWID: ");
		for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word++) {
			ret = fuse_read(bank, word, &val[i]);
			if (ret)
				goto err;
		}
		if (!strcmp(op, "read_manuf"))
			board_print_manufid(val);
		else
			board_print_hwid(val);
	} else if (!strcmp(op, "sense") || !strcmp(op, "sense_manuf")) {
		printf("Sensing HWID: ");
		for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word++) {
			ret = fuse_sense(bank, word, &val[i]);
			if (ret)
				goto err;
		}
		if (!strcmp(op, "sense_manuf"))
			board_print_manufid(val);
		else
			board_print_hwid(val);
	} else if (!strcmp(op, "prog")) {
		if (argc < CONFIG_HWID_WORDS_NUMBER)
			return CMD_RET_USAGE;

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming HWID... ");
		/* Write backwards, from MSB to LSB */
		word = CONFIG_HWID_START_WORD + CONFIG_HWID_WORDS_NUMBER - 1;
		for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word--) {
			if (strtou32(argv[i], 16, &val[i]))
				return CMD_RET_USAGE;

			ret = fuse_prog(bank, word, val[i]);
			if (ret)
				goto err;
		}
		printf("OK\n");
	} else if (!strcmp(op, "prog_manuf")) {
		if (argc < 1)
			return CMD_RET_USAGE;
		if (manufstr_to_hwid(argc, argv, val))
			return CMD_RET_FAILURE;
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming manufacturing information into HWID... ");
		for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word++) {
			ret = fuse_prog(bank, word, val[i]);
			if (ret)
				goto err;
		}
		printf("OK\n");
	} else if (!strcmp(op, "override")) {
		if (argc < CONFIG_HWID_WORDS_NUMBER)
			return CMD_RET_USAGE;

		printf("Overriding HWID... ");
		/* Write backwards, from MSB to LSB */
		word = CONFIG_HWID_START_WORD + CONFIG_HWID_WORDS_NUMBER - 1;
		for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word--) {
			if (strtou32(argv[i], 16, &val[i]))
				return CMD_RET_USAGE;

			ret = fuse_override(bank, word, val[i]);
			if (ret)
				goto err;
		}
		printf("OK\n");
	}  else if (!strcmp(op, "override_manuf")) {
		if (argc < 1)
			return CMD_RET_USAGE;
		if (manufstr_to_hwid(argc, argv, val))
			return CMD_RET_FAILURE;
		printf("Overriding manufacturing information into HWID... ");
		for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word++) {
			ret = fuse_override(bank, word, val[i]);
			if (ret)
				goto err;
		}
		printf("OK\n");
	} else if (!strcmp(op, "lock")) {
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Locking HWID... ");
		ret = fuse_prog(OCOTP_LOCK_BANK,
				OCOTP_LOCK_WORD,
				CONFIG_HWID_LOCK_FUSE);
		if (ret)
			goto err;
		printf("OK\n");
		printf("Locking of the HWID will be effective when the CPU is reset\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

err:
	puts("ERROR\n");
	return ret;
}

U_BOOT_CMD(
	hwid, CONFIG_SYS_MAXARGS, 0, do_hwid,
	"HWID on fuse sub-system",
	     "read - read HWID from shadow registers\n"
	"hwid read_manuf - read HWID from shadow registers and print manufacturing ID\n"
	"hwid sense - sense HWID from fuses\n"
	"hwid sense_manuf - sense HWID from fuses and print manufacturing ID\n"
	"hwid prog [-y] <high_word> <low_word> - program HWID (PERMANENT)\n"
	"hwid prog_manuf [-y] " CONFIG_MANUF_STRINGS_HELP " - program HWID with manufacturing ID (PERMANENT)\n"
	"hwid override <high_word> <low_word> - override HWID\n"
	"hwid override_manuf " CONFIG_MANUF_STRINGS_HELP " - override HWID with manufacturing ID\n"
	"hwid lock [-y] - lock HWID OTP bits (PERMANENT)\n"
);
