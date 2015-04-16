/*
 * (C) Copyright 2015
 * Digi International
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
#include <mmc.h>

extern int pmic_read_reg(int reg, unsigned char *value);
extern int pmic_write_reg(int reg, unsigned char value);

int do_pmic(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned int addr;
	unsigned char val;
	int i;
	int count = 1;
	int ret;

	if (argc < 3)
		return CMD_RET_USAGE;

	addr = simple_strtol(argv[2], NULL, 16);
	if (strcmp(argv[1], "read") == 0) {
		if (argc == 4)
			count = simple_strtol(argv[3], NULL, 16);
		for (i = 0; i < count; i++) {
			ret = pmic_read_reg(addr + i, &val);
			if (ret)
				printf("Error reading PMIC address 0x%x\n",
					addr + i);
			else
				printf("PMIC[0x%x]: 0x%02x\n", addr + i, val);
		}
	} else if (strcmp(argv[1], "write") == 0) {
		if (argc < 4)
			return CMD_RET_USAGE;
		val = simple_strtol(argv[3], NULL, 16);
		ret = pmic_write_reg(addr, (unsigned char)val);
		if (ret)
			printf("Error writing PMIC address 0x%x\n", addr);
	} else {
		return CMD_RET_USAGE;
	}

	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	pmic, 4, 1, do_pmic,
	"PMIC access",
	"read address [count] - read PMIC register(s)\n"
	"pmic write address value - write PMIC register"
);
