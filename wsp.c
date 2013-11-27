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
// ------------------------------------------------------------------------
//
// This program is designed to interface with the Fineoffset line
// of weather stations (WP1400, WH1080, WH1081, W-8681, etc).
//
// The information about how the data is stored on the device is
// based on information from Jim Easterbrooks webpage
// (http://www.jim-easterbrook.me.uk/weather/mm/)
// including the EasyWeather.dat layout
// (http://www.jim-easterbrook.me.uk/weather/ew/).
//

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
#include <getopt.h>
#include "wsp.h"
#include "wspusb.h"
#include "memory.h"
#include "utils.h"
#include "output.h"
#include "weather.h"

program_settings_t program_settings;

typedef unsigned char byte;

struct usb_dev_handle *devh;
unsigned int debug = 0;

//
// Shows usage.
//
void show_usage(char *program_name)
{
	printf("Weather Station Poller v%u.%u build %d\n", MAJOR_VERSION, MINOR_VERSION, svn_revision());
	printf("Copyright (C) Joakim Söderberg.\n");
	printf("  Usage: %s [option]... \n", program_name);
	printf("\n");
	printf("  -e, --easyweather     Outputs the weather data in the\n");
	printf("                        easyweather.dat csv format.\n");
	printf("  -s, --status          Shows status of the device, such\n");
	printf("                        as data count, date/time.\n");
	printf("  --settings            Prints the weather display's settings.\n");
	printf("  --maxmin              Outputs the max/min weather data\n");
	printf("                        recorded by the station.\n");
	printf("  --alarms              Displays all alarms set on the device\n");
	printf("                        and if they're enabled.\n");
	printf("  -c, --count #	        The number of history items to read (1-4080).\n");
	printf("                        Default is 1.\n");
	printf("  -a, --all             Gets all available history items.\n");
	printf("  -v[v..]               Shows extra debug information. For more\n");
	printf("                        detailed info, add more v's.\n");
	printf("  -t, --timezone #      Sets the timezone offset from CET\n");
	printf("                        from -12 to 12.\n");
	printf("  -d, --delay #         Sets the read update delay between\n");
	printf("                        weather data readings.\n");
	printf("  -A, --altitude #      Sets the altitude in m over sea level in meters.\n");
	printf("                        This is not saved anywhere, so it must be\n");
	printf("                        specified on each call. Used to calculate\n");
	printf("                        relative pressure.\n");
	printf("  --quickrain           Enables faster, and potentially inaccurate rain\n");
	printf("                        calculations. Instead of checking the time between\n");
	printf("                        each history item to get the accurate timestamp\n");
	printf("                        the delay is used. This will result in incorrect\n");
	printf("                        values if you changed the delay without resetting\n");
	printf("                        the memory. Notice that rain over 1h, 24h and so on\n");
	printf("                        might be calculated incorrectly.\n");
	printf("  --vendorid #          Changes the vendor id, should be in hex format.\n");
    printf("                        Default is %x.\n", VENDOR_ID);
	printf("  --productid #         Changes the product id, shoulb be in hex format.\n");
	printf("                        Default is %x.\n", PRODUCT_ID);
	printf("  --format <string>     Writes the output in the given format.\n");
	printf("  --formatlist          Lists available format string variables.\n");
	printf("  --dumpmem <path>      Dumps the entire weather station memory to a file.\n");
	printf("  --infile <path>       Uses a file as input instead of reading from the\n");
	printf("                        weather station memory. Use output from --dumpmem.\n");
	printf("  --reset               Resets all the data on the weather station.\n");
	printf("  --write #             Write a byte to a given address\n");
	printf("  --summary             Shows a small summary of the last recorded weather.\n");
	printf("  -h, --help            Shows this help text.\n");
	printf("\n");
}

//
// Handles SIGTERM.
//
void sigterm_handler(int signum)
{
	fprintf(stderr, "SIGTERM: Closing device\n");
	close_device(devh);
	exit(1);
}

void set_wind_direction(char *winddir, const char data)
{
	strcpy(winddir, get_wind_direction(data));
}

