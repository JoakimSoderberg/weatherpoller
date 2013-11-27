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

#ifndef __WSP_H__
#define __WSP_H__

#ifdef WIN32
#include <lusb0_usb.h>
#else
#include <usb.h>
#endif

#ifndef max
#define max(a, b) ((a)>(b) ? (a) : (b))
#endif

#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_NUM "$Revision: 28 $"

#define VENDOR_ID	0x1941
#define PRODUCT_ID	0x8021

#define ENDPOINT_INTERRUPT_ADDRESS	0x81
#define USB_TIMEOUT 1000

#define HISTORY_MAX 4080
#define HISTORY_CHUNK_SIZE 16
#define WEATHER_SETTINGS_CHUNK_SIZE 256
#define HISTORY_START WEATHER_SETTINGS_CHUNK_SIZE
#define HISTORY_END (HISTORY_START + (HISTORY_MAX * HISTORY_CHUNK_SIZE))

#define NUM_TRIES 3

#define LOST_SENSOR_CONTACT_BIT 6
#define RAIN_COUNTER_OVERFLOW_BIT 7

// The weather station stores signed shorts in a non-standard way.
// Instead of two's compliment, sign-magnitude is used (8-bit is sign).
// http://en.wikipedia.org/wiki/Signed_number_representations
#define MSB(v) 				(((v) >> 8) & 0xff)		// Most significant byte of short.
#define SIGN_BIT(v) 		(MSB(v) >> 7)			// Least Significant bit is sign bit.
#define MAGNITUDE_BITS(v) 	((v) & 0x7fff)
#define FIX_SIGN(v) 		((SIGN_BIT(v) ? -1 : 1) * MAGNITUDE_BITS(v))

#define PRINT_DEBUG(str) { if (debug) printf("DEBUG: "str); }

#define TEMP_VALID(t) ((t) != (0xffff ^ 0x7fff))

typedef enum wsp_mode_s
{
	get_mode,
	set_mode,
	dump_mode
} wsp_mode_t;

typedef struct program_settings_s
{
	int debug;				// Debug-level.
	wsp_mode_t mode;			// Mode.
	unsigned int count;			// The number of history entires to fetch. 0 = All.
	int show_status;			// 0 or 1. Show current status of device.
	int show_alarms;			// 0 or 1. Shows the current alarms set on the device.
	int show_settings;			// 0 or 1. Shows the unit/display settings for the device.
	int show_current;			// 0 or 1. Only show the latest values read from the device.
	int show_maxmin;			// 0 or 1. Show max min values.
	int show_easyweather;		// 0 or 1. Outputs the history data in the "EasyWeather.dat" format. "count" decides how many.
	int show_summary;			// 0 or 1. Shows a summary of the current weather.
	int set_timezone;			// 0 or 1. Should a new timezone be set?
	signed char timezone;		// -12 to 12. The new timezone to be set.
	int set_delay;				// 0 or 1. Should a new delay be set?.
	signed char delay;			// 0 to 255. The new delay between weather updates.
	int altitude;				// Altitude over sea level.
	int show_formatted;			// Print formatted string.
	char format_str[2048];		// The format string to be used.
	int show_formatlist;		// 0 or 1. Shows the available format variables.
	int product_id;				// The product id used to search for the usb device.
	int vendor_id;				// The vendor id used to search for the usb device.
	int quickrain;				// 0 or 1. Use quick rain calculations.
	int dumpmem;				// 0 or 1. Dump memory to a file.
	char dumpfile[2048];		// The path to the dumpfile.
	int from_file;				// 0 or 1. Read from a dump file instead of from the weather station.
	char infile[2048];			// The path to the file to read from instead of the weather station memory.
	FILE *f;					// File handle for the input file.
	int reset;					// 0 or 1. Reset weather station memory.
	int writebyte;				// 0 or 1. Write a specified byte to memory.
	unsigned char byte;			// The byte to write in writebyte mode.
	unsigned short addr;		// Address to write to.
	int address_is_set;			// 0 or 1. Is the address set or not.
} program_settings_t;

extern program_settings_t program_settings;
extern unsigned int debug;

