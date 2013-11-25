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

#ifndef __WEATHER_H__
#define __WEATHER_H__

int has_contact_with_sensor(weather_data_t *wdp);
float convert_avg_windspeed(weather_data_t *wdp);
float convert_gust_windspeed(weather_data_t *wdp);
float calculate_dewpoint(weather_data_t *wd);
float calculate_windchill(weather_data_t *wd);
unsigned int calculate_beaufort(float windspeed);
float calculate_rel_pressure(weather_data_t *wd);
weather_item_t *get_history_item_seconds_delta(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index, int seconds_delta);
float calculate_rain_hours_ago(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index, unsigned int hours_ago);
float calculate_rain_1h(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index);
float calculate_rain_24h(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index);

#endif // __WEATHER_H__