//
// Gets the raw bytes of the weather settings block.
//
void get_settings_block_raw(struct usb_dev_handle *h, char *buf, unsigned int len)
{
	unsigned int offset;

	assert(len <= WEATHER_SETTINGS_CHUNK_SIZE);
	assert((len % 32) == 0);

	memset(buf, 0, len);

	// Read 256 bytes in 32-byte chunks.
	for (offset = 0; (offset < WEATHER_SETTINGS_CHUNK_SIZE) && (offset < len); offset += 32)
	{
		// TODO: Check for error here.
		read_weather_address(h, offset, &buf[offset]);
		
		print_bytes(2, &buf[offset], 32);
	}
}

//
// Gets the weather settings block (the first 256 bytes in the weather display memory).
//
weather_settings_t get_settings_block(struct usb_dev_handle *h)
{
	weather_settings_t ws;
	char buf[WEATHER_SETTINGS_CHUNK_SIZE];

	memset(&ws, 0, sizeof(ws));

	get_settings_block_raw(h, buf, sizeof(buf));

	ws.magic_number[0]				= buf[0];
	ws.magic_number[1]				= buf[1];
	ws.read_period					= buf[16];
	ws.unit_settings1				= buf[17];
	ws.unit_settings2				= buf[18];
	ws.display_options1 			= buf[19];
	ws.display_options2 			= buf[20];
	ws.alarm_enable1				= buf[21];
	ws.alarm_enable2				= buf[22];
	ws.alarm_enable3				= buf[23];
	ws.timezone						= buf[24];
	ws.data_refreshed				= buf[26];
	ws.data_count					= (buf[27] & 0xff) | (buf[28] << 8);
	ws.current_pos					= (buf[30] & 0xff) | (buf[31] << 8);
	ws.relative_pressure			= (buf[32] & 0xff) | (buf[33] << 8);
	ws.absolute_pressure			= (buf[34] & 0xff) | (buf[35] << 8);
	memcpy(&ws.unknown, &buf[36], 7);
	memcpy(&ws.datetime, &buf[43], 5);
	ws.alarm_inhumid_high			= buf[48];
	ws.alarm_inhumid_low			= buf[49];
	ws.alarm_intemp_high			= FIX_SIGN((buf[50] & 0xff) | (buf[51] << 8));
	ws.alarm_intemp_low				= FIX_SIGN((buf[52] & 0xff) | (buf[53] << 8));
	ws.alarm_outhumid_high			= buf[54];
	ws.alarm_outhumid_low			= buf[55];
	ws.alarm_outtemp_high			= FIX_SIGN((buf[56] & 0xff) | (buf[57] << 8));
	ws.alarm_outtemp_low			= FIX_SIGN((buf[58] & 0xff) | (buf[59] << 8));
	ws.alarm_windchill_high			= FIX_SIGN((buf[60] & 0xff) | (buf[61] << 8));
	ws.alarm_windchill_low			= FIX_SIGN((buf[62] & 0xff) | (buf[63] << 8));
	ws.alarm_dewpoint_high			= FIX_SIGN((buf[64] & 0xff) | (buf[65] << 8));
	ws.alarm_dewpoint_low			= FIX_SIGN((buf[66] & 0xff) | (buf[67] << 8));
	ws.alarm_abs_pressure_high		= (buf[68] & 0xff) | (buf[69] << 8);
	ws.alarm_abs_pressure_low		= (buf[70] & 0xff) | (buf[71] << 8);
	ws.alarm_rel_pressure_high		= (buf[72] & 0xff) | (buf[73] << 8);
	ws.alarm_rel_pressure_low		= (buf[74] & 0xff) | (buf[75] << 8);
	ws.alarm_avg_wspeed_beaufort	= buf[76];
	ws.alarm_avg_wspeed_ms			= buf[77];
	ws.alarm_gust_wspeed_beaufort 	= buf[79];
	ws.alarm_gust_wspeed_ms			= buf[80];
	ws.alarm_wind_direction			= buf[82];
	ws.alarm_rain_hourly			= (buf[83] & 0xff) | (buf[84] << 8);
	ws.alarm_rain_daily				= (buf[85] & 0xff) | (buf[86] << 8);
	ws.alarm_time					= (buf[87] & 0xff) | (buf[88] << 8);
	ws.max_inhumid					= buf[98];
	ws.min_inhumid					= buf[99];
	ws.max_outhumid					= buf[100];
	ws.min_outhumid					= buf[101];
	ws.max_intemp					= FIX_SIGN((buf[102] & 0xff) | (buf[103] << 8));
	ws.min_intemp					= FIX_SIGN((buf[104] & 0xff) | (buf[105] << 8));
	ws.max_outtemp					= FIX_SIGN((buf[106] & 0xff) | (buf[107] << 8));
	ws.min_outtemp					= FIX_SIGN((buf[108] & 0xff) | (buf[109] << 8));
	ws.max_windchill				= FIX_SIGN((buf[110] & 0xff) | (buf[111] << 8));
	ws.min_windchill				= FIX_SIGN((buf[112] & 0xff) | (buf[113] << 8));
	ws.max_dewpoint					= FIX_SIGN((buf[114] & 0xff) | (buf[115] << 8));
	ws.min_dewpoint					= FIX_SIGN((buf[116] & 0xff) | (buf[117] << 8));
	ws.max_abs_pressure				= (buf[118] & 0xff) | (buf[119] << 8);
	ws.min_abs_pressure				= (buf[120] & 0xff) | (buf[121] << 8);
	ws.max_rel_pressure				= (buf[122] & 0xff) | (buf[123] << 8);
	ws.min_rel_pressure				= (buf[124] & 0xff) | (buf[125] << 8);
	ws.max_avg_wspeed				= (buf[126] & 0xff) | (buf[127] << 8);
	ws.max_gust_wspeed				= (buf[128] & 0xff) | (buf[129] << 8);
	ws.max_rain_hourly				= (buf[130] & 0xff) | (buf[131] << 8);
	ws.max_rain_daily				= (buf[132] & 0xff) | (buf[133] << 8);
	ws.max_rain_weekly				= (buf[134] & 0xff) | (buf[135] << 8);
	ws.max_rain_monthly				= (buf[136] & 0xff) | (buf[137] << 8);
	ws.max_rain_total				= (buf[138] & 0xff) | (buf[139] << 8);
	memcpy(&ws.max_inhumid_date, &buf[141], sizeof(ws.max_inhumid_date));
	memcpy(&ws.min_inhumid_date, &buf[146], sizeof(ws.min_inhumid_date));
	memcpy(&ws.max_outhumid_date, &buf[151], sizeof(ws.max_outhumid_date));
	memcpy(&ws.min_outhumid_date, &buf[156], sizeof(ws.min_outhumid_date));
	memcpy(&ws.max_intemp_date, &buf[161], sizeof(ws.max_intemp_date));
	memcpy(&ws.min_intemp_date, &buf[166], sizeof(ws.min_intemp_date));
	memcpy(&ws.max_outtemp_date, &buf[171], sizeof(ws.max_outtemp_date));
	memcpy(&ws.min_outtemp_date, &buf[176], sizeof(ws.min_outtemp_date));
	memcpy(&ws.max_windchill_date, &buf[181], sizeof(ws.max_windchill_date));
	memcpy(&ws.min_windchill_date, &buf[186], sizeof(ws.min_windchill_date));
	memcpy(&ws.max_dewpoint_date, &buf[191], sizeof(ws.max_dewpoint_date));
	memcpy(&ws.min_dewpoint_date, &buf[196], sizeof(ws.min_dewpoint_date));
	memcpy(&ws.max_abs_pressure_date, &buf[201], sizeof(ws.max_abs_pressure_date));
	memcpy(&ws.min_abs_pressure_date, &buf[206], sizeof(ws.min_abs_pressure_date));
	memcpy(&ws.max_rel_pressure_date, &buf[211], sizeof(ws.max_rel_pressure_date));
	memcpy(&ws.min_rel_pressure_date, &buf[216], sizeof(ws.min_rel_pressure_date));
	memcpy(&ws.max_avg_wspeed_date, &buf[221], sizeof(ws.max_avg_wspeed_date));
	memcpy(&ws.max_gust_wspeed_date, &buf[226], sizeof(ws.max_gust_wspeed_date));
	memcpy(&ws.max_rain_hourly_date, &buf[231], sizeof(ws.max_rain_hourly_date));
	memcpy(&ws.max_rain_daily_date, &buf[236], sizeof(ws.max_rain_daily_date));
	memcpy(&ws.max_rain_weekly_date, &buf[241], sizeof(ws.max_rain_weekly_date));
	memcpy(&ws.max_rain_monthly_date, &buf[246], sizeof(ws.max_rain_monthly_date));
	memcpy(&ws.max_rain_total_date, &buf[251], sizeof(ws.max_rain_total_date));

	return ws;
}