//
// Based on http://www.jim-easterbrook.me.uk/weather/mm/
//
typedef struct weather_settings_s
{
	char			magic_number[2];
	unsigned char	read_period;		// Minutes between each stored reading.

	unsigned char	unit_settings1;		// bit 0: indoor temperature: 0 = °C, 1 = °F
										// bit 1: outdoor temperature: 0 = °C, 1 = °F
										// bit 2: rain: 0 = mm, 1 = inch
										// bit 5: pressure: 1 = hPa
										// bit 6: pressure: 1 = inHg
										// bit 7: pressure: 1 = mmHg

	unsigned char	unit_settings2;		// bit 0: wind speed: 1 = m/s
										// bit 1: wind speed: 1 = km/h
										// bit 2: wind speed: 1 = knot
										// bit 3: wind speed: 1 = m/h
										// bit 4: wind speed: 1 = bft

	unsigned char	display_options1;	// bit 0: pressure: 0 = absolute, 1 = relative
										// bit 1: wind speed: 0 = average, 1 = gust
										// bit 2: time: 0 = 24 hour, 1 = 12 hour
										// bit 3: date: 0 = day-month-year, 1 = month-day-year
										// bit 4: time scale(?): 0 = 12 hour, 1 = 24 hour
										// bit 5: date: 1 = show year year
										// bit 6: date: 1 = show day name
										// bit 7: date: 1 = alarm time

	unsigned char	display_options2;	// bit 0: outdoor temperature: 1 = temperature
										// bit 1: outdoor temperature: 1 = wind chill
										// bit 2: outdoor temperature: 1 = dew point
										// bit 3: rain: 1 = hour
										// bit 4: rain: 1 = day
										// bit 5: rain: 1 = week
										// bit 6: rain: 1 = month
										// bit 7: rain: 1 = total

	unsigned char	alarm_enable1;		// bit 1: time
										// bit 2: wind direction
										// bit 4: indoor humidity low
										// bit 5: indoor humidity high
										// bit 6: outdoor humidity low
										// bit 7: outdoor humidity high

	unsigned char	alarm_enable2;		// bit 0: wind average
										// bit 1: wind gust
										// bit 2: rain hourly
										// bit 3: rain daily
										// bit 4: absolute pressure low
										// bit 5: absolute pressure high
										// bit 6: relative pressure low
										// bit 7: relative pressure high

	unsigned char	alarm_enable3;		// bit 0: indoor temperature low
										// bit 1: indoor temperature high
										// bit 2: outdoor temperature low
										// bit 3: outdoor temperature high
										// bit 4: wind chill low
										// bit 5: wind chill high
										// bit 6: dew point low
										// bit 7: dew point high

	char timezone;						// Hours offset from Central European Time, so in the UK this should be set to -1.
										// In stations without a radio controlled clock this is always zero.

	char data_refreshed;				// Computer writes 0xAA to indicate a change of settings. Weather station clears value to acknowledge.
	unsigned short data_count;			// Number of stored readings. Starts at zero, rises to 4080.

	unsigned short current_pos;			// Address of the stored reading currently being created.
										// Starts at 256, rises to 65520 in steps of 16, then loops
										// back to 256. The data at this address is updated every 48 seconds or so,
										// until the read period is reached. Then the address is incremented and
										// the next record becomes current.
										// Substract 256 and divide by 16 to get the number of saved history entries.

	unsigned short relative_pressure;	// Current relative (sea level) atmospheric pressure, multiply by 0.1 to get hPa.
	unsigned short absolute_pressure;	// Current absolute atmospheric pressure, multiply by 0.1 to get hPa.
	unsigned char unknown[7];			// Usually all zero, but have also seen 0x4A7600F724030E. If you have something different, let me know!
	unsigned char datetime[5];			// Date-time values are stored as year (last two digits), month, day, hour and minute in binary coded decimal, two digits per byte.
	unsigned char alarm_inhumid_high;	// alarm, indoor humidity, high.
	unsigned char alarm_inhumid_low;	// alarm, indoor humidity, low.
	short alarm_intemp_high;			// alarm, indoor temperature, high. Multiply by 0.1 to get °C.
	short alarm_intemp_low;				// alarm, indoor temperature, low. Multiply by 0.1 to get °C.
	unsigned char alarm_outhumid_high;	// alarm, outdoor humidity, high.
	unsigned char alarm_outhumid_low;	// alarm, outdoor humidity, low
	short alarm_outtemp_high;			// alarm, outdoor temperature, high. Multiply by 0.1 to get °C.
	short alarm_outtemp_low;			// alarm, outdoor temperature, low. Multiply by 0.1 to get °C.
	short alarm_windchill_high;			// alarm, wind chill, high. Multiply by 0.1 to get °C.
	short alarm_windchill_low;			// alarm, wind chill, low. Multiply by 0.1 to get °C.
	short alarm_dewpoint_high;			// alarm, dew point, high. Multiply by 0.1 to get °C.
	short alarm_dewpoint_low;			// alarm, dew point, low. Multiply by 0.1 to get °C.
	short alarm_abs_pressure_high;		// alarm, absolute pressure, high. Multiply by 0.1 to get hPa.
	short alarm_abs_pressure_low;		// alarm, absolute pressure, low. Multiply by 0.1 to get hPa.
	short alarm_rel_pressure_high;		// alarm, relative pressure, high. Multiply by 0.1 to get hPa.
	short alarm_rel_pressure_low;		// alarm, relative pressure, low. Multiply by 0.1 to get hPa.
	unsigned char alarm_avg_wspeed_beaufort; // alarm, average wind speed, Beaufort.
	unsigned char alarm_avg_wspeed_ms;	// alarm, average wind speed, m/s. Multiply by 0.1 to get m/s.
	unsigned char alarm_gust_wspeed_beaufort; // alarm, gust wind speed, Beaufort.
	unsigned char alarm_gust_wspeed_ms;	// alarm, gust wind speed, m/s. Multiply by 0.1 to get m/s.
	unsigned char alarm_wind_direction;	// alarm, wind direction. Multiply by 22.5 to get ° from north.
	unsigned short alarm_rain_hourly;	// alarm, rain, hourly. Multiply by 0.3 to get mm.
	unsigned short alarm_rain_daily;	// alarm, rain, daily. Multiply by 0.3 to get mm.
	unsigned short alarm_time;			// Hour & Time. BCD (http://en.wikipedia.org/wiki/Binary-coded_decimal)
	unsigned char max_inhumid;			// maximum, indoor humidity, value.
	unsigned char min_inhumid;			// minimum, indoor humidity, value.
	unsigned char max_outhumid;			// maximum, outdoor humidity, value.
	unsigned char min_outhumid;			// minimum, outdoor humidity, value.
	short max_intemp;					// maximum, indoor temperature, value. Multiply by 0.1 to get °C.
	short min_intemp;					// minimum, indoor temperature, value. Multiply by 0.1 to get °C.
	short max_outtemp;					// maximum, outdoor temperature, value. Multiply by 0.1 to get °C.
	short min_outtemp;					// minimum, outdoor temperature, value. Multiply by 0.1 to get °C.
	short max_windchill;				// maximum, wind chill, value. Multiply by 0.1 to get °C.
	short min_windchill;				// minimum, wind chill, value. Multiply by 0.1 to get °C.
	short max_dewpoint;					// maximum, dew point, value. Multiply by 0.1 to get °C.
	short min_dewpoint;					// minimum, dew point, value. Multiply by 0.1 to get °C.
	unsigned short max_abs_pressure;	// maximum, absolute pressure, value. Multiply by 0.1 to get hPa.
	unsigned short min_abs_pressure;	// minimum, absolute pressure, value. Multiply by 0.1 to get hPa.
	unsigned short max_rel_pressure;	// maximum, relative pressure, value. Multiply by 0.1 to get hPa.
	unsigned short min_rel_pressure;	// minimum, relative pressure, value. Multiply by 0.1 to get hPa.
	unsigned short max_avg_wspeed;		// maximum, average wind speed, value. Multiply by 0.1 to get m/s.
	unsigned short max_gust_wspeed;		// maximum, gust wind speed, value. Multiply by 0.1 to get m/s.
	unsigned short max_rain_hourly;		// maximum, rain hourly, value. Multiply by 0.3 to get mm.
	unsigned short max_rain_daily;		// maximum, rain daily, value. Multiply by 0.3 to get mm.
	unsigned short max_rain_weekly;		// maximum, rain weekly, value. Multiply by 0.3 to get mm.
	unsigned short max_rain_monthly;	// maximum, rain monthly, value. Multiply by 0.3 to get mm.
	unsigned short max_rain_total;		// maximum, rain total, value. Multiply by 0.3 to get mm.
	unsigned char max_inhumid_date[5];	// maximum, indoor humidity, when. Datetime in BCD-format.
	unsigned char min_inhumid_date[5];	// minimum, indoor humidity, when. Datetime in BCD-format.
	unsigned char max_outhumid_date[5];	// maximum, outdoor humidity, when. Datetime in BCD-format.
	unsigned char min_outhumid_date[5];	// minimum, outdoor humidity, when. Datetime in BCD-format.
	unsigned char max_intemp_date[5];	// maximum, indoor temperature, when. Datetime in BCD-format.
	unsigned char min_intemp_date[5];	// minimum, indoor temperature, when. Datetime in BCD-format.
	unsigned char max_outtemp_date[5];	// maximum, outdoor temperature, when. Datetime in BCD-format.
	unsigned char min_outtemp_date[5];	// minimum, outdoor temperature, when. Datetime in BCD-format.
	unsigned char max_windchill_date[5];// maximum, wind chill, when. Datetime in BCD-format.
	unsigned char min_windchill_date[5];// minimum, wind chill, when. Datetime in BCD-format.
	unsigned char max_dewpoint_date[5]; // maximum, dew point, when. Datetime in BCD-format.
	unsigned char min_dewpoint_date[5]; // minimum, dew point, when. Datetime in BCD-format.
	unsigned char max_abs_pressure_date[5]; // maximum, absolute pressure, when. Datetime in BCD-format.
	unsigned char min_abs_pressure_date[5]; // minimum, absolute pressure, when. Datetime in BCD-format.
	unsigned char max_rel_pressure_date[5]; // maximum, relative pressure, when. Datetime in BCD-format.
	unsigned char min_rel_pressure_date[5]; // minimum, relative pressure, when. Datetime in BCD-format.
	unsigned char max_avg_wspeed_date[5]; // maximum, average wind speed, when. Datetime in BCD-format.
	unsigned char max_gust_wspeed_date[5]; // maximum, gust wind speed, when. Datetime in BCD-format.
	unsigned char max_rain_hourly_date[5]; // maximum, rain hourly, when. Datetime in BCD-format.
	unsigned char max_rain_daily_date[5]; // maximum, rain daily, when. Datetime in BCD-format.
	unsigned char max_rain_weekly_date[5]; // maximum, rain weekly, when. Datetime in BCD-format.
	unsigned char max_rain_monthly_date[5]; // maximum, rain monthly, when. Datetime in BCD-format.
	unsigned char max_rain_total_date[5]; // maximum, rain total, when. Datetime in BCD-format.
} weather_settings_t;

