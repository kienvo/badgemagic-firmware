#include "CH58x_common.h"

#include "usb.h"
#include "debug.h"

extern USB_DEV_DESCR dev_desc;
extern uint8_t *cfg_desc;
extern uint8_t ep0buf[];

/* String Descriptor Zero, Specifying Languages Supported by the Device */
static uint8_t lang_desc[] = {
	0x04,       /* bLength */
	0x03,       /* bDescriptorType */
	0x09, 0x04  /* wLANGID - en-US */
};

static uint8_t vendor_info[] = {
	0x0E, /* bLength */
	0x03, /* bDescriptorType */

	/* bString */
	'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0
};

static uint8_t product_info[] = {
	0x0C, /* bLength */
	0x03, /* bDescriptorType */

	/* bString */
	'C', 0, 'H', 0, '5', 0, '7', 0, 'x', 0
};

static uint8_t serial_number[] = {
	106, /* bLength */
	0x03, /* bDescriptorType */

	/* bString */
	'T', 0, 'h', 0, 'i', 0, 's', 0, ' ', 0, 'i', 0, 's', 0, ' ', 0, 't', 0, 
	'h', 0, 'e', 0, ' ', 0, 's', 0, 'e', 0, 'r', 0, 'i', 0, 'a', 0, 'l', 0, 
	' ', 0, 's', 0, 't', 0, 'r', 0, 'i', 0, 'n', 0, 'g', 0, ' ', 0,

	'T', 0, 'h', 0, 'i', 0, 's', 0, ' ', 0, 'i', 0, 's', 0, ' ', 0, 't', 0, 
	'h', 0, 'e', 0, ' ', 0, 's', 0, 'e', 0, 'r', 0, 'i', 0, 'a', 0, 'l', 0, 
	' ', 0, 's', 0, 't', 0, 'r', 0, 'i', 0, 'n', 0, 'g', 0, ' ', 0
};

static void desc_dev(USB_SETUP_REQ *request)
{
	start_send_block(&dev_desc, dev_desc.bLength);
}

static void desc_config(USB_SETUP_REQ *request)
{
	start_send_block(cfg_desc, request->wLength);
}

static void desc_string(USB_SETUP_REQ *request)
{
	uint8_t *string_index[32] = {
		lang_desc,
		vendor_info,
		product_info,
		serial_number
	};
	uint8_t index = request->wValue & 0xff;
	if (index <= sizeof(string_index))
		start_send_block(string_index[index], string_index[index][0]);
}

static void dev_getDesc(USB_SETUP_REQ *request)
{
	_TRACE();
	uint8_t type = request->wValue >> 8;
	
	PRINT("Descriptor type: 0x%02x\n", type);

	static const void (*desc_type_handlers[4])(USB_SETUP_REQ *request) = {
		NULL,
		desc_dev,
		desc_config,
		desc_string
	};
	if (type <= 3 && desc_type_handlers[type])
		desc_type_handlers[type](request);
}

static void dev_getStatus(USB_SETUP_REQ *request)
{
	_TRACE();
	// Remote Wakeup disabled | Bus powered, hardcoded for now
	uint8_t buf[] = {0, 0};
	start_send_block(buf, 2);
}

static void dev_clearFeature(USB_SETUP_REQ *request)
{
	_TRACE();
	// DEVICE_REMOTE_WAKEUP and TEST_MODE are not available, ignore.
}

static void dev_setFeature(USB_SETUP_REQ *request)
{
	_TRACE();
	// DEVICE_REMOTE_WAKEUP and TEST_MODE are not available, ignore.
}

static void dev_setAddress(USB_SETUP_REQ *request)
{
	_TRACE();
	/* new address will be loadled in the next IN poll,
	so here just sending a ACK */
	set_address(request->wValue & 0xff);
	send_handshake(0, 1, ACK, 1, 0);
}

// FIXME: for now, multiple configuration is not supported
static uint8_t devcfg;

static void dev_getConfig(USB_SETUP_REQ *request)
{
	_TRACE();
	start_send_block(&devcfg, 1);
}

static void dev_setConfig(USB_SETUP_REQ *request)
{
	_TRACE();
	devcfg = (request->wValue) & 0xff;
	send_handshake(0, 1, ACK, 1, 0);
}

void handle_devreq(USB_SETUP_REQ *request)
{
	_TRACE();

	uint8_t req = request->bRequest;
	PRINT("bRequest: 0x%02x\n", req);

	static const void (*dev_req_handlers[13])(USB_SETUP_REQ *request) = {
		dev_getStatus,
		dev_clearFeature,
		NULL, // Reserved
		dev_setFeature,
		NULL, // Reserved
		dev_setAddress,
		dev_getDesc,
		NULL, // set desc
		dev_getConfig,
		dev_setConfig,
		NULL, // get interface
		NULL, // set interface
		NULL, // sync frame
	};

	if (req <= 12 && dev_req_handlers[req])
		dev_req_handlers[req](request);
}