//
// Sets a single byte at a specified offset in the fixed weather settings chunk.
//
int set_weather_setting_byte(struct usb_dev_handle *h, unsigned int offset, char data)
{
	assert(offset < WEATHER_SETTINGS_CHUNK_SIZE);
	return write_weather_1(h, offset, data);
}

//
// Writes a notify byte so the weather station knows a setting has changed.
//
int notify_weather_setting_change(struct usb_dev_handle *h)
{
	// Write 0xAA to address 0x1a to indicate a change of settings.
	return set_weather_setting_byte(h, 0x1a, 0xaa);
}

//
// Sets a weather setting at a given offset in the weather settings chunk.
//
int set_weather_setting(struct usb_dev_handle *h, unsigned int offset, char *data, unsigned int len)
{
	unsigned int i;
	for (i = 0; i < len; i++)
	{
		if (set_weather_setting_byte(h, offset, data[i]) != 0)
		{
			return -1;
		}
	}

	notify_weather_setting_change(h);
	return 0;
}

// TODO: Remake this to weather_settings_t structure and write all changes in that to the device.
int set_weather_settings_bulk(struct usb_dev_handle *h, unsigned int change_offset, char *data, unsigned int len)
{
	unsigned offset;
	//unsigned int i;
	char buf[WEATHER_SETTINGS_CHUNK_SIZE];

	// Make sure we're not trying to write outside the settings buffer.
	assert((change_offset + len) < WEATHER_SETTINGS_CHUNK_SIZE);

	get_settings_block_raw(h, buf, sizeof(buf));

	// Change the settings.
	memcpy(&buf[change_offset], data, len);

	// Send back the settings in 3 32-bit chunks.
	for (offset = 0; offset < (32 * 3); offset += 32)
	{		
		write_weather_32(h, offset, &buf[offset]);

		if (read_weather_ack(h) != 0)
		{
			return -1;
		}
	}

	notify_weather_setting_change(h);

	return 0;
}

