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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "wsp.h"
#include "utils.h"

int svn_revision()
{
	char *s = BUILD_NUM;

	while (*s)
	{
		if (*s == ':')
		{
			s++;
			return atoi(s);
		}

		s++;
	}

	return 0;
}

void debug_printf(unsigned int debug_level, const char* format, ... ) 
{
	if (debug >= debug_level)
	{
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
}

//
// Parses a date in BCD format (http://en.wikipedia.org/wiki/Binary-coded_decimal).
// Each nibble of each byte corresponds to one number. The first byte contains the year.
//
bcd_date_t parse_bcd_date(unsigned char date[5])
{
	bcd_date_t d;
	d.year		= 2000 + (((date[0] >> 4) & 0xf) * 10) + (date[0] & 0xf);
	d.month 	= (((date[1] >> 4) & 0xf) * 10) + (date[1] & 0xf);
	d.day 		= (((date[2] >> 4) & 0xf) * 10) + (date[2] & 0xf);
	d.hour 		= (((date[3] >> 4) & 0xf) * 10) + (date[3] & 0xf);
	d.minute 	= (((date[4] >> 4) & 0xf) * 10) + (date[4] & 0xf);
	return d;
}

//
// Prints a BCD date.
//
void print_bcd_date(unsigned char date[5])
{
	bcd_date_t d = parse_bcd_date(date);
	printf("%u-%02u-%02u %02u:%02u:00", d.year, d.month, d.day, d.hour, d.minute);
}

const char *get_bcd_date_string(unsigned char date[5])
{
	static char str[1024];
	bcd_date_t d = parse_bcd_date(date);
	sprintf(str, "%u-%02u-%02u %02u:%02u:00", d.year, d.month, d.day, d.hour, d.minute);
	return str;
}

//
// Gets a string for the wind direction from the settings byte.
//
const char *get_wind_direction(const char data)
{
	switch (data)
	{
		default:
		case 0: return "N";
		case 1: return "NNE";
		case 2: return "NE";
		case 3: return "NEE";
		case 4: return "E";
		case 5: return "SEE";
		case 6: return "SE";
		case 7: return "SSE";
		case 8: return "S";
		case 9: return "SSW";
		case 10: return "SW";
		case 11: return "WSW";
		case 12: return "W";
		case 13: return "WNW";
		case 14: return "NW";
		case 15: return "NNW";
	}
}

//
// Prints bytes for debug purposes.
//
void print_bytes(unsigned int debug_level, char *bytes, unsigned int len)
{
    if ((debug >= debug_level) && (len > 0))
	{
		unsigned int i;

		for (i = 0; i < len; i++)
		{
			printf("%02x ", (int)((unsigned char)bytes[i]));
		}
		
		debug_printf(debug_level, "\n");
    }
}

int file_exists(const char *filename)
{
	FILE *f;
	
	if ((f = fopen(program_settings.dumpfile, "r")) == NULL)
	{
		return 0;
	}
	
	fclose(f);
	
	return 1;
}

char prompt_user()
{
	char c;
	fflush(stdin);
	scanf("%c", &c);
	return c;
}

char *get_timestamp(time_t t)
{
	static char tbuf[128];
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:00", localtime(&t));
	return tbuf;
}

char *get_local_timestamp()
{
	static char tbuf[128];
	time_t now = time(NULL);
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:00", localtime(&now));
	return tbuf;
}

time_t bcd_to_unix_date(bcd_date_t date)
{
	time_t rawtime;
	struct tm timeinfo;

	memset(&timeinfo, 0, sizeof(timeinfo));
	timeinfo.tm_year	= date.year - 1900;
	timeinfo.tm_mon		= date.month - 1;
	timeinfo.tm_mday	= date.day;
	timeinfo.tm_hour	= date.hour;
	timeinfo.tm_min		= date.minute;
	timeinfo.tm_isdst	= 1;

	rawtime = mktime(&timeinfo);

	return rawtime;
}

