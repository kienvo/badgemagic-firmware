#include "CH58x_common.h"

#include "usb.h"
#include "debug.h"

extern USB_DEV_DESCR dev_desc;
extern uint8_t *cfg_desc;
extern ep_handler_t ep_handlers[];
extern if_handler_t if_handlers[];

extern uint8_t ep0buf[];
extern uint8_t *string_index[];

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
	uint8_t index = request->wValue & 0xff;
	if (index > 4)
		return;
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

void handle_ifreq(USB_SETUP_REQ *request)
{
	_TRACE();

	uint8_t ifn = request->wIndex & 0xff;
	PRINT("wInterfaceNumber: 0x%02x\n", ifn);

	if (if_handlers[ifn])
		if_handlers[ifn](request);
}

static void handle_endpoints(USB_SETUP_REQ *request)
{
	_TRACE();
	usb_status_reg_parse(R8_USB_INT_ST);

	/* Workaround for getting a setup packet but EP is 0x02 for unknown reason.
	This happens when return from the bootloader or after a sotfware reset. */
	uint8_t token = R8_USB_INT_ST & MASK_UIS_TOKEN;
	if (token == UIS_TOKEN_SETUP) {
		ep_handlers[0](request);
	}

	uint8_t ep_num = R8_USB_INT_ST & MASK_UIS_ENDP;

	if (ep_num < 8 && ep_handlers[ep_num])
		ep_handlers[ep_num](request);
}

static void handle_busReset()
{
	_TRACE();
	R8_USB_DEV_AD = 0;
	R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP4_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP5_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP6_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP7_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
}

static void handle_powerChange()
{
	_TRACE();
	if (R8_USB_MIS_ST & RB_UMS_SUSPEND) {
		;// suspend
	}
	else {
		;// resume
	}
}

__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void) {
	uint8_t intflag = R8_USB_INT_FG;
	clear_handshake_sent_flag();
	PRINT("\nusb: new interrupt\n");
	print_intflag_reg();

	if (intflag & RB_UIF_TRANSFER) {
		PRINT("usb: RX Length reg: %d\n", R8_USB_RX_LEN);
		print_status_reg();

		USB_SETUP_REQ *req = (USB_SETUP_REQ *)ep0buf;
		print_setuppk(req);

		handle_endpoints(req);
	}
	else if (intflag & RB_UIF_BUS_RST) {
		handle_busReset();
	}
	else if (intflag & RB_UIF_SUSPEND) {
		handle_powerChange();
	}

	if (handshake_sent() == 0) {
		PRINT("WARN: This transaction is being IGNORED!\n");
	}
	R8_USB_INT_FG = intflag; // clear interrupt flags
}