int set_timezone(struct usb_dev_handle *h, signed char timezone)
{
	return set_weather_setting(h, 24, (char *)&timezone, 1);
}

int set_delay(struct usb_dev_handle *h, unsigned char delay)
{
	return set_weather_setting(h, 16, (char *)&delay, 1);
}

//
// Gets weather data from a memory address in the history.
//
weather_data_t get_history_chunk(struct usb_dev_handle *h, weather_settings_t *ws, unsigned short history_pos)
{
	static unsigned short prev_history_pos = -1;
	static char buf[32];
	char *b;
	int trycount = 0;
	weather_data_t d;

	// Try reading the chunk 3 times.
	do
	{
		// We read two chunks at a time. Since we always read 32 bytes at a time. 1 chunk = 16 bytes.
		if (!read_weather_address(h, history_pos, buf))
		{
			print_bytes(2, buf, 32);
			break;
		}
		
		print_bytes(2, buf, 32);
		
		fprintf(stderr, "Failed to read history chunk. Try %d of %d\n", trycount, NUM_TRIES);
		
		trycount++;		
	} while (trycount < NUM_TRIES);

	b = buf;

	memset(&d, 0, sizeof(d));

	d.delay  			= b[0];
	d.in_humidity	 	= b[1];
	d.in_temp 			= FIX_SIGN((b[2] & 0xff) | (b[3] << 8));
	d.out_humidity		= b[4];
	d.out_temp			= FIX_SIGN((b[5] & 0xff) | (b[6] << 8));
	d.abs_pressure		= (b[7] & 0xff) | (b[8] << 8);
	d.avg_wind_lowbyte	= b[9];
	d.gust_wind_lowbyte = b[10];
	d.wind_highbyte		= b[11];
	d.wind_direction	= b[12];
	d.total_rain		= (b[13] & 0xff) | (b[14] << 8);
	d.status			= b[15];

	memcpy(&d.raw_data, b, sizeof(d.raw_data));

	prev_history_pos = history_pos;

	return d;
}

