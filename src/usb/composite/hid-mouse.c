#include <stdint.h>
#include <memory.h>

#include "CH58x_common.h"

#include "../usb.h"
#include "../debug.h"

#define EP_NUM   (2)
#define IF_NUM   (1)

static __attribute__((aligned(4))) uint8_t ep_buf[64 + 64]; 
static uint8_t *const ep_out = ep_buf;
static uint8_t *const ep_in = ep_buf + 64;

static USB_ITF_DESCR if_desc = {
	.bLength = sizeof(USB_ITF_DESCR),
	.bDescriptorType = 0x04,
	.bInterfaceNumber = IF_NUM,
	.bAlternateSetting = 0x00,
	.bNumEndpoints = 0x01,
	.bInterfaceClass = 0x03,
	.bInterfaceSubClass = 0x01,
	.bInterfaceProtocol = 0x02,
	.iInterface = 0x00
};

static USB_HID_DESCR hid_desc = {
	.bLength = sizeof(USB_HID_DESCR),
	.bDescriptorType = 0x21,
	.bcdHID = 0x0110,
	.bCountryCode = 0x00,
	.bNumDescriptors = 0x01,
	.bDescriptorTypeX = 0x22,
	.wDescriptorLengthL = 0x34,
	.wDescriptorLengthH = 0x00
};

static USB_ENDP_DESCR read_ep_desc = {
	.bLength = sizeof(USB_ENDP_DESCR),
	.bDescriptorType = 0x05,
	.bEndpointAddress = 0x80 | EP_NUM,
	.bmAttributes = 0x03,
	.wMaxPacketSize = 0x0004,
	.bInterval = 50 // mS, polling interval
};

static uint8_t hid_report[4] = {0x0, 0x0, 0x0, 0x0};

static const uint8_t report_desc[] = {
	0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29,
	0x03, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x03, 0x81, 0x02, 0x75, 0x05, 0x95, 0x01,
	0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7f, 0x75,
	0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0
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

void mouse_hidReport(uint8_t mouse)
{
	hid_report[0] = mouse;
	memcpy(ep_in, hid_report, sizeof(hid_report));
	send_handshake(EP_NUM, 1, ACK, 0, sizeof(hid_report));
	intflag = 1;
}

void mouse_init()
{
	cfg_desc_append(&if_desc);
	cfg_desc_append(&hid_desc);
	cfg_desc_append(&read_ep_desc);

	if_register(IF_NUM, if_handler);
	ep_register(EP_NUM, ep_handler);

	dma_register(EP_NUM, ep_out);
}
