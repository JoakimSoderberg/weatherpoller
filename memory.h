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

#ifndef __MEMORY_H__
#define __MEMORY_H__

int send_usb_msgbuf(struct usb_dev_handle *h, char *msg, int msgsize);
int read_weather_msg(struct usb_dev_handle *h, char buf[32]);
int read_weather_address(struct usb_dev_handle *h, unsigned short addr, char buf[32]);
int read_weather_ack(struct usb_dev_handle *h);
int write_weather_1(struct usb_dev_handle *h, unsigned short addr, char data);
int write_weather_32(struct usb_dev_handle *h, unsigned short addr, char data[32]);

#endif // __MEMORY_H__