void get_weather_data(struct usb_dev_handle *h)
{
	int i = 0;
	int history_address;
	weather_item_t history[HISTORY_MAX];
	weather_settings_t ws;
	unsigned int items_to_read = 0;

	// Try 3 times until the magic number is correct, otherwise abort.
	do
	{
		if (i >= NUM_TRIES)
		{
			fprintf(stderr, "Incorrect magic number!\n");
			return;
		}

		debug_printf(1, "Start Reading status block\n");
		ws = get_settings_block(h);
		debug_printf(1, "End Reading status block\n\n");

		i++;
	} while ((ws.magic_number[0] != 0x55) && (ws.magic_number[1] != 0xaa));

	if (program_settings.show_status)
	{
		print_status(&ws);
	}

	if (program_settings.show_alarms)
	{
		print_alarms(&ws);
	}

	if (program_settings.show_settings)
	{
		print_settings(&ws);
	}

	if (program_settings.show_maxmin)
	{
		print_maxmin(&ws);
	}

	items_to_read = (program_settings.count == 0) ? ws.data_count : program_settings.count;

	// Read all events.
	// Loop through the events in reverse order, starting with the last recorded one
	// and calculate the timestamp for each event. We only know the current
	// weather station date/time + the delay in minutes between each event, so
	// we can only get the timestamps by doing it this way.
	{
		// Convert the weather station date from a BCD date to unix date.
		time_t station_date = bcd_to_unix_date(parse_bcd_date(ws.datetime));
		unsigned int total_seconds = 0;
		unsigned int seconds = 0;
		unsigned int history_begin = (ws.current_pos + HISTORY_CHUNK_SIZE);
		int history_index;
		unsigned int j;

		memset(&history, 0, sizeof(history));

		debug_printf(2, "Start reading history blocks\n");
		debug_printf(2, "Index\tTimestamp\t\tDelay\n");

		for (history_address = ws.current_pos, i = (HISTORY_MAX - 1), j = 0;
			(j < items_to_read);
			history_address -= HISTORY_CHUNK_SIZE, i--, j++)
		{
			// The buffer is full so it acts as a circular buffer, so we need to
			// wrap to the end to get the next item.
			if (history_address < HISTORY_START)
			{
				history_address = HISTORY_END - (HISTORY_START - history_address);
			}

			// Calculate the index we're at in the history, from 0-4080.
			if (ws.data_count < HISTORY_MAX)
			{
				history_index = 1 + (history_address - HISTORY_START) / HISTORY_CHUNK_SIZE; // Normal.
			}
			else
			{
				history_index = 1 + ((history_address - HISTORY_START) + (HISTORY_END - history_begin)) / HISTORY_CHUNK_SIZE; // Circular buffer.
			}

			// Read history chunk.
			history[i].history_index = history_index;
			history[i].address = history_address;
			history[i].data = get_history_chunk(h, &ws, history_address);

			// Calculate timestamp.
			history[i].timestamp = (time_t)(station_date - total_seconds);
			seconds = history[i].data.delay * 60;
			total_seconds += seconds;

			// Debug print.	
			debug_printf(2, "DEBUG: Seconds before current event = %d\n", total_seconds);
			debug_printf(2, "DEBUG: Temp = %2.1fC\n", history[i].data.in_temp * 0.1f);
			debug_printf(2, "DEBUG: %d,\t%s,\t%u minutes\n",
				i,
				get_timestamp(history[i].timestamp),
				history[i].data.delay);
		
		}

		debug_printf(1, "End reading history blocks\n\n");
	}

	if (program_settings.show_summary)
	{
		debug_printf(1, "Show summary:\n");
		print_summary(&ws, &history[HISTORY_MAX - 1]);
	}

	if (program_settings.show_formatted)
	{
		debug_printf(1, "Show formatted:\n");

		for (i = (HISTORY_MAX - items_to_read); i < HISTORY_MAX; i++)
		{
			print_history_item_formatstring(h, &ws, history, i, program_settings.format_str);
		}
	}
	// Prints output in the Easyweather.dat format.
	else if (program_settings.show_easyweather)
	{
		// Output chronologically.
		for (i = (HISTORY_MAX - items_to_read); i < HISTORY_MAX; i++)
		{
			print_history_item(&history[i], i);
		}
	}
}

