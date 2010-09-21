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
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <ctype.h>
#include <usb.h>
#include <time.h>
#include <math.h>
#include <getopt.h>

#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_NUM "$Revision$"

#define VENDOR_ID	0x1941
#define PRODUCT_ID	0x8021

#define ENDPOINT_INTERRUPT_ADDRESS	0x81
#define USB_TIMEOUT 1000

#define WEATHER_STATION_HISTORY_MAX 4080
#define HISTORY_CHUNK_SIZE 16
#define WEATHER_SETTINGS_CHUNK_SIZE 256

#define PRINT_DEBUG(str) { if (debug) printf("DEBUG: "str); }

typedef unsigned char byte;

struct usb_dev_handle *devh;
int debug = 0;

typedef enum mode_s
{
	get_mode,
	set_mode
} mode_t;

typedef struct program_settings_s
{
	int debug;					// Debug-level.
	mode_t mode;				// Mode.
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
	float altitude;				// Altitude over sea level.
	int show_formatted;			// Print formatted string.
	char format_str[2048];		// The format string to be used.
	int show_formatlist;		// 0 or 1. Shows the available format variables.
} program_settings_t;

static program_settings_t program_settings;

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
	unsigned char wind_direction;	// Multiply by 22.5 to get ° from north.
	unsigned short total_rain;		// Total rain. Multiply by 0.3 to get mm.
	unsigned char status;			// Bits. 
									// 7th bit (i.e. bit 6, 64) indicates loss of contact with sensors. 
									// 8th bit (i.e. bit 7, 128) indicates rain counter overflow.
	unsigned char raw_data[16];
} weather_data_t;

//
// Shows usage.
//
void show_usage(char *program_name)
{
	printf("Weather Station Poller v%u.%u %s\n", MAJOR_VERSION, MINOR_VERSION, BUILD_NUM);
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
	printf("  --format <string>     Writes the output in the given format.\n");
	printf("  --formatlist          Lists available format string variables.\n");
	printf("  --summary             Shows a small summary of the last recorded weather.\n");
	printf("  -h, --help            Shows this help text.\n");
	printf("\n");
}

//
// Finds the device based on vendor and product id.
//
struct usb_device *find_device(int vendor, int product)
{
    struct usb_bus *bus;
	struct usb_device *dev;
    
    for (bus = usb_get_busses(); bus; bus = bus->next) 
	{		
		for (dev = bus->devices; dev; dev = dev->next) 
		{
			if (dev->descriptor.idVendor == vendor
			&& dev->descriptor.idProduct == product)
			{
				return dev;
			}
		}
    }
    return NULL;
}

