#include <stdint.h>
#include <memory.h>

#include "CH58x_common.h"

#include "../usb.h"
#include "../debug.h"
#include "../../data.h"
#include "../../power.h"
#include "../../leddrv.h"

#define EP_NUM   (1)
#define IF_NUM   (0)

static __attribute__((aligned(4))) uint8_t ep_buf[64 + 64]; 
static uint8_t *const ep_out = ep_buf;
static uint8_t *const ep_in = ep_buf + 64;

static uint8_t hid_report[64];
static void (*on_write)(uint8_t *buf, uint16_t len);

static USB_ITF_DESCR if_desc = {
	.bLength = sizeof(USB_ITF_DESCR),
	.bDescriptorType = USB_DESCR_TYP_INTERF,
	.bInterfaceNumber = IF_NUM,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2, /* One for read, one for write */
	.bInterfaceClass = 0x03, /* HID class */
	.bInterfaceSubClass = 0, /* No subclass */
	.bInterfaceProtocol = 0, /* Not a Mouse nor Keyboard */
	.iInterface = 0 /* Index of string descriptor */ // For now, placed a 0 for testing
};

static USB_HID_DESCR hid_desc = {
	.bLength = sizeof(USB_HID_DESCR),
	.bDescriptorType = USB_DESCR_TYP_HID,
	.bcdHID = 0x0100,
	.bCountryCode = 0x00,
	.bNumDescriptors = 0x01,
	.bDescriptorTypeX = 0x22,
	.wDescriptorLengthL = 0x22,
	.wDescriptorLengthH = 0x00
};

static USB_ENDP_DESCR read_ep_desc = {
	.bLength = sizeof(USB_ENDP_DESCR),
	.bDescriptorType = USB_DESCR_TYP_ENDP,
	.bEndpointAddress = 0x80 | EP_NUM, /* IN endpoint */
	.bmAttributes = 0x03, /* exchange data over Interrupt */
	.wMaxPacketSize = sizeof(hid_report), /* bytes */
	.bInterval = 0xff /* polling interval */
};

static USB_ENDP_DESCR write_ep_desc = {
	.bLength = sizeof(USB_ENDP_DESCR),
	.bDescriptorType = USB_DESCR_TYP_ENDP,
	.bEndpointAddress = EP_NUM, /* IN endpoint */
	.bmAttributes = 0x03, /* exchange data over Interrupt */
	.wMaxPacketSize = sizeof(hid_report), /* bytes */
	.bInterval = 8 /* polling interval */
};

static const uint8_t report_desc[] = {
	0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
	0x09, 0x01,        // Usage (0x01)
	0xA1, 0x01,        // Collection (Application)

	/* IN */
	0x09, 0x02,        //   Usage (0x02)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0x00, 0xFF,  //   Logical Maximum (-256)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x40,        //   Report Count (64)
	0x81, 0x06,        //   INPUT

	/* OUT */
	0x09, 0x02,        //   Usage (0x02)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0x00, 0xFF,  //   Logical Maximum (-256)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x40,        //   Report Count (64)
	0x91, 0x06,        //   OUTPUT

	0xC0,              // End Collection
};

static void if_handler(USB_SETUP_REQ * request)
{
	_TRACE();
	static uint8_t report_val, idle_val;
	uint8_t req = request->bRequest;
	uint16_t type = request->wValue >> 8;

	switch(req) {
	case USB_GET_DESCRIPTOR:
		PRINT("- USB_GET_DESCRIPTOR\n");
		PRINT("bDescriptorType: 0x%02x\n", type);
		switch (type)
		{
		case USB_DESCR_TYP_REPORT:
			PRINT("- USB_DESCR_TYP_REPORT\n");
			start_send_block(report_desc, sizeof(report_desc));
			break;

		default:
			break;
		}
		break;

	case HID_GET_REPORT:
		PRINT("- HID_GET_REPORT\n");
		// send_handshake(ep_num, 1, ACK, 1, request->wLength);
		// TODO: update this request, ignore for now
		break;

	case HID_GET_IDLE:
		PRINT("- HID_GET_IDLE\n");
		start_send_block(&idle_val, 1);
		break;

	case HID_GET_PROTOCOL:
		PRINT("- HID_GET_PROTOCOL\n");
		start_send_block(&report_val, 1);
		break;

	case HID_SET_REPORT:
		PRINT("- HID_SET_REPORT\n");
		// Enable control register to receive interrupt (expect receiving DATA1 on EP_NUM)
		send_handshake(0, 1, ACK, 1, 0);
		break;

	case HID_SET_IDLE:
		PRINT("- HID_SET_IDLE\n");
		send_handshake(0, 1, ACK, 1, 0);
		break;

	case HID_SET_PROTOCOL:
		PRINT("- HID_SET_PROTOCOL\n");
		// TODO: update this request, ignore for now
		break;

	default:
		break;
	}
}

static int intflag;

static void ep_handler(USB_SETUP_REQ *request)
{
	_TRACE();
	static int tog;
	uint8_t token = R8_USB_INT_ST & MASK_UIS_TOKEN;

	switch(token) {
	case UIS_TOKEN_OUT:
		if (on_write)
			on_write(ep_out, R8_USB_RX_LEN);
		send_handshake(EP_NUM, 1, ACK, tog, 0);
		tog = !tog;
		break;

	case UIS_TOKEN_IN:
		// TODO: receving data (read)
		PRINT("received a interrupt (read request)\n");
		if (intflag) {
			intflag = 0;
			send_handshake(EP_NUM, 1, ACK, 1, sizeof(hid_report));
		} else {
			send_handshake(EP_NUM, 1, NAK, 1, 0);
		}
		break;
	
	default:
		break;
	}
}

// TODO: remove this, as it not suitable for current use case
void hiddev_report(uint8_t key)
{
	hid_report[2] = key;
	memcpy(ep_in, hid_report, sizeof(hid_report));
	send_handshake(EP_NUM, 1, ACK, 0, sizeof(hid_report));
	intflag = 1;
}

void hiddev_onWrite(void (*cb)(uint8_t *buf, uint16_t len))
{
	on_write = cb;
}

void hiddev_init()
{
	cfg_desc_append(&if_desc);
	cfg_desc_append(&hid_desc);
	cfg_desc_append(&read_ep_desc);
	cfg_desc_append(&write_ep_desc);

	if_register(IF_NUM, if_handler);
	ep_register(EP_NUM, ep_handler);

	dma_register(EP_NUM, ep_out);
}