//
// Resets the weather station memory.
//
void reset_memory(struct usb_dev_handle *h)
{
	// TODO: Use the 32 byte write instead here.
	// Set data count to zero.
	write_weather_1(h, 27, 0x0);
	write_weather_1(h, 28, 0x0);
	
	// Reset the address to 256 (0x100). 
	write_weather_1(h, 30, 0x00);
	write_weather_1(h, 31, 0x1);

	// Finally tell the station the data has been updated.
	write_weather_1(h, 26, 0xAA);
}

//
// Sets weather display settings.
//
void set_weather_data(struct usb_dev_handle *h)
{
	if (program_settings.set_timezone)
	{
		if (!set_timezone(h, program_settings.timezone))
		{
			printf("Timezone set to CET%s%d\n", ((program_settings.timezone >= 0) ? "+" : "-"), program_settings.timezone);
		}
		else
		{
			fprintf(stderr, "Failed to update timezone.\n");
		}
	}

	if (program_settings.set_delay)
	{
		if (!set_delay(h, program_settings.delay))
		{
			printf("Updating delay set to %u minutes.\n", program_settings.delay);
			printf("!!! NOTICE that using --quickrain now will produce inaccurate !!!\n");
			printf("!!! rain data unless you reset the station memory, due to the !!!\n");
			printf("!!! fact that it assumes the delay between each weather       !!!\n");
			printf("!!! reading is the same throughout the entire history.        !!!\n");
		}
		else
		{
			fprintf(stderr, "Failed to update delay.\n");
		}
	}
	
	if (program_settings.writebyte)
	{
		fprintf(stderr, "About to write %d (0x%x) to address %d (0x%x)\n", 
			program_settings.byte, program_settings.byte,
			 program_settings.addr, program_settings.addr); 
		
		fprintf(stderr, "Are you sure you want to write to the weather station memory? (Y/N): ");
		
		if (prompt_user() != 'Y')
		{
			return;
		}
		
		if (write_weather_1(h, program_settings.addr, program_settings.byte))
		{
			fprintf(stderr, "Failed to write to the weather station\n");
			return;
		}
		
		printf("Wrote to the weather station successfully\n");
	}
}

int dump_memory(struct usb_dev_handle *h)
{
	FILE *f; 
	
	if (file_exists(program_settings.dumpfile))
	{
		fprintf(stderr, "The file \"%s\" already exists. Overwrite? (Y/N): ", program_settings.dumpfile);
		
		if (prompt_user() != 'Y')
		{
			return -1;
		}
	}
	
	if (!(f = fopen(program_settings.dumpfile, "w")))
	{
		fprintf(stderr, "Failed to open \"%s\"", program_settings.dumpfile);
		return -1;
	}
	else
	{
		// Dump the memory to file.
		unsigned short offset;
		int trycount;
		char buf[32];
		
		for (offset = 0; offset < (HISTORY_END - 32); offset += 32)
		{
			trycount = 0;
			memset(buf, 0, sizeof(buf));
			
			while (read_weather_address(h, offset, buf) && (trycount < NUM_TRIES))
			{
				fprintf(stderr, "Failed to read from weather memory offset %d (0x%x). Try %d of %d\n", 
						offset, offset, trycount + 1, NUM_TRIES);
				
				trycount++;
			}
			
			fwrite(buf, 1, sizeof(buf), f);
			
			print_bytes(2, buf, 32);
		}
	}
	
	fclose(f);
	
	return 0;
}

