#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>
#include "font.h"
#include "arg.h"

#define I2C_DEV "/dev/i2c-1"
#define MDH_ADR 0x61

#define DEFAULT_MODE 0x18
#define OPTS 0xE /* 0xE = 35mA, 0 = 40mA */

#define CMD_MODE    0x00
#define CMD_UPDATE  0x0C
#define CMD_OPTION  0x0D
#define CMD_BRIGHT  0x19
#define CMD_MATRIX1 0x01
#define CMD_MATRIX2 0x0E
#define CMD_RESET   0xFF

static int d;


__u8
i2c_set(__u8 reg, __u8 arg) {
	__u8 buf[2];

	buf[0] = reg;
	buf[1] = arg;

	return write(d, buf, 2) == 2;
}

__u8
i2c_get(__u8 reg, __u8 *ret, size_t n) {
	__u8 r;

	r = reg | 0xFF;

	if (write(d, &r, 1) <= 0)
		return 0;

	return read(d, ret, n) == n;
}

__u8
i2c_adr(__u8 addr) {
	return ioctl(d, I2C_SLAVE, addr);
}

void
printchar(int x, int chr) {
	int i, y, z;
	char buf[7], reg;

	reg = x % 2 ? CMD_MATRIX2 : CMD_MATRIX1;

	i2c_adr(MDH_ADR + x / 2);

	switch (chr) {
	case '\0':
	case '\n':
	case '\r':
		return;
	}

	if (x % 2) {
		/* flip */
		for (i = 0, y = 7; i < 7; i++, y--)
			buf[i] = 0;

		for (i = 0; i < 8; i++)
			for (y = 0; y < 7; y++)
				buf[i] |= font[chr][y] & (1 << i) ? 1 << y : 0;

		for (i = 0; i < 7; i++)
			i2c_set(reg + i, buf[i]);
	} else
		for (i = 0; i < 7; i++)
			i2c_set(reg + i, font[chr][i]);
}

void
printstring(char *str) {
	int x = 0;
	char *p = str;

	while (*p) {
		printchar(x++, *p++);

		if (x > 5)
			return;
	}
}

void
i2c_set_all(__u8 reg, __u8 val) {
	int i;

	for (i = 0; i < 3; i++) {
		i2c_adr(MDH_ADR + i);
		i2c_set(reg, val);
	}
}

void
update() {
	i2c_set_all(CMD_UPDATE, 0);
}

void
reset() {
	i2c_set_all(CMD_RESET, 0);
}

void
clear() {
	int i;

	for (i = 0; i < 6; i++)
		printchar(i, ' ');
}

void
brightness(int val) {
	i2c_set_all(CMD_BRIGHT, val);
}


int
main(int argc, char **argv) {
	int i;
	int rflag = 0;
	int hflag = 0;
	int iflag = 0;
	char *argv0;
	char line[8];

	d = open(I2C_DEV, O_RDWR);
	if (d < 0)
		err(1, "open '%s'", I2C_DEV);

	reset();
	i2c_set_all(CMD_MODE, DEFAULT_MODE);

	ARGBEGIN {
	case 'r':
		rflag = 1;
		break;
	case 'h':
		hflag = 1;
		break;
	case 'i':
		iflag = 1;
		break;
	case 'b':
		brightness(atoi(ARGF()));
		break;
	} ARGEND

	if (!iflag) {
		while (*argv)
			printstring(*argv++);
	} else {
		while (fgets(line, 8, stdin)) {
			clear();
			printstring(line);
			update();
		}
	}

	update();

	if (hflag) {
		getchar();
		rflag = 1;
	}

	if (rflag)
		reset();

	close(d);
	return 0;
}
