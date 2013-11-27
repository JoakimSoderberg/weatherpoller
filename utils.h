//
// Weather Station Poller
//
// Copyright (C) 2010 Joakim Söderberg
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

typedef struct bcd_date_s
{
	unsigned short year;
	unsigned short month;
	unsigned short day;
	unsigned short hour;
	unsigned short minute;
} bcd_date_t;

int svn_revision();
void debug_printf(unsigned int debug_level, const char* format, ... );
bcd_date_t parse_bcd_date(unsigned char date[5]);
void print_bcd_date(unsigned char date[5]);
const char *get_bcd_date_string(unsigned char date[5]);
const char *get_wind_direction(const char data);
void print_bytes(unsigned int debug_level, char *bytes, unsigned int len);
int file_exists(const char *filename);
char prompt_user();
char *get_timestamp(time_t t);
char *get_local_timestamp();
time_t bcd_to_unix_date(bcd_date_t date);

#endif // __UTILS_H__
