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
#include "wsp.h"
#include "memory.h"
#include "utils.h"

//
// Sends a USB message to the device from a given buffer.
//
int send_usb_msgbuf(struct usb_dev_handle *h, char *msg, int msgsize)
{
	int bytes_written = 0;

	debug_printf(2, "--> ");
	print_bytes(2, msg, msgsize);

	bytes_written = usb_control_msg(h, USB_TYPE_CLASS + USB_RECIP_INTERFACE,
									9, 0x200, 0, msg, msgsize, USB_TIMEOUT);
	//assert(bytes_written == msgsize);
	
	return bytes_written;
}

//
// All data from the weather station is read in 32 byte chunks.
//
int read_weather_msg(struct usb_dev_handle *h, char buf[32])
{
	return usb_interrupt_read(h, ENDPOINT_INTERRUPT_ADDRESS, buf, 32, USB_TIMEOUT);
}

//
// Reads a weather message from a given address in history.
//
int read_weather_address(struct usb_dev_handle *h, unsigned short addr, char buf[32])
{
	if (program_settings.from_file)
	{
		// Special case if we try to read the next to last history chunk.
		int bytes_to_read = (addr == (HISTORY_END - HISTORY_CHUNK_SIZE)) ? 16 : 32;
		FILE *f = program_settings.f;
		
		// Only open the file once.
		if (!f)
		{
			f = fopen(program_settings.infile, "r");
		}

		if (fseek(f, addr, SEEK_SET))
		{
			fprintf(stderr, "Failed to seek to position %d (0x%d) in file.\n", addr, addr);
			return -1;
		}
		
		if (fread(buf, 1, bytes_to_read, f) != bytes_to_read)
		{
			if (feof(f))
			{
				fprintf(stderr, "Tried to read past the end of file.\n");
			}
			else
			{
				perror("Error reading from file. ");
			}
			
			return -1;
		}
		
		return 0;
	}
	else
	{
		char msg[8] = {0xa1, (addr >> 8), (addr & 0xff), 0x20, 0xa1, 0, 0, 0x20};
		send_usb_msgbuf(h, msg, 8);
		return (read_weather_msg(h, buf) != 32);
	}
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
		debug_printf(2, "%x ", (buf[i] & 0xff));

		if ((buf[i] & 0xff) != 0xa5)
			return -1;
	}

	debug_printf(2, "\n");

	return 0;
}

//
// Writes 1 byte of data to the weather station.
//
int write_weather_1(struct usb_dev_handle *h, unsigned short addr, char data)
{
	char msg[8] = {0xa2, (addr >> 8), (addr & 0xff), 0x20, 0xa2, data, 0, 0x20};

	send_usb_msgbuf(h, msg, 8);	

	return read_weather_ack(h);
}

//
// Writes 32 bytes of data to the weather station.
//
int write_weather_32(struct usb_dev_handle *h, unsigned short addr, char data[32])
{
	char msg[8] = {0xa0, (addr >> 8), (addr & 0xff), 0x20, 0xa0, 0, 0, 0x20};

	send_usb_msgbuf(h, msg, 8);		// Send write command.
	send_usb_msgbuf(h, data, 32);	// Send data.
	
	return read_weather_ack(h);
}

