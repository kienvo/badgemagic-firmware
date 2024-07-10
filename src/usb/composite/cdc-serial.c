#include <stdint.h>
#include <memory.h>

#include "CH58x_common.h"

#include "../usb.h"
#include "../debug.h"

#define NOTI_EP_NUM   (2)
#define DATA_EP_NUM   (3)
#define ACM_IF_NUM    (1)

#define USB_DESCTYPE_CS_INTERFACE 0x24

static __attribute__((aligned(4))) uint8_t noti_ep_buf[64 + 64]; 
static uint8_t *const noti_ep_out = noti_ep_buf;
static uint8_t *const noti_ep_in = noti_ep_buf + 64;

static __attribute__((aligned(4))) uint8_t data_ep_buf[64 + 64]; 
static uint8_t *const data_ep_out = data_ep_buf;
static uint8_t *const data_ep_in = data_ep_buf + 64;

static void (*on_write)(uint8_t *buf, uint16_t len);

/* CDC Communication interface */
static USB_ITF_DESCR acm_if_desc = {
	.bLength = sizeof(USB_ITF_DESCR),
	.bDescriptorType = USB_DESCR_TYP_INTERF,
	.bInterfaceNumber = ACM_IF_NUM,
	.bAlternateSetting = 0,
	.bNumEndpoints = 3, // A Notification and RX/TX Endpoint

	.bInterfaceClass = 0x02, /* Communications and CDC Control */
	.bInterfaceSubClass = 2, /* ACM subclass */
	.bInterfaceProtocol = 1, /* AT Command V.250 protocol */

	.iInterface = 0 /* No string descriptor */
};

/* Header Functional descriptor */
static uint8_t header_func_desc[] = {
	5,                          /* bLength */
	USB_DESCTYPE_CS_INTERFACE,  /* bDescriptortype */
	0x00,       /* bDescriptorsubtype, HEADER */
	0x10, 0x01, /* bcdCDC */
};

/* ACM Functional descriptor */
static uint8_t acm_func_desc[] = {
	4,                          /* bLength */
	USB_DESCTYPE_CS_INTERFACE,  /* bDescriptortype */
	0x02,    /* bDescriptorsubtype, ABSTRACT CONTROL MANAGEMENT */
	0x02,    /* bmCapabilities: Supports subset of ACM commands */
};

/* Call Management Functional descriptor */
static uint8_t callmgr_func_desc[] = {
	4,                          /* bLength */
	USB_DESCTYPE_CS_INTERFACE,  /* bDescriptortype */
	0x01,    /* bDescriptorsubtype, CALL MANAGEMENT */
	0x03,    /* bmCapabilities, DIY */
};

/* Notification Endpoint descriptor */
static USB_ENDP_DESCR noti_ep_desc = {
	.bLength = sizeof(USB_ENDP_DESCR),
	.bDescriptorType = USB_DESCR_TYP_ENDP,
	.bEndpointAddress = 0x80 | NOTI_EP_NUM, /* IN endpoint */
	.bmAttributes = 0x03,       /* Interrupt transfer */
	.wMaxPacketSize = 64,       /* bytes */
	.bInterval = 0xff
};

/* Data TX Endpoint descriptor */
static USB_ENDP_DESCR tx_ep_desc = {
	.bLength = sizeof(USB_ENDP_DESCR),
	.bDescriptorType = USB_DESCR_TYP_ENDP,
	.bEndpointAddress = 0x80 | DATA_EP_NUM, /* IN endpoint */
	.bmAttributes = 0x02,       /* Bulk */
	.wMaxPacketSize = 64,       /* bytes */
	.bInterval = 0xff
};

/* Data RX Endpoint descriptor */
static USB_ENDP_DESCR rx_ep_desc = {
	.bLength = sizeof(USB_ENDP_DESCR),
	.bDescriptorType = USB_DESCR_TYP_ENDP,
	.bEndpointAddress = DATA_EP_NUM, /* OUT endpoint */
	.bmAttributes = 0x02,       /* Bulk */
	.wMaxPacketSize = 64,       /* bytes */
	.bInterval = 0xff
};

static void acm_if_handler(USB_SETUP_REQ * request)
{
	_TRACE();
	static uint8_t report_val, idle_val;
	uint8_t req = request->bRequest;
	uint16_t type = request->wValue >> 8;

	switch(req) {

	default:
		break;
	}
}

static void noti_ep_handler(USB_SETUP_REQ *request)
{
	_TRACE();

	uint8_t req = request->bRequest;
	switch(req) {
	case USB_CLEAR_FEATURE:
		PRINT("- USB_CLEAR_FEATURE\n");
		PRINT("wFeatureSelector: 0x%02x\n", request->wValue);
		if (request->wValue == 0) { // Endpoint halt
			prepare_handshake(NOTI_EP_NUM, ACK, 1, 0);
		}
		break;

	default:
		break;
	}
}

static int req_len;

static void data_ep_handler(USB_SETUP_REQ *request)
{
	_TRACE();
	static int tog;

	uint8_t token = R8_USB_INT_ST & MASK_UIS_TOKEN;
	switch(token) {
	case UIS_TOKEN_OUT:
		if (on_write)
			on_write(data_ep_out, R8_USB_RX_LEN);
		tog = !tog;
		prepare_handshake(DATA_EP_NUM, ACK, tog, 0);
		break;

	case UIS_TOKEN_IN:
		if (req_len > 0) { // FIXME: move to multiple transfer, here just testing
			req_len = 0;
		} else {
			prepare_handshake(DATA_EP_NUM, STALL, 1, 0);
		}
		break;
	
	default:
		break;
	}
}

void cdc_acm_tx(uint8_t *buf, uint8_t len)
{
	static int tog;
	memcpy(data_ep_in, buf, len);
	prepare_handshake(DATA_EP_NUM, ACK, tog, len);
	tog = !tog;
	req_len = len;
}

void cdc_onWrite(void (*cb)(uint8_t *buf, uint16_t len))
{
	on_write = cb;
}

void cdc_acm_init()
{
	cfg_desc_append(&acm_if_desc);
	cfg_desc_append(header_func_desc);
	cfg_desc_append(acm_func_desc);
	cfg_desc_append(callmgr_func_desc);

	cfg_desc_append(&noti_ep_desc);
	cfg_desc_append(&rx_ep_desc);
	cfg_desc_append(&tx_ep_desc);

	if_register(ACM_IF_NUM, acm_if_handler);
	ep_register(NOTI_EP_NUM, noti_ep_handler);
	ep_register(DATA_EP_NUM, data_ep_handler);

	dma_register(NOTI_EP_NUM, noti_ep_buf);
	dma_register(DATA_EP_NUM, data_ep_buf);
}