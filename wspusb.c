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
#include <assert.h>
#include <usb.h>
#include "wsp.h"
#include "wspusb.h"

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

    dev = find_device(program_settings.vendor_id, program_settings.product_id);

	if (!dev)
	{
		fprintf(stderr, "No device with vendor id: 0x%x (%d) and product id: %x (%d) was found\n",
			program_settings.vendor_id, program_settings.vendor_id,
			program_settings.product_id, program_settings.product_id);
		exit(1);
	}

    h = usb_open(dev);
    assert(h);
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

	if (!h || ret != 0)
	{
		fprintf(stderr, "Failed to open USB device, errorcode - %d\n", ret);
		exit(1);
	}

	return h;
}

//
// Inits the USB descriptors.
//
void init_device_descriptors(struct usb_dev_handle *h)
{
	char buf[1024];
	int ret = 0;

	ret = usb_get_descriptor(h, USB_DT_DEVICE, 0, buf, sizeof(buf));
	ret = usb_get_descriptor(h, USB_DT_CONFIG, 0, buf, sizeof(buf));
	ret = usb_release_interface(h, 0);

	if (ret != 0)
		fprintf(stderr, "Failed to release interface before set_configuration: %d\n", ret);

	ret = usb_set_configuration(h, 1);
	ret = usb_claim_interface(h, 0);

	if (ret != 0)
		fprintf(stderr, "Claim after set_configuration failed with error %d\n", ret);

	ret = usb_set_altinterface(h, 0);
	ret = usb_control_msg(h, USB_TYPE_CLASS + USB_RECIP_INTERFACE, 0xa, 0, 0, NULL, 0, USB_TIMEOUT);
	ret = usb_get_descriptor(h, USB_DT_REPORT, 0, buf, sizeof(buf));
}