//
// Closes the connection to the USB device.
//
void close_device(struct usb_dev_handle *h)
{
	int ret = usb_release_interface(h, 0);
	
	if (ret != 0) 
		fprintf(stderr, "Could not release interface: %d\n", ret);
    
	ret = usb_close(h);
    
	if (ret != 0) 
		fprintf(stderr, "Error closing interface: %d\n", ret);
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

//
// Opens the USB device.
//
struct usb_dev_handle *open_device() 
{
	int ret;
	struct usb_dev_handle *h;
    struct usb_device *dev;
	char buf[1024];

    usb_init();
    usb_find_busses();
    usb_find_devices();
	
    dev = find_device(VENDOR_ID, PRODUCT_ID);
    assert(dev);
    h = usb_open(dev);
    assert(h);
    signal(SIGTERM, sigterm_handler);
    ret = usb_get_driver_np(h, 0, buf, sizeof(buf));

    if (ret == 0) 
	{
		//fprintf(stderr, "Interface 0 already claimed by driver \"%s\", attempting to detach it.\n", buf);
		ret = usb_detach_kernel_driver_np(h, 0);
		//printf("usb_detach_kernel_driver_np returned %d\n", ret);
    }
	
    ret = usb_claim_interface(h, 0);

    if (ret != 0) 
	{
		fprintf(stderr, "Could not open usb device, errorcode - %d\n", ret);
		exit(1);
    }
	
    ret = usb_set_altinterface(h, 0);
    assert(ret >= 0);
	
	return h;
}

//
// Inits the USB descriptors.
//
void init_device_descriptors() 
{	
	char buf[1024];
	int ret = 0;
	
	ret = usb_get_descriptor(devh, USB_DT_DEVICE, 0, buf, sizeof(buf));
	ret = usb_get_descriptor(devh, USB_DT_CONFIG, 0, buf, sizeof(buf));
	ret = usb_release_interface(devh, 0);

	if (ret != 0)
		fprintf(stderr, "Failed to release interface before set_configuration: %d\n", ret);
	
	ret = usb_set_configuration(devh, 1);
	ret = usb_claim_interface(devh, 0);
	
	if (ret != 0) 
		fprintf(stderr, "Claim after set_configuration failed with error %d\n", ret);
	
	ret = usb_set_altinterface(devh, 0);
	ret = usb_control_msg(devh, USB_TYPE_CLASS + USB_RECIP_INTERFACE, 0xa, 0, 0, NULL, 0, USB_TIMEOUT);
	ret = usb_get_descriptor(devh, USB_DT_REPORT, 0, buf, sizeof(buf));
}

//
// Prints bytes for debug purposes.
//
void print_bytes(char *bytes, unsigned int len) 
{
    if ((program_settings.debug == 2) && (len > 0)) 
	{
		int i;
		
		for (i=0; i < len; i++) 
		{
			printf("%02x ", (int)((unsigned char)bytes[i]));
		}
    }
	
	printf("\n");
}

//
// Sends a USB message to the device from a given buffer.
//
void send_usb_msgbuf(struct usb_dev_handle *h, char *msg, int msgsize)
{
	int bytes_written = 0;
	
	if (debug == 2) 
	{
		printf("--> ");
		print_bytes(msg, msgsize);
	}
	
	bytes_written = usb_control_msg(h, USB_TYPE_CLASS + USB_RECIP_INTERFACE,
									9, 0x200, 0, msg, msgsize, USB_TIMEOUT);
	assert(bytes_written == msgsize);
}

//
// Sends 8 bytes of data over usb.
//
void send_usb_msg8(struct usb_dev_handle *h, char b1, char b2, char b3, char b4, char b5, char b6, char b7, char b8)
{
	char buf[8];
	buf[0] = b1;
	buf[1] = b2;
	buf[2] = b3;
	buf[3] = b4;
	buf[4] = b5;
	buf[5] = b6;
	buf[6] = b7;
	buf[7] = b8;
	
	send_usb_msgbuf(h, buf, sizeof(buf));
}

//
// The weather station wants 8 byte control messages. 
// Only 4 bytes are relevant and are repeated twice for each message.
//
void send_weather_msg(struct usb_dev_handle *h, char b1, char b2, char b3, char b4)
{
	send_usb_msg8(h, b1, b2, b3, b4, b1, b2, b3, b4);
}

//
// All data from the weather station is read in 32 byte chunks.
//
int read_weather_msg(struct usb_dev_handle *h, char *buf)
{
	return usb_interrupt_read(h, ENDPOINT_INTERRUPT_ADDRESS, buf, 32, USB_TIMEOUT);
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

void set_wind_direction(char *winddir, const char data)
{
	strcpy(winddir, get_wind_direction(data));
}

typedef struct bcd_date_s
{
	unsigned short year;
	unsigned short month;
	unsigned short day;
	unsigned short hour;
	unsigned short minute;
} bcd_date_t;

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
		send_weather_msg(h, 0xa1, 0x00, offset, 0x20);
		read_weather_msg(h, &buf[offset]);
		
		if (debug == 2)
		{
			print_bytes(&buf[offset], 32);
		}
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
	ws.alarm_intemp_high			= (buf[50] & 0xff) | (buf[51] << 8);
	ws.alarm_intemp_low				= (buf[52] & 0xff) | (buf[53] << 8);
	ws.alarm_outhumid_high			= buf[54];
	ws.alarm_outhumid_low			= buf[55];
	ws.alarm_outtemp_high			= (buf[56] & 0xff) | (buf[57] << 8);
	ws.alarm_outtemp_low			= (buf[58] & 0xff) | (buf[59] << 8);
	ws.alarm_windchill_high			= (buf[60] & 0xff) | (buf[61] << 8);
	ws.alarm_windchill_low			= (buf[62] & 0xff) | (buf[63] << 8);
	ws.alarm_dewpoint_high			= (buf[64] & 0xff) | (buf[65] << 8);
	ws.alarm_dewpoint_low			= (buf[66] & 0xff) | (buf[67] << 8);
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
	ws.max_intemp					= (buf[102] & 0xff) | (buf[103] << 8);
	ws.min_intemp					= (buf[104] & 0xff) | (buf[105] << 8);
	ws.max_outtemp					= (buf[106] & 0xff) | (buf[107] << 8);
	ws.min_outtemp					= (buf[108] & 0xff) | (buf[109] << 8);
	ws.max_windchill				= (buf[110] & 0xff) | (buf[111] << 8);
	ws.min_windchill				= (buf[112] & 0xff) | (buf[113] << 8);
	ws.max_dewpoint					= (buf[114] & 0xff) | (buf[115] << 8);
	ws.min_dewpoint					= (buf[116] & 0xff) | (buf[117] << 8);
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
// Reads weather ack message when writing setting data.
//
int read_weather_ack(struct usb_dev_handle *h)
{
	unsigned int i;
	char buf[8];
	
	read_weather_msg(h, buf);
	
	// The ack should consist of just 0xa5.
	for (i = 0; i < 8; i++)
	{
		if (debug == 2)
			printf("%x ", (buf[i] & 0xff));
		
		if ((buf[i] & 0xff) != 0xa5)
			return -1;
	}
	
	if (debug == 2) 
		printf("\n");
	
	return 0;
}

//
// Writes a notify byte so the weather station knows a setting has changed.
//
void notify_weather_setting_change(struct usb_dev_handle *h)
{
	// Write 0xAA to address 0x1a to indicate a change of settings.
	send_usb_msg8(h, 0xa2, 0x00, 0x1a, 0x20, 0xa2, 0xaa, 0x00, 0x20);
	read_weather_ack(h);
}

//
// Sets a single byte at a specified offset in the fixed weather settings chunk.
//
int set_weather_setting_byte(struct usb_dev_handle *h, unsigned int offset, char data)
{
	assert(offset < WEATHER_SETTINGS_CHUNK_SIZE);
	send_usb_msg8(h, 0xa2, 0x00, offset, 0x20, 0xa2, data, 0x00, 0x20);
	return read_weather_ack(h);
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
	unsigned int i;
	char buf[WEATHER_SETTINGS_CHUNK_SIZE];
	
	// Make sure we're not trying to write outside the settings buffer.
	assert((change_offset + len) < WEATHER_SETTINGS_CHUNK_SIZE);
	
	get_settings_block_raw(h, buf, sizeof(buf));
	
	// Change the settings.
	memcpy(&buf[change_offset], data, len);
	
	// Send back the settings in 3 32-bit chunks.
	for (offset = 0; offset < (32 * 3); offset += 32)
	{
		send_weather_msg(h, 0xa0, 0x00, offset, 0x20);
		
		// Send 4 * 8 bytes.
		for (i = offset; i < (offset + (4 * 8)); i += 8)
		{
			send_usb_msgbuf(h, &buf[i], 8);
		}
		
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
	char buf[32];
	unsigned short high_pos = (history_pos >> 8) & 0xff;
	unsigned short low_pos = (history_pos & 0xff);
	weather_data_t d;
	
	// The memory address we want to read from is sent.
	send_weather_msg(h, 0xa1, high_pos, low_pos, 0x20);
	read_weather_msg(h, buf);
	
	if (debug == 2)
	{
		print_bytes(buf, 32);
	}

	memset(&d, 0, sizeof(d));
	
	d.delay  			= buf[0];
	d.in_humidity	 	= buf[1];
	d.in_temp 			= (buf[2] & 0xff) | (buf[3] << 8);
	d.out_humidity		= buf[4];
	d.out_temp			= (buf[5] & 0xff) | (buf[6] << 8);
	d.abs_pressure		= (buf[7] & 0xff) | (buf[8] << 8);
	d.avg_wind_lowbyte	= buf[9];
	d.gust_wind_lowbyte = buf[10];
	d.wind_highbyte		= buf[11];
	d.wind_direction	= buf[12];
	d.total_rain		= (buf[13] & 0xff) | (buf[14] << 8);
	d.status			= buf[15];
	
	memcpy(&d.raw_data, buf, sizeof(d.raw_data));

	return d;
}

float convert_avg_windspeed(weather_data_t *wdp)
{
	return (wdp->avg_wind_lowbyte | (wdp->wind_highbyte & 0xf)) * 0.1f;
}

float convert_gust_windspeed(weather_data_t *wdp)
{
	return (wdp->avg_wind_lowbyte | ((wdp->wind_highbyte >> 8) & 0xf)) * 0.1f;
}

typedef struct weather_item_s
{
	weather_data_t data;
	time_t timestamp;
	unsigned int address;
} weather_item_t;

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

float calculate_dewpoint(weather_data_t *wd)
{
	#define DEW_A 17.27
	#define DEW_B 237.7
	float temp = wd->out_temp * 0.1f;
	float gamma = (DEW_A * temp / (DEW_B + temp)) + log(wd->out_humidity / 100.0);
	float dew_point = DEW_B * gamma / (DEW_A - gamma);
	return dew_point;
}

//
// Court's formula for Heat Loss.
// 
float calculate_windchill(weather_data_t *wd)
{
	float wc;
	float avg_windspeed = convert_avg_windspeed(wd);
	float t = wd->out_temp * 0.1f;

	if ((t < 33.0) && (avg_windspeed >= 1.79))
	{
		wc = 33 + ((t - 33) * (0.55 + (0.417 * sqrt(avg_windspeed)) - (0.0454 * avg_windspeed)));
	}
	else
	{
		wc = t;
	}
	
	return wc;
}

unsigned int calculate_beaufort(float windspeed)
{
	float k = 0.8365;
	return (int)(pow((windspeed / k), (2.0 / 3.0)) + 0.5);
}

float calculate_rel_pressure(weather_data_t *wd)
{
	float p = wd->abs_pressure * 0.1f;
	float m = program_settings.altitude / (18429.1 + (67.53 * wd->out_temp) + (0.003 * program_settings.altitude));
	p = p * pow(10.0, m);
	return p;
}

//
// Gets the closest history item to the amount of seconds either forward or backwards in time from the given index.
//
weather_item_t *get_history_item_seconds_delta(weather_item_t *history, unsigned int history_count, unsigned int index, int seconds_delta)
{
	unsigned int i;
	int seconds = 0;
	int direction = (seconds <= 0) ? -1 : 1;
	
	for (i = index; (i > 0) && (i < history_count); i += direction)
	{
		seconds += (history[i].data.delay * 60);
		
		if (seconds >= abs(seconds_delta))
			return &history[i];
	}
	
	return &history[i];
}

//
// Calculates the rain since x hours ago.
//
float calculate_rain_hours_ago(weather_item_t *history, unsigned int history_count, unsigned int index, unsigned int hours_ago)
{
	weather_item_t *cur		= &history[index];
	weather_item_t *prev	= get_history_item_seconds_delta(history, history_count, index, hours_ago * 60 * 60);
	float total_rain 		= cur->data.total_rain * 0.3f;
	float prev_total_rain	= prev->data.total_rain * 0.3f;
	float rain_delta 		= (total_rain - prev_total_rain);
	
	if ((rain_delta < 0) || (prev_total_rain >= 65000))
	{
		total_rain = prev_total_rain = 0.0f;
	}
	
	return (total_rain - prev_total_rain) * 0.3f;
}

float calculate_rain_1h(weather_item_t *history, unsigned int history_count, unsigned int index)
{
	return calculate_rain_hours_ago(history, history_count, index, 1);
}

float calculate_rain_24h(weather_item_t *history, unsigned int history_count, unsigned int index)
{
	return calculate_rain_hours_ago(history, history_count, index, 24);
}

void print_history_item_formatstring(weather_settings_t *ws, weather_item_t *history, unsigned int history_count, unsigned int index, char *format_str)
{
	weather_data_t *wd = &history[index].data;
	char *s = format_str;
	
	while (*s)
	{
		if (*s == '%')
		{
			s++;
			
			switch (*s)
			{
				case 'i': printf("%u", index); break; // History item index.
				case 'h': printf("%u", wd->in_humidity);			break; // Inside humidity.
				case 'H': printf("%u", wd->out_humidity);			break; // Outside humidity.
				case 't': printf("%0.1f", wd->in_temp * 0.1f);		break; // Inside temperature.
				case 'T': printf("%0.1f", wd->out_temp * 0.1f);	break; // Outside temperature.
				case 'C': printf("%0.1f", calculate_dewpoint(wd));	break; // Dewpoint.
				case 'c': printf("%0.1f", calculate_windchill(wd));	break; // Windchill.
				case 'W': printf("%0.1f", convert_avg_windspeed(wd));break; // Average wind speed.
				case 'G': printf("%0.1f", convert_gust_windspeed(wd));break; // Gust wind speed.
				case 'D': printf("%s", get_wind_direction(wd->wind_direction)); break; // Wind direction, name.
				case 'd': printf("%0.1f", wd->wind_direction * 22.5f); break; // Wind direction, degrees.
				case 'P': printf("%0.1f", wd->abs_pressure * 0.1f); break; // Absolute pressure.
				case 'p': printf("%0.1f", calculate_rel_pressure(wd)); break; // Relative pressure.
				case 'R': printf("%0.1f", wd->total_rain * 0.3f); 	break; // Total rain.
				case 'r': printf("%0.1f", calculate_rain_1h(history, history_count, index)); break; // Rain 1h mm/h.
				case 'F': printf("%0.1f", calculate_rain_24h(history, history_count, index) / 24.0f); break; // rain 24h mm.
				case 'f': printf("%0.1f", calculate_rain_24h(history, history_count, index)); break; // rain 24h mm/h.
				case 'N': printf("%s", get_timestamp(history[index].timestamp)); break; // Date.
				case '%': printf("%%"); break;
				default: break;
			}
		}
		else if (*s == '\\')
		{
			s++;
			switch (*s)
			{
				case 'n': printf("\n"); break;
				case 't': printf("\t"); break;
				case 'r': printf("\r"); break;
				case '\\': printf("\\"); break;
				default: printf("%c", *s); break;
			}
		}
		else
		{
			printf("%c", *s);
		}
		
		s++;
	}
}

//   1, 2010-09-13 13:41:34, 2010-08-13 14:46:53,  30,   53,  26.1,   55,  25.2,  15.5,  24.1,  1019.3,  1013.3,  3.1,   2,  5.8,   4,  10,  SW, 		   34,    10.2,     0.0,     0.0,     0.0,     0.0,     0.0,      0.0, 0, 0, 0, 0, 0, 0, 0, 0, 000100, 1E 35 05 01 37 FC 00 D1 27 1F 3A 00 0A 22 00 00 ,       
void print_history_item(weather_item_t *item, unsigned int index)
{
	weather_data_t *wd = &item->data;
	int i;
	
	//      1    2   3   4   5   6      7   8      9      10	 11		12     13    14   15    16   17    18  19   20     21    22      23     24     25     26    27  28  29  30  31  32  33  34  35
	printf("%u, %s, %s, %u, %u, %2.1f, %u, %2.1f, %2.1f, %2.1f, %4.1f, %4.1f, %2.1f, %u, %2.1f, %u, %2.1f, %s, %d, %2.1f, %2.1f, %2.1f, %2.1f, %2.1f, %2.1f, %2.1f, %u, %u, %u, %u, %u, %u, %u, %u, %06x, ", 
		index, 											// 1  Index.
		get_local_timestamp(), 							// 2  Date/time read from weather station.
		get_timestamp(item->timestamp),					// 3  Date/time data was recored.
		wd->delay,										// 4  Minutes since previous reading.
		wd->in_humidity,								// 5  Indoor humidity.
		wd->in_temp * 0.1f,								// 6  Indoor temperature.
		wd->out_humidity,								// 7  Outdoor humidity.
		wd->out_temp * 0.1f,							// 8  Outdoor temperature.
		calculate_dewpoint(wd),							// 9  Dew point.
		calculate_windchill(wd),						// 10 Wind chill.
		wd->abs_pressure * 0.1f,						// 11 Absolute pressure.
		wd->abs_pressure * 0.1f,						// 12 Relative pressure. // TODO: Calculate this somehow!!!
		convert_avg_windspeed(wd),						// 13 Wind average (m/s).
		calculate_beaufort(convert_avg_windspeed(wd)),	// 14 Wind average Beaufort. // TODO: Calculate this, integer.
		convert_gust_windspeed(wd),						// 15 Wind gust (m/s).
		calculate_beaufort(convert_gust_windspeed(wd)),	// 16 Wind gust (Beaufort). // TODO: Calculate this, integer.
		wd->wind_direction * 22.5f,						// 17 Wind direction.
		get_wind_direction(wd->wind_direction),			// 18 Wind direction, text.
		wd->total_rain,									// 19 Rain ticks integer. Cumulative count of number of times rain gauge has tipped. Resets to zero if station's batteries removed
		wd->total_rain * 0.3f,							// 20 mm rain total. Column 19 * 0.3, but does not reset to zero, stays fixed until ticks catch up.
		0.0,											// 21 Rain since last reading. mm
		0.0,											// 22 Rain in last hour. mm
		0.0,											// 23 Rain in last 24 hours. mm
		0.0,											// 24 Rain in last 7 days. mm
		0.0,											// 25 Rain in last 30 days. mm
		0.0,											// 26 Rain total in last year? mm. This is the same as column 20 in my data
		((wd->status >> 0) & 0x1),						// 27 Status bit 0.
		((wd->status >> 1) & 0x1),						// 28 Status bit 1.
		((wd->status >> 2) & 0x1),						// 29 Status bit 2.
		((wd->status >> 3) & 0x1),						// 30 Status bit 3.
		((wd->status >> 4) & 0x1),						// 31 Status bit 4.
		((wd->status >> 5) & 0x1),						// 32 Status bit 5.
		((wd->status >> 6) & 0x1),						// 33 Status bit 6.
		((wd->status >> 7) & 0x1),						// 34 Status bit 7.
		item->address);									// 35 Data address.
	
	for (i = 0; i < 16; i++)
	{
		printf("%X ", wd->raw_data[i]);
	}
	
	printf(",\n");
}

void print_settings(weather_settings_t *ws)
{
	printf("Unit settings:\n");
	printf("  Indoor temperature unit:\t%s\n",  (ws->unit_settings1 & (1 << 0)) ? "Fahrenheit" : "Celcius");
	printf("  Outdoor temperature unit:\t%s\n", (ws->unit_settings1 & (1 << 1)) ? "Fahrenheit" : "Celcius");
	printf("  Rain unit:\t\t\t%s\n", (ws->unit_settings1 & (1 << 2)) ? "mm" : "inch");
	
	printf("  Pressure unit:\t\t");
	if (ws->unit_settings1 & (1 << 5))
		printf("hPa");
	else if (ws->unit_settings1 & (1 << 6)) 
		printf("inHg");
	else if (ws->unit_settings1 & (1 << 7)) 
		printf("mmHg");
	printf("\n");
	
	printf("  Wind speed unit:\t\t");
	if (ws->unit_settings2 & (1 << 0))
		printf("m/s");
	else if (ws->unit_settings2 & (1 << 1)) 
		printf("km/h");
	else if (ws->unit_settings2 & (1 << 2)) 
		printf("knot");
	else if (ws->unit_settings2 & (1 << 3)) 
		printf("m/h");
	else if (ws->unit_settings2 & (1 << 4)) 
		printf("bft");
	printf("\n");
	
	printf("Display settings:\n");
	printf("  Pressure:\t\t\t%s\n", (ws->display_options1 & (1 << 0)) ? "Relative" : "Absolute" );
	printf("  Wind speed:\t\t\t%s\n", (ws->display_options1 & (1 << 1)) ? "Gust" : "Average");
	printf("  Time:\t\t\t\t%s\n", (ws->display_options1 & (1 << 2)) ? "12 hour" : "24 hour");
	printf("  Date:\t\t\t\t%s\n", (ws->display_options1 & (1 << 3)) ?  "Month-day-year" : "Day-month-year");
	printf("  Time scale:\t\t\t%s\n", (ws->display_options1 & (1 << 4)) ? "24 hour" : "12 hour");
	
	printf("  Date:\t\t\t\t");
	if (ws->display_options1 & (1 << 5))
		printf("Show year year");
	else if (ws->display_options1 & (1 << 6)) 
		printf("Show day name");
	else if (ws->display_options1 & (1 << 7)) 
		printf("Alarm time");
	printf("\n");
	
	printf("  Outdoor temperature:\t\t");
	if (ws->display_options2 & (1 << 0))
		printf("Temperature");
	else if (ws->display_options2 & (1 << 1)) 
		printf("Wind chill");
	else if (ws->display_options2 & (1 << 2)) 
		printf("Dew point");
	printf("\n");
	
	printf("  Rain:\t\t\t\t");
	if (ws->display_options2 & (1 << 3))
		printf("Hour");
	else if (ws->display_options2 & (1 << 4)) 
		printf("Day");
	else if (ws->display_options2 & (1 << 5)) 
		printf("Week");
	else if (ws->display_options2 & (1 << 6)) 
		printf("Month");
	else if (ws->display_options2 & (1 << 7)) 
		printf("Total");
	printf("\n");
}

void print_alarms(weather_settings_t *ws)
{
	#define ALARM_ENABLED(index, bit) ((ws->alarm_enable##index & (1 << bit)) ? "Enabled" : "Disabled")
	printf("Alarm enable:\n"); 
	printf("  Time:\t\t\t\t%02u:%02u\t\t%s\n",				(ws->alarm_time >> 4) & 0xf, 
															ws->alarm_time & 0xf, 
																								ALARM_ENABLED(1, 1));
	printf("  Wind direction:\t\t%2.1f %s\t\t%s\n", 		ws->alarm_wind_direction * 22.5f, 
															get_wind_direction(ws->alarm_wind_direction), 
																								ALARM_ENABLED(1, 2));
	printf("  Indoor humidity low:\t\t%u%%\t\t%s\n",		ws->alarm_inhumid_low,				ALARM_ENABLED(1, 4));
	printf("  Indoor humidity high:\t\t%u%%\t\t%s\n",	 	ws->alarm_inhumid_high, 			ALARM_ENABLED(1, 5));
	printf("  Outdoor humidity low:\t\t%u%%\t\t%s \n",	 	ws->alarm_outhumid_low, 			ALARM_ENABLED(1, 6));
	printf("  Outdoor humidity high:\t%u%%\t\t%s\n",		ws->alarm_outhumid_high, 			ALARM_ENABLED(1, 7));
	printf("  Wind average:\t\t\t%2.1f m/s\t\t%s\n",		ws->alarm_avg_wspeed_ms * 0.1f,		ALARM_ENABLED(2, 0));
	printf("  Wind gust:\t\t\t%u m/s\t\t%s\n",	 			ws->alarm_gust_wspeed_ms,			ALARM_ENABLED(2, 1));
	printf("  Rain hourly:\t\t\t%2.1f mm\t\t%s\n", 			ws->alarm_rain_hourly * 0.3f,		ALARM_ENABLED(2, 2));
	printf("  Rain daily:\t\t\t%2.1f mm\t%s\n",				ws->alarm_rain_daily * 0.3f,		ALARM_ENABLED(2, 3));
	printf("  Abs pressure low:\t\t%4.1f hPa\t%s\n",	 	ws->alarm_abs_pressure_low * 0.1f,	ALARM_ENABLED(2, 4));
	printf("  Abs pressure high:\t\t%4.1f hPa\t%s\n",		ws->alarm_abs_pressure_high * 0.1f, ALARM_ENABLED(2, 5));
	printf("  Abs relative low:\t\t%4.1f hPa\t%s\n",		ws->alarm_rel_pressure_low * 0.1f,	ALARM_ENABLED(2, 6));
	printf("  Abs relative high:\t\t%4.1f hPa\t%s\n",		ws->alarm_rel_pressure_high * 0.1f,	ALARM_ENABLED(2, 7));
	printf("  Indoor temperature low:\t%2.1f C\t\t%s\n",	ws->alarm_intemp_low * 0.1f,		ALARM_ENABLED(3, 0));
	printf("  Indoor temperature high:\t%2.1f C\t\t%s\n",	ws->alarm_intemp_high * 0.1f,		ALARM_ENABLED(3, 1));
	printf("  Outdoor temperature low:\t%2.1f C\t%s\n",		ws->alarm_outtemp_low * 0.1f,		ALARM_ENABLED(3, 2));
	printf("  Outdoor temperature high:\t%2.1f C\t\t%s\n",	ws->alarm_outtemp_high * 0.1f,		ALARM_ENABLED(3, 3));
	printf("  Wind chill low:\t\t%2.1f C\t\t%s\n",			ws->alarm_windchill_low * 0.1f,		ALARM_ENABLED(3, 4));
	printf("  Wind chill high:\t\t%2.1f C\t\t%s\n",			ws->alarm_windchill_high * 0.1f,	ALARM_ENABLED(3, 5));
	printf("  Dew point low:\t\t%2.1f C\t%s\n",				ws->alarm_dewpoint_low * 0.1f, 		ALARM_ENABLED(3, 6));
	printf("  Dew point high:\t\t%2.1f C\t\t%s\n",			ws->alarm_dewpoint_high * 0.1f,		ALARM_ENABLED(3, 7));
}

void print_maxmin(weather_settings_t *ws)
{
	printf("Max/min values:\t\t\tValue\t\tDate/Time\n");
	printf("  Max indoor temperature:\t%2.1f C\t\t",		ws->max_intemp * 0.1f);		  print_bcd_date(ws->max_intemp_date); printf("\n");
	printf("  Min indoor temperature:\t%2.1f C\t\t",		ws->min_intemp * 0.1f);		  print_bcd_date(ws->min_intemp_date); printf("\n");
	printf("  Max outdoor temperature:\t%2.1f C\t\t",		ws->max_outtemp * 0.1f);	  print_bcd_date(ws->max_outtemp_date); printf("\n");
	printf("  Min outdoor temperature:\t%2.1f C\t\t",		ws->min_outtemp * 0.1f);	  print_bcd_date(ws->min_outtemp_date); printf("\n");
	printf("  Max windchill:\t\t%2.1f C\t\t",				ws->max_windchill * 0.1f);	  print_bcd_date(ws->max_windchill_date); printf("\n");
	printf("  Min windchill:\t\t%2.1f C\t\t",				ws->min_windchill * 0.1f);	  print_bcd_date(ws->min_windchill_date); printf("\n");
	printf("  Max dewpoint:\t\t\t%2.1f C\t\t",				ws->max_dewpoint * 0.1f);	  print_bcd_date(ws->max_dewpoint_date); printf("\n");
	printf("  Min dewpoint:\t\t\t%2.1f C\t\t",				ws->min_dewpoint * 0.1f);	  print_bcd_date(ws->min_dewpoint_date); printf("\n");
	printf("  Max indoor humidity:\t\t%u%%\t\t",			ws->max_inhumid);			  print_bcd_date(ws->max_inhumid_date); printf("\n");
	printf("  Min indoor humidity:\t\t%u%%\t\t",			ws->min_inhumid);			  print_bcd_date(ws->min_inhumid_date); printf("\n");
	printf("  Max outdoor humidity:\t\t%u%%\t\t",			ws->max_outhumid);			  print_bcd_date(ws->max_outhumid_date); printf("\n");
	printf("  Min outdoor humidity:\t\t%u%%\t\t",			ws->min_outhumid);			  print_bcd_date(ws->min_outhumid_date); printf("\n");
	printf("  Max abs pressure:\t\t%5.1f hPa\t",			ws->max_abs_pressure * 0.1f); print_bcd_date(ws->max_abs_pressure_date); printf("\n");
	printf("  Min abs pressure:\t\t%5.1f hPa\t", 			ws->min_abs_pressure * 0.1f); print_bcd_date(ws->min_abs_pressure_date); printf("\n");
	printf("  Max relative pressure:\t%5.1f hPa\t",			ws->max_rel_pressure * 0.1f); print_bcd_date(ws->max_rel_pressure_date); printf("\n");
	printf("  Min relative pressure:\t%5.1f hPa\t",			ws->min_rel_pressure * 0.1f); print_bcd_date(ws->min_rel_pressure_date); printf("\n");
	printf("  Max average wind speed:\t%2.1f m/s\t\t",		ws->max_avg_wspeed * 0.1f);	  print_bcd_date(ws->max_avg_wspeed_date); printf("\n");
	printf("  Max gust wind speed:\t\t%2.1f m/s\t",			ws->max_gust_wspeed * 0.1f);  print_bcd_date(ws->max_gust_wspeed_date); printf("\n");
	printf("  Max rain hourly:\t\t%2.1f mm\t\t",			ws->max_rain_hourly * 0.3f);  print_bcd_date(ws->max_rain_hourly_date); printf("\n");
	printf("  Max rain daily:\t\t%2.1f mm\t\t",				ws->max_rain_daily * 0.3f);   print_bcd_date(ws->max_rain_daily_date); printf("\n");
	printf("  Max rain weekly:\t\t%2.1f mm\t\t",			ws->max_rain_weekly * 0.3f);  print_bcd_date(ws->max_rain_weekly_date); printf("\n");
	printf("  Max rain monthly:\t\t%2.1f mm\t",				ws->max_rain_monthly * 0.3f); print_bcd_date(ws->max_rain_monthly_date); printf("\n");
	printf("  Max rain total:\t\t%2.1f mm\t",				ws->max_rain_total * 0.3f);   print_bcd_date(ws->max_rain_total_date); printf("\n");
}

void print_status(weather_settings_t *ws)
{
	printf("Magic number:\t\t\t0x%x%x\n", ws->magic_number[0], ws->magic_number[1]);
	printf("Read period:\t\t\t%u minutes\n", ws->read_period);
		
	printf("Timezone:\t\t\tCET%s%u\n", (ws->timezone > 0) ? "+" : "-", ws->timezone);
	printf("Data count:\t\t\t%u/%u (%1.1f%%)\n", ws->data_count, WEATHER_STATION_HISTORY_MAX, (float)ws->data_count / WEATHER_STATION_HISTORY_MAX * 100);
	printf("Current memory position:\t%u (0x%x)\n", ws->current_pos, ws->current_pos);
	printf("Current relative pressure:\t%4.1f hPa\n", ws->relative_pressure * 0.1f);
	printf("Current Absolute pressure:\t%4.1f hPa\n", ws->absolute_pressure * 0.1f);
	printf("Unknown bytes:\t\t\t0x");
	{
		int i;
		for (i = 0; i < 7; i++) printf("%x", ws->unknown[i]);
	}
	printf("\n");
	printf("Station date/time:\t\t");
	print_bcd_date(ws->datetime);
	printf("\n");
}

void print_summary(weather_settings_t *ws, weather_item_t *item)
{
	weather_data_t *wd = &item->data;	
	printf("Use --help for more options.\n\n");

	printf("Indoor:\n");
	printf("  Temperature:\t\t%2.1f C\n",		wd->in_temp * 0.1f);
	printf("  Humidity:\t\t%u%%\n",				wd->in_humidity);
	printf("\n");
	printf("Outdoor:\n");
	printf("  Temperature:\t\t%2.1f C\n",		wd->out_temp * 0.1f);
	printf("  Windchill:\t\t%2.1f C\n",			calculate_windchill(wd));
	printf("  Dewpoint:\t\t%2.1f C\n",			calculate_dewpoint(wd));
	printf("  Humidity:\t\t%u%%\n",				wd->out_humidity);
	printf("  Abs pressure:\t\t%4.1f hPa\n",	wd->abs_pressure * 0.1f);
	printf("  Average windspeed:\t%2.1f m/s\n",	convert_avg_windspeed(wd));
	printf("  Gust windspeed:\t%2.1f m/s\n",	convert_gust_windspeed(wd));
	printf("  Wind direction:\t%2.1f %s\n",		wd->wind_direction * 22.5f, get_wind_direction(wd->wind_direction));
	printf("  Total rain:\t\t%2.1f mm\n",		wd->total_rain * 0.3f);
	printf("\n");
}

void get_weather_data(struct usb_dev_handle *h)
{
	int i;
	int history_address;
	weather_item_t history[WEATHER_STATION_HISTORY_MAX];
	weather_settings_t ws;
	unsigned int items_to_read = 0;
	
	PRINT_DEBUG("Start Reading status block\n");
	ws = get_settings_block(h);
	PRINT_DEBUG("End Reading status block\n\n");
	
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
	
	if (ws.magic_number[0] != 0x55)
	{
		fprintf(stderr, "Incorrect magic number!\n");
		return;
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
		unsigned int target_address = (ws.current_pos - (items_to_read * 16));

		memset(&history, 0, sizeof(history));
		
		// Correct the date based on timezone.
		//station_date -= (ws.timezone * 60 * 60); // TODO: this shouldn't be needed :p
		//printf("Station date: %s\n", get_timestamp(station_date));
		//printf("Timezone: UTC%s%u\n", (ws.timezone > 0) ? "+" : "-", ws.timezone);
		
		PRINT_DEBUG("Start reading history blocks\n");
		PRINT_DEBUG("Index\tTimestamp\t\tDelay\n");

		for (history_address = ws.current_pos, 
			i = ws.data_count; 
			(history_address > target_address) && (history_address > WEATHER_SETTINGS_CHUNK_SIZE); 
			history_address -= 16, i--)
		{			
			// Read history chunk.
			history[i].address = history_address;
			history[i].data = get_history_chunk(h, &ws, history_address);;

			// Calculate timestamp
			seconds = history[i].data.delay * 60;
			total_seconds += seconds;
			history[i].timestamp = (time_t)(station_date - total_seconds);

			// Debug print.
			if (debug)
			{
				printf("DEBUG: %d,\t%s,\t%u minutes\n", 
					i, 
					get_timestamp(history[i].timestamp), 
					history[i].data.delay);
			}
		}
		
		PRINT_DEBUG("End reading history blocks\n\n");
	}
	
	if (program_settings.show_summary)
	{
		print_summary(&ws, &history[ws.data_count]);
	}
	
	if (program_settings.show_formatted)
	{
		for (i = (ws.data_count - items_to_read + 1); i <= ws.data_count; i++)
		{
			print_history_item_formatstring(&ws, history, ws.data_count, i, program_settings.format_str);
		}
	}
	// Prints output in the Easyweather.dat format.
	else if (program_settings.show_easyweather)
	{
		// Output chronologically.
		for (i = (ws.data_count - items_to_read + 1); i <= ws.data_count; i++)
		{
			print_history_item(&history[i], i);
		}
	}
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
		}
		else
		{
			fprintf(stderr, "Failed to update delay.\n");
		}
	}
}

int read_arguments(int argc, char **argv)
{
	int c;
	memset(&program_settings, 0, sizeof(program_settings));
	
	// Set defaults.
	program_settings.count = 1;
	program_settings.mode = get_mode;
	
	if (argc == 1)
		program_settings.show_summary = 1;
	
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
			{"count", required_argument,		0, 'c'},
			{"timezone", required_argument, 	0, 't'},
			{"delay", required_argument, 		0, 'd'},
			{"help", required_argument,			0, 'h'},
			{"format", required_argument,		0, 0},
			{"formatlist", no_argument,			0, 0},
			{0, 0, 0, 0}
		};
		
		int option_index = 0;
		c = getopt_long(argc, argv, "aesvmc:ht:d:", long_options, &option_index);
		
		if (c == -1)
		{
			break;
		}
			
		switch (c)
		{
			case 0:
			{
				if (!strcmp("format", long_options[option_index].name))
				{
					program_settings.show_formatted = 1;
					strcpy(program_settings.format_str, optarg);
				}
				else if (!strcmp("formatlist", long_options[option_index].name))
				{
					program_settings.show_formatlist = 1;
				}
				
				break;
			}
			case 'a': program_settings.count = 0; 				break;
			case 'v': program_settings.debug++;					break;
			case 'm': program_settings.show_maxmin = 1;			break;
			case 's': program_settings.show_status = 1;			break;
			case 'e': program_settings.show_easyweather = 1;	break;
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
	
	debug = program_settings.debug;
	
	return 0;
}

int main(int argc, char **argv)
{
	if (read_arguments(argc, argv))
		return 1;
		
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
		printf("%%r - Rain 1h (mm/h).\n");
		printf("%%f - Rain 24h (mm/h).\n");
		printf("%%P - Rain 24h (mm).\n");
		printf("%%R - Total rain (mm).\n");
		printf("%%N - Date/time string for the weather reading.\n");
		
		return 0;
	}
	
	// Open te device.
	devh = open_device();
	init_device_descriptors();
	
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
	}
	
	close_device(devh);

	return 0;
}




