#include "CH58x_common.h"

#include "usb.h"

#define EP0_SIZE    0x40

const uint8_t dev_desc[] = {
	0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, EP0_SIZE, 0x3d, 0x41, 0x07, 0x21, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01
};

#define KEY_EP_ADDR_IN     (0x80 | 4)
#define MOUSE_EP_ADDR_IN   (0x80 | 2)
#define MYSTERO_EP_ADDR_IN   (0x80 | 3)

const uint8_t cfg_desc[] = {
	0x09, 0x02, 0x3b, 0x00, 0x02, 0x01, 0x00, 0xA0, 0x32, // configuration

	0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00, // interface, keyboard
	0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3e, 0x00, // HID class
	0x07, 0x05, KEY_EP_ADDR_IN, 0x03, 0x08, 0x00, 0x0a,      // endpoint

	0x09, 0x04, 0x01, 0x00, 0x01, 0x03, 0x01, 0x02, 0x00, // interface, mouse
	0x09, 0x21, 0x10, 0x01, 0x00, 0x01, 0x22, 0x34, 0x00, // HID class
	0x07, 0x05, MOUSE_EP_ADDR_IN, 0x03, 0x04, 0x00, 0x0a     // endpoint
};

/* Qualifier */
const uint8_t qual_desc[] = {
	0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00
};

/* USB speed mode */
uint8_t USB_FS_OSC_DESC[sizeof(cfg_desc)] = {
	0x09, 0x07, /* others missing parts will be added by library */
};

const uint8_t lang_desc[] = {0x04, 0x03, 0x09, 0x04};
const uint8_t vendor_info[] = {0x0E, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0};
const uint8_t product_info[] = {0x0C, 0x03, 'C', 0, 'H', 0, '5', 0, '7', 0, 'x', 0};

uint8_t        req;
uint16_t       req_len;
const uint8_t *p_desc;
uint8_t        sleep_status = 0x00; /* USB睡眠状态 */

__attribute__((aligned(4))) uint8_t ep0buf[64 + 64 + 64]; //ep0(64)+ep4_out(64)+ep4_in(64)
#define setup_req          ((PUSB_SETUP_REQ)ep0buf)

