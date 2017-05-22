/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2017 Skyport Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

/* Use this programmer if the SPI signals to your flash chip are
 * connected to GPIOs, and the GPIO controller has a sysfs interface (see
 * Documentation/gpio/sysfs.txt in the Linux source tree).
 * 
 * Pass the GPIO sysfs paths for the SPI signals as programmer
 * parameters:
 *   -p linux_gpio_spi:cs=/sys/class/gpio/gpio451,sck=...,mosi=...,miso=...
 */

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include "programmer.h"

int f_cs, f_sck, f_mosi, f_miso;

const char ZERO[] = { '0' };
const char ONE[] = { '1' };

static void set_cs(int val)
{
	if (write(f_cs, val ? ONE : ZERO, 1) != 1)
		msg_perr("Failed to write CS GPIO\n");
}

static void set_sck(int val)
{
	if (write(f_sck, val ? ONE : ZERO, 1) != 1)
		msg_perr("Failed to write SCK GPIO\n");
}

static void set_mosi(int val)
{
	if (write(f_mosi, val ? ONE : ZERO, 1) != 1)
		msg_perr("Failed to write MOSI GPIO\n");
}

static int get_miso(void)
{
	char buf[1];

	lseek(f_miso, 0, SEEK_SET);
	if (read(f_miso, buf, 1) != 1) {
		msg_perr("Failed to read MISO GPIO\n");
		return -1;
	}
	return buf[0] == '1';
}

static const struct bitbang_spi_master bitbang_spi_master_gpio = {
	.type = BITBANG_SPI_MASTER_LINUX_GPIO,
	.set_cs = set_cs,
	.set_sck = set_sck,
	.set_mosi = set_mosi,
	.get_miso = get_miso,
	.half_period = 0,
};

static int open_gpio(char *name, char *dir, int flags) {
	char *arg;
	char p[PATH_MAX];
	int f;

	arg = extract_programmer_param(name);
	if (!arg) {
		msg_perr("Missing %s= param\n", name);
		return -1;
	}
	snprintf(p, PATH_MAX, "%s/direction", arg);
	p[PATH_MAX - 1] = '\0';
	f = open(p, O_WRONLY);
	if (f >= 0) {
		int l = write(f, dir, strlen(dir));

		close(f);
		if (l != strlen(dir)) {
			msg_perr("Failed to write %s to %s\n", dir, p);
			return -1;
		}
	}
	snprintf(p, PATH_MAX, "%s/value", arg);
	p[PATH_MAX - 1] = '\0';
	f = open(p, flags);
	if (f < 0) {
		msg_perr("Failed to open %s\n", p);
		return -1;
	}
	return f;
}

int linux_gpio_spi_init(void)
{
	f_cs = open_gpio("cs", "out", O_WRONLY);
	if (f_cs < 0)
		return 1;
	f_sck = open_gpio("sck", "out", O_WRONLY);
	if (f_sck < 0)
		return 1;
	f_mosi = open_gpio("mosi", "out", O_WRONLY);
	if (f_mosi < 0)
		return 1;
	f_miso = open_gpio("miso", "in", O_RDONLY);
	if (f_miso < 0)
		return 1;

	if (register_spi_bitbang_master(&bitbang_spi_master_gpio)) {
		msg_perr("Failed to register Linux GPIO bitbang SPI master\n");
		return 1;
	}

	return 0;
}
