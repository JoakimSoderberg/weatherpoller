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
#include "wsp.h"
#include "utils.h"
#include "weather.h"

int has_contact_with_sensor(weather_data_t *wdp)
{
	return !((wdp->status >> LOST_SENSOR_CONTACT_BIT) & 0x1);
}

float convert_avg_windspeed(weather_data_t *wdp)
{
	return (((wdp->wind_highbyte & 0xf) << 8) | (wdp->avg_wind_lowbyte & 0xff)) * 0.1f;
}

float convert_gust_windspeed(weather_data_t *wdp)
{
	return ((((wdp->wind_highbyte >> 4) & 0xf) << 8) | (wdp->gust_wind_lowbyte & 0xff)) * 0.1f;
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
	float m = program_settings.altitude / (18429.1 + 67.53 * wd->out_temp + 0.003 * program_settings.altitude);
	p = p * pow(10, m);
	return p;
}

//
// Gets the closest history item to the amount of seconds either forward or backwards in time from the given index.
//
weather_item_t *get_history_item_seconds_delta(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index, int seconds_delta)
{
	unsigned int i;
	int seconds = 0;
	int delay_seconds = 0;

	if (program_settings.quickrain)
	{
		// This is meant for when a small number of items are read, but we still
		// want accurate weather data.
		//
		// We cheat and assume that the delay between each item has always
		// been weather_settings_t.read_period instead of checking the time between
		// each history item. This might be inaccurate.

		// The number of items we need to go back to go seconds_delta seconds into the past.
		int index_delta = (seconds_delta / (ws->read_period * 60));
		//time_t station_date = bcd_to_unix_date(parse_bcd_date(ws->datetime));

		assert(abs(index_delta) < ws->data_count);

		i = index + index_delta;

		// If we're outside the range of available items we'll
		// just return the current item instead, so we don't get
		// inaccurate data (like calculating over 5 hours when we
		// were asked for 24h).
		if (i < (HISTORY_MAX - ws->data_count))
		{
			return &history[index];
		}

		// Fetch the data if it doesn't already exist in the history.
		if ((history[i].timestamp == 0))
		{
			int new_address;
			int new_index;
			//unsigned int history_begin = (ws->current_pos + HISTORY_CHUNK_SIZE);

			new_index = history[index].history_index + index_delta;
			new_address = HISTORY_START + (new_index * HISTORY_CHUNK_SIZE);

			// Read history chunk.
			history[i].history_index = new_index;
			history[i].address = new_address;
			history[i].data = get_history_chunk(h, ws, new_address);
			history[i].timestamp = (time_t)(history[index].timestamp + seconds_delta);
			
			return &history[i];
		}

		return &history[i];
	}
	else
	{
		// Go through enough previous (or future) history items relative to the current index
		// until we find the closest item which is "seconds_delta" seconds from the current history item.
		for (i = (index - 1); (i > (HISTORY_MAX - ws->data_count)) && (i < HISTORY_MAX); i--)
		{
			// We don't have enough history items to go any further.
			if (history[i].timestamp == 0)
				return &history[i-1];

			// TODO: if (has_contact_with_sensor(&history[i].data)) ...
			delay_seconds = (history[i].data.delay * 60);
			seconds += delay_seconds;

			if (seconds >= abs(seconds_delta))
				return &history[i];
		}

		return &history[i];
	}

	// If everything failed, just return the current item.
	return &history[index];
}

//
// Calculates the rain since x hours ago.
//
float calculate_rain_hours_ago(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index, unsigned int hours_ago)
{
	int seconds_to_go_back	= hours_ago * 60 * 60;
	weather_item_t *cur		= &history[index];
	weather_item_t *prev	= get_history_item_seconds_delta(h, ws, history, index, -seconds_to_go_back);
	float total_rain 		= cur->data.total_rain * 0.3f;
	float prev_total_rain	= prev->data.total_rain * 0.3f;

	if ((prev->timestamp == 0)
	|| (abs(cur->timestamp - prev->timestamp) < seconds_to_go_back))
	{
		return 0.0;
	}
	
	//printf("< %0.1f - %0.1f = %0.1f >", total_rain, prev_total_rain, (total_rain - prev_total_rain));

	return (total_rain - prev_total_rain);
}

float calculate_rain_1h(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index)
{
	return calculate_rain_hours_ago(h, ws, history, index, 1);
}

float calculate_rain_24h(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index)
{
	return calculate_rain_hours_ago(h, ws, history, index, 24);
}

