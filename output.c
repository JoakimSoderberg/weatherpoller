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
#include <usb.h>
#include <stdlib.h>
#include "wsp.h"
#include "utils.h"
#include "output.h"
#include "weather.h"

void print_history_item_formatstring(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index, char *format_str)
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
				case 'i': printf("%u", history[index].history_index); break; // History item index.
				case 'h': printf("%u", wd->in_humidity);			break; // Inside humidity.
				case 'H': printf("%u", wd->out_humidity);			break; // Outside humidity.
				case 't': printf("%0.1f", wd->in_temp * 0.1f);		break; // Inside temperature.
				case 'T': printf("%0.1f", wd->out_temp * 0.1f);	break; // Outside temperature.
				case 'C': printf("%0.1f", calculate_dewpoint(wd));	break; // Dewpoint.
				case 'c': printf("%0.1f", calculate_windchill(wd));	break; // Windchill.
				case 'W': printf("%0.1f", convert_avg_windspeed(wd));break; // Average wind speed.
				case 'G': printf("%0.1f", convert_gust_windspeed(wd));break; // Gust wind speed.
				case 'D': printf("%s", get_wind_direction(wd->wind_direction)); break; // Wind direction, name.
				case 'd': printf("%0.0f", wd->wind_direction * 22.5f); break; // Wind direction, degrees.
				case 'P': printf("%0.1f", wd->abs_pressure * 0.1f); break; // Absolute pressure.
				case 'p': printf("%0.1f", calculate_rel_pressure(wd)); break; // Relative pressure.
				case 'R': printf("%0.1f", wd->total_rain * 0.3f); 	break; // Total rain.
				case 'r': printf("%0.1f", calculate_rain_1h(h, ws, history, index)); break; // Rain 1h mm/h.
				case 'F': printf("%0.1f", calculate_rain_24h(h, ws, history, index) / 24.0f); break; // rain 24h mm.
				case 'f': printf("%0.1f", calculate_rain_24h(h, ws, history, index)); break; // rain 24h mm/h.
				case 'N': printf("%s", get_timestamp(history[index].timestamp)); break; // Date.
				case 'e': printf("%s", has_contact_with_sensor(wd) ? "True" : "False"); break; // Has contact with sensor? True or False.
				case 'E': printf("%d", has_contact_with_sensor(wd)); break; // Has contact with sensor? 1 or 0.
				case 'a': printf("%04x", history[index].address); break; // History address.
				case '%': printf("%%"); break;
				case 'b':
				{
					int i;
					for (i = 0; i < 16; i++)
					{
						printf("%02X ", wd->raw_data[i]);
					}
					break;
				}
				default:
				{
					int c = (s - format_str);
					fprintf(stderr, "Incorrect format string at character %d, %%%c is not a valid variable.\n", c, *s);
					printf("\n");
					exit(1);
					break;
				}
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
		item->history_index, 							// 1  Index.
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
	printf("  Wind direction:\t\t%2.0f %s\t\t%s\n", 		ws->alarm_wind_direction * 22.5f,
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
	printf("Indoor:\n");
	printf("  Max indoor temperature:\t%2.1f C\t\t",		ws->max_intemp * 0.1f);		  print_bcd_date(ws->max_intemp_date); printf("\n");
	printf("  Min indoor temperature:\t%2.1f C\t\t",		ws->min_intemp * 0.1f);		  print_bcd_date(ws->min_intemp_date); printf("\n");
	printf("  Max indoor humidity:\t\t%u%%\t\t",			ws->max_inhumid);			  print_bcd_date(ws->max_inhumid_date); printf("\n");
	printf("  Min indoor humidity:\t\t%u%%\t\t",			ws->min_inhumid);			  print_bcd_date(ws->min_inhumid_date); printf("\n");
	printf("Outdoor:\n");
	printf("  Max outdoor temperature:\t%2.1f C\t\t",		ws->max_outtemp * 0.1f);	  print_bcd_date(ws->max_outtemp_date); printf("\n");
	printf("  Min outdoor temperature:\t%2.1f C\t\t",		ws->min_outtemp * 0.1f);	  print_bcd_date(ws->min_outtemp_date); printf("\n");
	printf("  Max windchill:\t\t%2.1f C\t\t",				ws->max_windchill * 0.1f);	  print_bcd_date(ws->max_windchill_date); printf("\n");
	printf("  Min windchill:\t\t%2.1f C\t\t",				ws->min_windchill * 0.1f);	  print_bcd_date(ws->min_windchill_date); printf("\n");
	printf("  Max dewpoint:\t\t\t%2.1f C\t\t",				ws->max_dewpoint * 0.1f);	  print_bcd_date(ws->max_dewpoint_date); printf("\n");
	printf("  Min dewpoint:\t\t\t%2.1f C\t\t",				ws->min_dewpoint * 0.1f);	  print_bcd_date(ws->min_dewpoint_date); printf("\n");
	printf("  Max outdoor humidity:\t\t%u%%\t\t",			ws->max_outhumid);			  print_bcd_date(ws->max_outhumid_date); printf("\n");
	printf("  Min outdoor humidity:\t\t%u%%\t\t",			ws->min_outhumid);			  print_bcd_date(ws->min_outhumid_date); printf("\n");
	printf("  Max abs pressure:\t\t%5.1f hPa\t",			ws->max_abs_pressure * 0.1f); print_bcd_date(ws->max_abs_pressure_date); printf("\n");
	printf("  Min abs pressure:\t\t%5.1f hPa\t", 			ws->min_abs_pressure * 0.1f); print_bcd_date(ws->min_abs_pressure_date); printf("\n");
	printf("  Max relative pressure:\t%5.1f hPa\t",			ws->max_rel_pressure * 0.1f); print_bcd_date(ws->max_rel_pressure_date); printf("\n");
	printf("  Min relative pressure:\t%5.1f hPa\t",			ws->min_rel_pressure * 0.1f); print_bcd_date(ws->min_rel_pressure_date); printf("\n");
	printf("  Max average wind speed:\t%2.1f m/s\t",		ws->max_avg_wspeed * 0.1f);	  print_bcd_date(ws->max_avg_wspeed_date); printf("\n");
	printf("  Max gust wind speed:\t\t%2.1f m/s\t",			ws->max_gust_wspeed * 0.1f);  print_bcd_date(ws->max_gust_wspeed_date); printf("\n");
	printf("  Max rain hourly:\t\t%2.1f mm\t",				ws->max_rain_hourly * 0.3f);  print_bcd_date(ws->max_rain_hourly_date); printf("\n");
	printf("  Max rain daily:\t\t%2.1f mm\t",				ws->max_rain_daily * 0.3f);   print_bcd_date(ws->max_rain_daily_date); printf("\n");
	printf("  Max rain weekly:\t\t%2.1f mm\t",				ws->max_rain_weekly * 0.3f);  print_bcd_date(ws->max_rain_weekly_date); printf("\n");
	printf("  Max rain monthly:\t\t%2.1f mm\t",				ws->max_rain_monthly * 0.3f); print_bcd_date(ws->max_rain_monthly_date); printf("\n");
	printf("  Max rain total:\t\t%2.1f mm\t",				ws->max_rain_total * 0.3f);   print_bcd_date(ws->max_rain_total_date); printf("\n");
}

