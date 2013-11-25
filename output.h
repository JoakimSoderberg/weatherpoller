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

#ifndef __OUTPUT_H__
#define __OUTPUT_H__

void print_history_item_formatstring(struct usb_dev_handle *h, weather_settings_t *ws, weather_item_t *history, unsigned int index, char *format_str);
void print_history_item(weather_item_t *item, unsigned int index);
void print_settings(weather_settings_t *ws);
void print_alarms(weather_settings_t *ws);
void print_maxmin(weather_settings_t *ws);
void print_status(weather_settings_t *ws);
void print_summary(weather_settings_t *ws, weather_item_t *item);

#endif // __OUTPUT_H__