int read_arguments(int argc, char **argv)
{
	int c;
	memset(&program_settings, 0, sizeof(program_settings));

	// Set defaults.
	program_settings.count = 1;
	program_settings.mode = get_mode;
	program_settings.product_id = PRODUCT_ID;
	program_settings.vendor_id = VENDOR_ID;

	while (1)
	{
		static struct option long_options[] =
		{
			{"all", no_argument,			0, 'a'},
			{"status", no_argument,			&program_settings.show_status, 1},
			{"alarms", no_argument,			&program_settings.show_alarms, 1},
			{"settings", no_argument,		&program_settings.show_settings, 1},
			{"maxmin", no_argument,			&program_settings.show_maxmin, 1},
			{"settings", no_argument,		&program_settings.show_settings, 1},
			{"easyweather", no_argument,	&program_settings.show_easyweather, 1},
			{"summary", no_argument,		&program_settings.show_summary, 1},
			{"quickrain", no_argument,		&program_settings.quickrain, 1},
			{"count", required_argument,		0, 'c'},
			{"timezone", required_argument, 	0, 't'},
			{"delay", required_argument, 		0, 'd'},
			{"help", required_argument,			0, 'h'},
			{"format", required_argument,		0, 0},
			{"formatlist", no_argument,			0, 0},
			{"altitude", required_argument,		0, 'A'},
			{"productid", required_argument,	0, 0},
			{"vendorid", required_argument,		0, 0},
			{"dumpmem", required_argument,		0, 0},
			{"infile", required_argument,		0, 0},
			{"reset", no_argument,				0, 0},
			{"writebyte", required_argument,	0, 0},
			{"address", required_argument,		0, 0},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "aesvmc:ht:d:A:", long_options, &option_index);

		if (c == -1)
		{
			break;
		}

		switch (c)
		{
			case 0:
			{
				if (!strcmp("reset", long_options[option_index].name))
				{
					program_settings.reset = 1;
				}
				else if (!strcmp("address", long_options[option_index].name))
				{
					program_settings.address_is_set = 1;
					sscanf(optarg, "%x", (unsigned int *)&program_settings.addr);
				}
				else if (!strcmp("writebyte", long_options[option_index].name))
				{
					program_settings.writebyte = 1;
					program_settings.mode = set_mode;
					sscanf(optarg, "%x", (unsigned int *)&program_settings.byte);
					program_settings.byte &= 0xff;
				}
				else if (!strcmp("infile", long_options[option_index].name))
				{
					program_settings.from_file = 1;
					strcpy(program_settings.infile, optarg);
				}
				else if (!strcmp("dumpmem", long_options[option_index].name))
				{
					program_settings.mode = dump_mode;
					strcpy(program_settings.dumpfile, optarg);
				}
				else if (!strcmp("format", long_options[option_index].name))
				{
					program_settings.show_formatted = 1;
					strcpy(program_settings.format_str, optarg);
				}
				else if (!strcmp("formatlist", long_options[option_index].name))
				{
					program_settings.show_formatlist = 1;
				}
				else if (!strcmp("productid", long_options[option_index].name))
				{
					sscanf(optarg, "%x", &program_settings.product_id);
				}
				else if (!strcmp("vendorid", long_options[option_index].name))
				{
					sscanf(optarg, "%x", &program_settings.vendor_id);
				}

				break;
			}
			case 'a': program_settings.count = 0;				break;
			case 'v': program_settings.debug++;					break;
			case 'm': program_settings.show_maxmin = 1;			break;
			case 's': program_settings.show_status = 1;			break;
			case 'e': program_settings.show_easyweather = 1;	break;
			case 'A': program_settings.altitude = atoi(optarg); break;
			case 'd':
			{
				program_settings.mode = set_mode;
				program_settings.set_delay = 1;
				program_settings.delay = atoi(optarg);
				break;
			}
			case 't':
			{
				program_settings.mode = set_mode;
				program_settings.set_timezone = 1;
				program_settings.timezone = atoi(optarg);
				break;
			}
			case 'h':
			{
				show_usage(argv[0]);
				exit(0);
			}
			case 'c':
			{
				// The number of history events to fetch.
				program_settings.count = atoi(optarg);
				break;
			}
			default:
			{
				show_usage(argv[0]);
				abort();
				break;
			}
		}
	}

	// Set show summary as default if nothing else has been set to show.
	if (!program_settings.show_status
	&& !program_settings.show_maxmin
	&& !program_settings.show_easyweather
	&& !program_settings.show_formatlist
	&& !program_settings.show_formatted)
	{
		program_settings.show_summary = 1;
	}

	// Turn off quickrain if we're getting all items,
	// then we can be accurate without spending more time :)
	if (program_settings.count == 0)
	{
		program_settings.quickrain = 0;
	}

	debug = program_settings.debug;

	return 0;
}

