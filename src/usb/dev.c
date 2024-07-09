#include "CH58x_common.h"

#include "usb.h"
#include "debug.h"

extern USB_DEV_DESCR dev_desc;
extern uint8_t *cfg_desc;
extern const uint8_t lang_desc[];
extern const uint8_t vendor_info[];
extern const uint8_t product_info[];
extern ep_handler_t ep_handlers[];
extern if_handler_t if_handlers[];
extern uint8_t ep0buf[];

static void dev_getDesc(USB_SETUP_REQ *request)
{
	_TRACE();
	uint8_t type = request->wValue >> 8;
	uint8_t index = request->wValue & 0xff;
	PRINT("Descriptor type: 0x%02x\n", type);

	switch(type) {
	case USB_DESCR_TYP_DEVICE:
		PRINT("- USB_DESCR_TYP_DEVICE\n");
		ctrl_load_short_chunk(&dev_desc, dev_desc.bLength);
		break;

	case USB_DESCR_TYP_CONFIG:
		PRINT("- USB_DESCR_TYP_CONFIG\n");
		start_send_block(cfg_desc, request->wLength);
		break;

	case USB_DESCR_TYP_STRING:
		PRINT("- USB_DESCR_TYP_STRING\n");
		switch(index) {
		case 1:
			PRINT("- 1\n");
			ctrl_load_short_chunk(vendor_info, vendor_info[0]);
			break;
		case 2:
			PRINT("- 2\n");
			ctrl_load_short_chunk(product_info, product_info[0]);
			break;
		case 0:
			PRINT("- 0\n");
			ctrl_load_short_chunk(lang_desc, lang_desc[0]);
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

static void dev_getStatus(USB_SETUP_REQ *request)
{
	_TRACE();
	// Remote Wakeup disabled | Bus powered, hardcoded for now
	uint8_t buf[] = {0, 0};
	ctrl_load_short_chunk(buf, request->wLength);
}

static void dev_clearFeature()
{
	_TRACE();
	// DEVICE_REMOTE_WAKEUP and TEST_MODE are not available, ignore.
}

static void dev_setFeature()
{
	_TRACE();
	// DEVICE_REMOTE_WAKEUP and TEST_MODE are not available, ignore.
}

void handle_devreq(USB_SETUP_REQ *request)
{
	_TRACE();

	// FIXME: for now, multiple configuration is not supported
	static uint8_t devcfg;

	uint8_t req = request->bRequest;
	PRINT("bRequest: 0x%02x\n", req);

	switch(req) {
	case USB_GET_STATUS:
		PRINT("- USB_GET_STATUS\n");
		dev_getStatus(request);
		break;

	case USB_CLEAR_FEATURE:
		PRINT("- USB_CLEAR_FEATURE\n");
		dev_clearFeature();
		break;
	case USB_SET_FEATURE:
		PRINT("- USB_SET_FEATURE\n");
		dev_setFeature();
		break;

	case USB_SET_ADDRESS:
		PRINT("- USB_SET_ADDRESS\n");
		/* new address will be loadled in the next IN poll,
		so here just sending a ACK */
		set_address(request->wValue & 0xff);
		send_handshake(0, 1, ACK, 1, 0);
		break;

	case USB_GET_DESCRIPTOR:
		PRINT("- USB_GET_DESCRIPTOR\n");
		dev_getDesc(request);
		break;
	case USB_SET_DESCRIPTOR:
		PRINT("- USB_SET_DESCRIPTOR\n");
		// return dev_set_desc();
		break;
	
	case USB_GET_CONFIGURATION:
		PRINT("- USB_GET_CONFIGURATION\n");
		ctrl_load_short_chunk(&devcfg, 1);
		break;
	case USB_SET_CONFIGURATION:
		PRINT("- USB_SET_CONFIGURATION\n");
		devcfg = (request->wValue) & 0xff;
		send_handshake(0, 1, ACK, 1, 0);
		break;

	default:
		break;
	}	
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
