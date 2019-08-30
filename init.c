/*
  stm32flash - Open Source ST STM32 flash program for *nix
  Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>
  Copyright (C) 2013 Antonio Borneo <borneo.antonio@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/gpio.h>

#include "init.h"
#include "serial.h"
#include "stm32.h"
#include "port.h"
#include "gpio-utils.h"

#define COMSUMER "stm32flash"
#define DEVICE_NAME "gpiochip0"

struct gpio_list {
	struct gpio_list *next;
	unsigned int gpio;
    int fd;
};

#if !defined(__linux__)
static int drive_gpio(int n, int level, struct gpio_list **gpio_to_release)
{
	fprintf(stderr, "GPIO control only available in Linux\n");
	return 0;
}
#else

static int drive_gpio(int n, int level, struct gpio_list **gpio_to_release)
{
	struct gpio_list *new;
    struct gpiohandle_data data;
    int found = 0;
    struct gpio_list *item = *gpio_to_release;
    for (; item != NULL; item = item->next)
    {
        if (item->gpio == n){
            found = 1;
            break;
        }
    }
    if (!found) {
        // create new handle
        printf("New gpio %d.\n", n);
		new = (struct gpio_list *)malloc(sizeof(struct gpio_list));
		if (new == NULL) {
			fprintf(stderr, "Out of memory\n");
			return 0;
		}
        data.values[0] = level;
        new->next = *gpio_to_release;
        new->gpio = n;
        new->fd = gpiotools_request_linehandle(DEVICE_NAME, &new->gpio, 1,
					   GPIOHANDLE_REQUEST_OUTPUT, &data,
					   COMSUMER);
        printf("Create handle with %d\n", new->fd);
        if (new->fd < 0) {
            perror("Error");
            exit(-1);
        }
        *gpio_to_release = new;
    }
    else
    {
        printf("Line founded. Try write %d value=%d\n", item->fd, level);
        data.values[0] = level;
        gpiotools_set_values(item->fd, &data);
    }
    return 1;



}
#endif


static int gpio_sequence(struct port_interface *port, const char *s, size_t l)
{
	struct gpio_list *gpio_to_release = NULL, *to_free;
	int ret, level, gpio;

	ret = 1;
	while (ret == 1 && *s && l > 0) {
		if (*s == '-') {
			level = 0;
			s++;
			l--;
		} else
			level = 1;

		if (isdigit(*s)) {
			gpio = atoi(s);
			while (isdigit(*s)) {
				s++;
				l--;
			}
		} else if (!strncmp(s, "rts", 3)) {
			gpio = -GPIO_RTS;
			s += 3;
			l -= 3;
		} else if (!strncmp(s, "dtr", 3)) {
			gpio = -GPIO_DTR;
			s += 3;
			l -= 3;
		} else if (!strncmp(s, "brk", 3)) {
			gpio = -GPIO_BRK;
			s += 3;
			l -= 3;
		} else {
			fprintf(stderr, "Character \'%c\' is not a digit\n", *s);
			ret = 0;
			break;
		}

		if (*s && (l > 0)) {
			if (*s == ',') {
				s++;
				l--;
			} else {
				fprintf(stderr, "Character \'%c\' is not a separator\n", *s);
				ret = 0;
				break;
			}
		}
		if (gpio < 0)
			ret = (port->gpio(port, -gpio, level) == PORT_ERR_OK);
		else
			ret = drive_gpio(gpio, level, &gpio_to_release);
		usleep(100000);
	}

	while (gpio_to_release) {
        gpiotools_release_linehandle(gpio_to_release->fd);
		to_free = gpio_to_release;
		gpio_to_release = gpio_to_release->next;
		free(to_free);
	}
	usleep(500000);
	return ret;
}

static int gpio_bl_entry(struct port_interface *port, const char *seq)
{
	char *s;

	if (seq == NULL || seq[0] == ':')
		return 1;

	s = strchr(seq, ':');
	if (s == NULL)
		return gpio_sequence(port, seq, strlen(seq));

	return gpio_sequence(port, seq, s - seq);
}

static int gpio_bl_exit(struct port_interface *port, const char *seq)
{
	char *s;

	if (seq == NULL)
		return 1;

	s = strchr(seq, ':');
	if (s == NULL || s[1] == '\0')
		return 1;

	return gpio_sequence(port, s + 1, strlen(s + 1));
}

int init_bl_entry(struct port_interface *port, const char *seq)
{
	if (seq)
		return gpio_bl_entry(port, seq);

	return 1;
}

int init_bl_exit(stm32_t *stm, struct port_interface *port, const char *seq)
{
	if (seq && strchr(seq, ':'))
		return gpio_bl_exit(port, seq);

	if (stm32_reset_device(stm) != STM32_ERR_OK)
		return 0;
	return 1;
}