typedef struct weather_data_s
{
	unsigned char delay;			// Minutes since last stored reading.
	unsigned char in_humidity;		// Indoor humidity.
	short in_temp;					// Indoor temperature. Multiply by 0.1 to get °C.
	unsigned char out_humidity;		// Outdoor humidity.
	short out_temp;					// Outdoor temperature. Multiply by 0.1 to get °C.
	unsigned short abs_pressure;	// Absolute pressure. Multiply by 0.1 to get hPa.
	unsigned char avg_wind_lowbyte;	// Average wind speed, low bits. Multiply by 0.1 to get m/s. (I've read elsewhere that the factor is 0.38. I don't know if this is correct.)
	unsigned char gust_wind_lowbyte;// Gust wind speed, low bits. Multiply by 0.1 to get m/s. (I've read elsewhere that the factor is 0.38. I don't know if this is correct.)
	unsigned char wind_highbyte;	// Wind speed, high bits. Lower 4 bits are the average wind speed high bits, upper 4 bits are the gust wind speed high bits.
	unsigned char wind_direction;	// Multiply by 22.5 to get ° from north. If bit 7 is 1, no valid wind direction.
	unsigned short total_rain;		// Total rain. Multiply by 0.3 to get mm.
	unsigned char status;			// Bits.
									// 7th bit (i.e. bit 6, 64) indicates loss of contact with sensors.
									// 8th bit (i.e. bit 7, 128) indicates rain counter overflow.
	unsigned char raw_data[16];
} weather_data_t;

typedef struct weather_item_s
{
	weather_data_t data;
	int history_index;
	time_t timestamp;
	unsigned int address;
} weather_item_t;

weather_data_t get_history_chunk(struct usb_dev_handle *h, weather_settings_t *ws, unsigned short history_pos);

#endif // __WSP_H__
