//
// Weather Station Poller
//
// Copyright (C) 2010 Joakim S�derberg
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

#ifndef __WSPUSB_H__
#define __WSPUSB_H__

struct usb_device *find_device(int vendor, int product);
void close_device(struct usb_dev_handle *h);
struct usb_dev_handle *open_device();
void init_device_descriptors();

#endif // __WSPUSB_H__