// anlyze tokens and enpoint number
void handle_token()
{
	uint8_t len;

	switch(R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) {
	case UIS_TOKEN_IN: // in 0
		switch(req) {
		case USB_GET_DESCRIPTOR:
			len = req_len >= EP0_SIZE ? EP0_SIZE : req_len; // 本次传输长度
			memcpy(ep0buf, p_desc, len);                          /* 加载上传数据 */
			req_len -= len;
			p_desc += len;
			R8_UEP0_T_LEN = len;
			R8_UEP0_CTRL ^= RB_UEP_T_TOG; // 翻转
			return;
		case USB_SET_ADDRESS:
			R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | req_len;
			R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
			return;

		case USB_SET_FEATURE:
			return;

		default:
			R8_UEP0_T_LEN = 0; // 状态阶段完成中断或者是强制上传0长度数据包结束控制传输
			R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
			return;
		}
		return;

	case UIS_TOKEN_OUT: // out 0
		len = R8_USB_RX_LEN;
		if(req == 0x09) {
			PRINT("[%s] Num Lock\t", (ep0buf[0] & (1<<0)) ? "*" : " ");
			PRINT("[%s] Caps Lock\t", (ep0buf[0] & (1<<1)) ? "*" : " ");
			PRINT("[%s] Scroll Lock\n", (ep0buf[0] & (1<<2)) ? "*" : " ");
		}
		return;

	case UIS_TOKEN_OUT | 1: // out 1
		// if(R8_USB_INT_ST & RB_UIS_TOG_OK) { // 不同步的数据包将丢弃
		// 	R8_UEP1_CTRL ^= RB_UEP_R_TOG;
		// 	len = R8_USB_RX_LEN;
		// 	DevEP1_OUT_Deal(len);
		// }
		return;

	case UIS_TOKEN_IN | 1: // in 1
		R8_UEP1_CTRL ^= RB_UEP_T_TOG;
		R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
		return;

	case UIS_TOKEN_OUT | 2: // out 2
		// if(R8_USB_INT_ST & RB_UIS_TOG_OK) { // 不同步的数据包将丢弃
		// 	R8_UEP2_CTRL ^= RB_UEP_R_TOG;
		// 	len = R8_USB_RX_LEN;
		// 	DevEP2_OUT_Deal(len);
		// }
		return;

	case UIS_TOKEN_IN | 2: // in 2
		R8_UEP2_CTRL ^= RB_UEP_T_TOG;
		R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
		return;

	case UIS_TOKEN_OUT | 3: // out 3
		// if(R8_USB_INT_ST & RB_UIS_TOG_OK) { // 不同步的数据包将丢弃
		// 	R8_UEP3_CTRL ^= RB_UEP_R_TOG;
		// 	len = R8_USB_RX_LEN;
		// 	DevEP3_OUT_Deal(len);
		// }
		return;

	case UIS_TOKEN_IN | 3: // in 3
		R8_UEP3_CTRL ^= RB_UEP_T_TOG;
		R8_UEP3_CTRL = (R8_UEP3_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
		return;

	case UIS_TOKEN_OUT | 4: // out 4
		// if(R8_USB_INT_ST & RB_UIS_TOG_OK) {
		// 	R8_UEP4_CTRL ^= RB_UEP_R_TOG;
		// 	// len = R8_USB_RX_LEN;
		// 	// DevEP4_OUT_Deal(len);
		// }
		return;

	case UIS_TOKEN_IN | 4: // in 4
		R8_UEP4_CTRL ^= RB_UEP_T_TOG;
		R8_UEP4_CTRL = (R8_UEP4_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
		return;

	default:
		return;
	}
}

static uint8_t onGetDesc()
{
	uint8_t errflag = 0, len = 0;
	uint16_t val = setup_req->wValue;
	uint16_t n_interface = setup_req->wIndex & 0xff;

	switch((val >> 8)) {
	case USB_DESCR_TYP_DEVICE:
		p_desc = dev_desc;
		len = dev_desc[0];
		break;

	case USB_DESCR_TYP_CONFIG:
		p_desc = cfg_desc;
		len = cfg_desc[2];
		break;

	case USB_DESCR_TYP_HID:
		/* interfaces */
		switch(n_interface) {
		case 0:
			p_desc = (uint8_t *)(&cfg_desc[18]);
			len = 9;
			break;

		case 1:
			p_desc = (uint8_t *)(&cfg_desc[43]);
			len = 9;
			break;

		default:
			errflag = 0xff;
			break;
		}
		break;

	case USB_DESCR_TYP_REPORT:
		// Interface 0 report descriptor
		if((n_interface) == 0) {
			p_desc = KeyRepDesc;
			len = KeyRepDesc_len;
		}

		// Interface 1 report descriptor
		else if((n_interface) == 1) {
			p_desc = MouseRepDesc;
			len = MouseRepDesc_len;
		} else
			len = 0xff; // This program only has 2 interfaces. This sentence cannot be executed normally.
		break;

	case USB_DESCR_TYP_STRING:
		switch(val & 0xff) {
		case 1:
			p_desc = vendor_info;
			len = vendor_info[0];
			break;
		case 2:
			p_desc = product_info;
			len = product_info[0];
			break;
		case 0:
			p_desc = lang_desc;
			len = lang_desc[0];
			break;
		default:
			errflag = 0xFF;
			break;
		}
		break;

	case USB_DESCR_TYP_QUALIF:
		p_desc = (uint8_t *)(&qual_desc[0]);
		len = sizeof(qual_desc);
		break;

	case USB_DESCR_TYP_SPEED:
		memcpy(&USB_FS_OSC_DESC[2], &cfg_desc[2], sizeof(cfg_desc) - 2);
		p_desc = (uint8_t *)(&USB_FS_OSC_DESC[0]);
		len = sizeof(USB_FS_OSC_DESC);
		break;

	default:
		errflag = 0xff;
		break;
	}
	if(req_len > len)
		req_len = len; // Actual total length to be sent
	len = (req_len >= EP0_SIZE) ? EP0_SIZE : req_len;
	memcpy(ep0buf, p_desc, len);
	p_desc += len;
	return errflag;
}

static uint8_t set_feature()
{
	uint8_t req_type = setup_req->bRequestType;
	if((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
		/* Enpoints */
		switch(setup_req->wIndex) {
		case MYSTERO_EP_ADDR_IN:
			R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
			break;
		case 0x03:
			R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
			break;
		case MOUSE_EP_ADDR_IN:
			R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
			break;
		case 0x02:
			R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
			break;
		case KEY_EP_ADDR_IN:
			R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL;
			break;
		case 0x01:
			R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL;
			break;

		/* unsupported endpoints */
		default:
			return 0xff;
		}
		return 0;
	}

	if((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
		if(setup_req->wValue == 1) {
			/* 设置睡眠 */
			sleep_status |= 0x01;
		}
		return 0;
	}
	return 0xFF;
}

static uint8_t clear_feature()
{
	uint8_t req_type = setup_req->bRequestType;
	if((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
		switch((setup_req->wIndex) & 0xff) {
		case MYSTERO_EP_ADDR_IN:
			R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
			break;
		case 0x03:
			R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
			break;
		case MOUSE_EP_ADDR_IN:
			R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
			break;
		case 0x02:
			R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
			break;
		case KEY_EP_ADDR_IN:
			R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
			break;
		case 0x01:
			R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
			break;
		default:
			return 0xFF;
		}
		return 0;
	}

	if((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
		if(setup_req->wValue == 1) {
			sleep_status &= ~0x01;
		}
		return 0;
	}
	return 0xff;
}

static uint8_t get_status()
{
	uint8_t req_type = setup_req->bRequestType;
	if((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
		ep0buf[0] = 0x00;
		switch(setup_req->wIndex) {
		case MYSTERO_EP_ADDR_IN:
			if((R8_UEP3_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL) {
				ep0buf[0] = 0x01;
			}
			break;

		case 0x03:
			if((R8_UEP3_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL) {
				ep0buf[0] = 0x01;
			}
			break;

		case MOUSE_EP_ADDR_IN:
			if((R8_UEP2_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL) {
				ep0buf[0] = 0x01;
			}
			break;

		case 0x02:
			if((R8_UEP2_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL) {
				ep0buf[0] = 0x01;
			}
			break;

		case KEY_EP_ADDR_IN:
			if((R8_UEP1_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL) {
				ep0buf[0] = 0x01;
			}
			break;

		case 0x01:
			if((R8_UEP1_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL) {
				ep0buf[0] = 0x01;
			}
			break;
		}
	}
	else if((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
		ep0buf[0] = 0x00;
		if(sleep_status) {
			ep0buf[0] = 0x02;
		} else {
			ep0buf[0] = 0x00;
		}
	}
	ep0buf[1] = 0;
	if(req_len >= 2) {
		req_len = 2;
	}
	return 0;
}

/* standard request */
uint8_t std_req()
{
	static uint8_t devcfg;

	switch(req) {
	case USB_GET_DESCRIPTOR:
		return onGetDesc();

	case USB_SET_ADDRESS:
		req_len = (setup_req->wValue) & 0xff;
		return 0;

	case USB_GET_CONFIGURATION:
		ep0buf[0] = devcfg;
		if(req_len > 1)
			req_len = 1;
		return 0;

	case USB_SET_CONFIGURATION:
		devcfg = (setup_req->wValue) & 0xff;
		return 0;

	case USB_CLEAR_FEATURE:
		return clear_feature();

	case USB_SET_FEATURE:
		return set_feature();
	
	case USB_GET_INTERFACE:
		ep0buf[0] = 0x00;
		if(req_len > 1)
			req_len = 1;
		return 0;

	case USB_SET_INTERFACE:
		return 0;

	case USB_GET_STATUS:
		return get_status();

	default:
		return 0xff;
	}
	return 0;
}

void onBusReset()
{
	R8_USB_DEV_AD = 0;
	R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
	R8_USB_INT_FG = RB_UIF_BUS_RST;
}

void onPowerChange()
{
	if(R8_USB_MIS_ST & RB_UMS_SUSPEND) {
		;
	} // hang
	else {
		;
	} // wake
	R8_USB_INT_FG = RB_UIF_SUSPEND;
}

static void handle_setup()
{
	R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
	req_len = setup_req->wLength;
	req = setup_req->bRequest;
	uint8_t reqtype = setup_req->bRequestType;
	uint8_t len = 0;

	// If a standard request
	if((reqtype & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
		uint8_t errflag = std_req();
		if(errflag == 0xff) { // Error or not supported
			R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
			R8_USB_INT_FG = RB_UIF_TRANSFER; // clear flag
			return;
		}
	} else { //If a Non standard request
		if(reqtype & USB_REQ_TYP_VENDOR) {
			/* Vendor request */
		}
		else if(reqtype & USB_REQ_TYP_CLASS) {
			static uint8_t report_val, idle_val;
			switch(req) {
			case DEF_USB_SET_IDLE:
				idle_val = ep0buf[3]; // This must be 3 ??
				break;

			case DEF_USB_SET_REPORT:
				break;

			case DEF_USB_SET_PROTOCOL:
				report_val = ep0buf[2];
				break;

			case DEF_USB_GET_IDLE:
				ep0buf[0] = idle_val;
				len = 1;
				break;

			case DEF_USB_GET_PROTOCOL:
				ep0buf[0] = report_val;
				len = 1;
				break;

			default:
				R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
				R8_USB_INT_FG = RB_UIF_TRANSFER; // clear flag
				return;
			}
		}
	}

	if(reqtype & USB_REQ_TYP_IN) { // device to host
		len = (req_len > EP0_SIZE) ? EP0_SIZE : req_len;
		req_len -= len;
	} else // host to device
		len = 0;

	R8_UEP0_T_LEN = len;
	R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; // 默认数据包是DATA1
	R8_USB_INT_FG = RB_UIF_TRANSFER; // clear flag
	return;
}

// USB response handler
void USB_DevTransProcess(void) {
	uint8_t intflag = R8_USB_INT_FG;

	if(intflag & RB_UIF_TRANSFER) {

		// Is received PID a TOKEN
		if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN) {
			handle_token();
			R8_USB_INT_FG = RB_UIF_TRANSFER; // clear flag
			return;
		}

		// If received a setup request
		if(R8_USB_INT_ST & RB_UIS_SETUP_ACT) {
			handle_setup();
			return;
		}
	}

	if(intflag & RB_UIF_BUS_RST) { // Bus reset
		onBusReset();
		return;
	}
	if(intflag & RB_UIF_SUSPEND) { // USB suspend or resume
		onPowerChange();
		return;
	}
	R8_USB_INT_FG = intflag; // clear interrupt flags
}

void usb_dma_config(uint8_t ep_num, void *buf)
{
	if (ep_num > 7) // CH582 support only 8 endpoint
		return;

	volatile uint16_t *p_regs[] = {
		&R16_UEP0_DMA,
		&R16_UEP1_DMA,
		&R16_UEP2_DMA,
		&R16_UEP3_DMA,
		NULL,
		&R16_UEP5_DMA,
		&R16_UEP6_DMA,
		&R16_UEP7_DMA,
	};
	volatile uint8_t *ctrl_regs[] = {
		&R8_UEP0_CTRL,
		&R8_UEP1_CTRL,
		&R8_UEP2_CTRL,
		&R8_UEP3_CTRL,
		&R8_UEP4_CTRL,
		&R8_UEP5_CTRL,
		&R8_UEP6_CTRL,
		&R8_UEP7_CTRL,
	};
	// ep4's dma buffer is hardcoded pointing to 
	// the next ep0 (?)
	if (ep_num != 4)
		*p_regs[ep_num] = (uint16_t)(uint32_t)buf;

	*ctrl_regs[ep_num] = UEP_R_RES_ACK | UEP_T_RES_NAK;
				// | (ep_num == 0) ? 0 : RB_UEP_AUTO_TOG;
}

void usb_res_ack(uint8_t ep_num, uint8_t len)
{
	if (ep_num > 7) // CH582 support only 8 endpoint
		return;
	volatile uint8_t *ctrl_regs[] = {
		&R8_UEP0_CTRL,
		&R8_UEP1_CTRL,
		&R8_UEP2_CTRL,
		&R8_UEP3_CTRL,
		&R8_UEP4_CTRL,
		&R8_UEP5_CTRL,
		&R8_UEP6_CTRL,
		&R8_UEP7_CTRL
	};
	volatile uint8_t *len_regs[] = {
		&R8_UEP0_T_LEN,
		&R8_UEP1_T_LEN,
		&R8_UEP2_T_LEN,
		&R8_UEP3_T_LEN,
		&R8_UEP4_T_LEN,
		&R8_UEP5_T_LEN,
		&R8_UEP6_T_LEN,
		&R8_UEP7_T_LEN
	};
	*len_regs[ep_num] = len;
	*ctrl_regs[ep_num] = (*ctrl_regs[ep_num] & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

void USB_DeviceInit(void)
{
	R8_USB_CTRL = 0x00;

	R8_UEP4_1_MOD = RB_UEP4_RX_EN | RB_UEP4_TX_EN |
				RB_UEP1_RX_EN | RB_UEP1_TX_EN;
	R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN |
				RB_UEP3_RX_EN | RB_UEP3_TX_EN;
	R8_UEP567_MOD = RB_UEP7_RX_EN | RB_UEP7_TX_EN |
				RB_UEP6_RX_EN | RB_UEP6_TX_EN |
				RB_UEP5_RX_EN | RB_UEP5_TX_EN;

	R8_USB_DEV_AD = 0x00;
	R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN; // 启动USB设备及DMA，在中断期间中断标志未清除前自动返回NAK
	R16_PIN_ANALOG_IE |= RB_PIN_USB_IE | RB_PIN_USB_DP_PU;         // 防止USB端口浮空及上拉电阻
	R8_USB_INT_FG = 0xFF;                                          // 清中断标志
	R8_UDEV_CTRL = RB_UD_PD_DIS | RB_UD_PORT_EN;                   // 允许USB端口
	R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;
}

void mouse_init();
void key_init();

void usb_start() {
	usb_dma_config(0, ep0buf);
	mouse_init();
	key_init();

	USB_DeviceInit();
	PFIC_EnableIRQ(USB_IRQn);
}

__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void) {
	USB_DevTransProcess();
}