void print_status(weather_settings_t *ws)
{
	printf("Magic number:\t\t\t0x%x%x\n", ws->magic_number[0], ws->magic_number[1] & 0xff);
	printf("Read period:\t\t\t%u minutes\n", ws->read_period);
	printf("Timezone:\t\t\tCET%s%u\n", (ws->timezone > 0) ? "+" : "-", ws->timezone);
	printf("Data count:\t\t\t%u/%u (%1.1f%%)\n", ws->data_count, HISTORY_MAX, (float)ws->data_count / HISTORY_MAX * 100);
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
	int contact = has_contact_with_sensor(wd);

	printf("Use --help for more options.\n\n");

	printf("Indoor:\n");
	printf("  Temperature:\t\t%2.1f C\n",				wd->in_temp * 0.1f);
	printf("  Humidity:\t\t%u%%\n",						wd->in_humidity);
	printf("\n");
	printf("Outdoor: %s\n", (!contact) ? "NO CONTACT WITH SENSOR" : "");

	// Only show current data if we have sensor contact.
	if (contact)
	{
		printf("  Temperature:\t\t%0.1f C\n",			wd->out_temp * 0.1f );
		printf("  Wind chill:\t\t%0.1f C\n",			calculate_windchill(wd));
		printf("  Dewpoint:\t\t%0.1f C\n",				calculate_dewpoint(wd));
		printf("  Humidity:\t\t%u%%\n",					wd->out_humidity);
		printf("  Absolute pressure:\t%0.1f hPa\n",		wd->abs_pressure * 0.1f);
		printf("  Relative pressure:\t%0.1f hPa\n",		calculate_rel_pressure(wd));
		printf("  Average windspeed:\t%0.1f m/s\n",		convert_avg_windspeed(wd));
		printf("  Gust wind speed:\t%2.1f m/s\n",		convert_gust_windspeed(wd));
		printf("  Wind direction:\t%0.0f %s\n",			wd->wind_direction * 22.5f, get_wind_direction(wd->wind_direction));
		printf("  Total rain:\t\t%0.1f mm\n",			wd->total_rain * 0.3f);
	}

	printf("\n");
}