int main(int argc, char **argv)
{
	if (read_arguments(argc, argv))
	{
		printf("Error reading arguments\n");
		return 1;
	}

	if (program_settings.show_formatlist)
	{
		printf("%%h - Inside humidity (%%).\n");
		printf("%%H - Outside humidity (%%).\n");
		printf("%%t - Inside temperature (Celcius).\n");
		printf("%%T - Outside temperature (Celcius).\n");
		printf("%%C - Outside dew point temperature (Celcius).\n");
		printf("%%c - Outside Wind chill temperature (Celcius).\n");
		printf("%%W - Wind speed (m/s).\n");
		printf("%%G - Gust speed (m/s).\n");
		printf("%%D - Name of wind direction.\n");
		printf("%%d - Wind direction in degrees.\n");
		printf("%%P - Absolute pressure (hPa).\n");
		printf("%%p - Relative pressure (hPa).\n");
		printf("!!! To correctly calculate rain info you need to read  !!!\n");
		printf("!!! at least 24h of events or use --quickrain          !!!\n");
		printf("%%r - Rain 1h (mm/h).\n");
		printf("%%f - Rain 24h (mm/h).\n");
		printf("%%F - Rain 24h (mm).\n");
		printf("%%R - Total rain (mm).\n");
		printf("%%N - Date/time string for the weather reading.\n");
		printf("%%e - Do we have contact with the sensor for this reading? (True/False).\n");
		printf("%%E - Do we have contact with the sensor for this reading? (1/0).\n");
		printf("%%b - Original bytes in hex format containing the data.\n");
		printf("%%a - Address in history.\n");
		printf("%%%% - %% sign\n");
		printf("\\n - Newline.\n");
		printf("\\t - Tab.\n");
		printf("\\r - Carriage return.\n");

		return 0;
	}

	// Open te device.
	devh = open_device();
	signal(SIGTERM, sigterm_handler);
	init_device_descriptors(devh);
	
	if (program_settings.reset)
	{
		fprintf(stderr, "Are you sure you want to reset the weather station memory? (Y/N): ");
		
		if (prompt_user() != 'Y')
		{
			return -1;
		}
		
		reset_memory(devh);
		printf("Memory reset\n");
		
		return 0;
	}
	else if (program_settings.from_file)
	{
		FILE *f;
		
		debug_printf(1, "Reading input from \"%s\"\n", program_settings.infile);
		
		if (program_settings.mode != get_mode)
		{
			fprintf(stderr, "You cannot set any settings or dump the memory while using a dump file as input.\n");
			goto cleanup;
		}

		if (!(f = fopen(program_settings.infile, "r")))
		{
			fprintf(stderr, "Failed to open file \"%s\". ", program_settings.infile);
			perror(NULL);
			goto cleanup;
		}
		
		program_settings.f = f;
	}
	
	switch (program_settings.mode)
	{
		default:
		case get_mode:
		{
			get_weather_data(devh);
			break;
		}
		case set_mode:
		{
			set_weather_data(devh);
			break;
		}
		case dump_mode:
		{
			if (dump_memory(devh))
			{
				fprintf(stderr, "Failed to dump memory.\n");
			}
			break;
		}
	}
	
cleanup:
	close_device(devh);
	if (program_settings.f)
	{
		fclose(program_settings.f);
	}

	return 0;
}

