#include <stdint.h>
#include <memory.h>

#include "CH58x_common.h"

#include "../usb.h"
#include "../debug.h"

#define EP_NUM     (4)
#define IF_NUM     (0)

#if EP_NUM == 4
extern uint8_t ep0buf[]; // FIXME: only endpoint4 would need this
#define ep_out      (ep0buf + 64)
#define ep_in       (ep0buf + 128)

#else
static __attribute__((aligned(4))) uint8_t ep_buf[64 + 64]; 
static uint8_t *const ep_out = ep_buf;
static uint8_t *const ep_in = ep_buf + 64;
#endif

static USB_ITF_DESCR if_desc = {
	.bLength = sizeof(USB_ITF_DESCR),
	.bDescriptorType = 0x04,
	.bInterfaceNumber = IF_NUM,
	.bAlternateSetting = 0x00,
	.bNumEndpoints = 0x01,
	.bInterfaceClass = 0x03,
	.bInterfaceSubClass = 0x01,
	.bInterfaceProtocol = 0x01,
	.iInterface = 0x00
};

static USB_HID_DESCR hid_desc = {
	.bLength = sizeof(USB_HID_DESCR),
	.bDescriptorType = 0x21,
	.bcdHID = 0x0111,
	.bCountryCode = 0x00,
	.bNumDescriptors = 0x01,
	.bDescriptorTypeX = 0x22,
	.wDescriptorLengthL = 0x3e,
	.wDescriptorLengthH = 0x00
};

static USB_ENDP_DESCR ep_desc = {
	.bLength = sizeof(USB_ENDP_DESCR),
	.bDescriptorType = 0x05,
	.bEndpointAddress = 0x80 | EP_NUM,
	.bmAttributes = 0x03,
	.wMaxPacketSize = 0x0008,
	.bInterval = 0x0a
};

static uint8_t hid_report[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

static const uint8_t report_desc[] = {
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x06,        // Usage (Keyboard)
	0xA1, 0x01,        // Collection (Application)
	0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
	0x19, 0xE0,        //   Usage Minimum (0xE0)
	0x29, 0xE7,        //   Usage Maximum (0xE7)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x08,        //   Report Count (8)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x95, 0x01,        //   Report Count (1)
	0x75, 0x08,        //   Report Size (8)
	0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x95, 0x03,        //   Report Count (3)
	0x75, 0x01,        //   Report Size (1)
	0x05, 0x08,        //   Usage Page (LEDs)
	0x19, 0x01,        //   Usage Minimum (Num Lock)
	0x29, 0x03,        //   Usage Maximum (Scroll Lock)
	0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x95, 0x05,        //   Report Count (5)
	0x75, 0x01,        //   Report Size (1)
	0x91, 0x01,        //   Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x95, 0x06,        //   Report Count (6)
	0x75, 0x08,        //   Report Size (8)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
	0x19, 0x00,        //   Usage Minimum (0x00)
	0x29, 0x91,        //   Usage Maximum (0x91)
	0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              // End Collection
};

static void if_handler(USB_SETUP_REQ * request)
{
	_TRACE();
	static uint8_t report_val, idle_val;
	uint8_t req = request->bRequest;
	uint16_t type = request->wValue >> 8;

	PRINT("bDescriptorType: 0x%02x\n", type);

	switch(req) {
	case USB_GET_DESCRIPTOR:
		PRINT("- USB_GET_DESCRIPTOR\n");
		switch (type)
		{
		case USB_DESCR_TYP_REPORT:
			PRINT("- USB_DESCR_TYP_REPORT\n");
			ctrl_load_short_chunk(report_desc, sizeof(report_desc));
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
		ctrl_load_short_chunk(&idle_val, 1);
		break;

	case HID_GET_PROTOCOL:
		PRINT("- HID_GET_PROTOCOL\n");
		ctrl_load_short_chunk(&report_val, 1);
		break;

	case HID_SET_REPORT:
		PRINT("- HID_SET_REPORT\n");
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
	uint8_t token = R8_USB_INT_ST & MASK_UIS_TOKEN;

	switch(token) {
	case UIS_TOKEN_OUT:
		break;

	case UIS_TOKEN_IN:
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

void key_hidReport(uint8_t key)
{
	hid_report[2] = key;
	memcpy(ep_in, hid_report, sizeof(hid_report));
	send_handshake(EP_NUM, 1, ACK, 0, sizeof(hid_report));
	intflag = 1;
}

void key_init()
{
	cfg_desc_append(&if_desc);
	cfg_desc_append(&hid_desc);
	cfg_desc_append(&ep_desc);

	if_register(IF_NUM, if_handler);
	ep_register(EP_NUM, ep_handler);

	dma_register(EP_NUM, ep_out);
}
