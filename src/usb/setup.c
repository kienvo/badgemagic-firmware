#include <stdlib.h>

#include "CH58x_common.h"

#include "usb.h"

USB_DEV_DESCR dev_desc = {
	.bLength = sizeof(USB_DEV_DESCR),
	.bDescriptorType = 0x01,
	.bcdUSB = 0x0110,

	.bDeviceClass = 0x00,
	.bDeviceSubClass = 0x00,
	.bDeviceProtocol = 0x00,

	.bMaxPacketSize0 = MAX_PACKET_SIZE,

	.idVendor = 0x0416,
	.idProduct = 0x5020,
	.bcdDevice = 0x0000,
	.iManufacturer = 1, // TODO: update strings
	.iProduct = 2,
	.iSerialNumber = 0x00,
	.bNumConfigurations = 0x01
};

// /* Qualifier - only required on High-Speed usb described in Device Descriptor */
// const uint8_t qual_desc[] = {
// 	0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00
// };

// /* USB speed mode -
// only required on High-Speed usb described in Device Descriptor */
// uint8_t USB_FS_OSC_DESC[sizeof(cfg_desc)] = {
// 	0x09, 0x07, /* others missing parts will be added on runtime */
// };

/* Configuration Descriptor template */
USB_CFG_DESCR cfg_static = {
	.bLength = sizeof(USB_CFG_DESCR),
	.bDescriptorType = 0x02,
	.wTotalLength = sizeof(USB_CFG_DESCR), // will be updated on cfg_desc_add()
	.bNumInterfaces = 0, // will be updated on cfg_desc_add()
	.bConfigurationValue = 0x01,
	.iConfiguration = 0x00, // TODO: add get_string
	.bmAttributes = 0xA0,
	.MaxPower = 50 // mA
};

uint8_t *cfg_desc; // FIXME:

 // FIXME: change to ASCII
uint8_t lang_desc[] = {0x04, 0x03, 0x09, 0x04};
uint8_t vendor_info[] = {0x0E, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0};
uint8_t product_info[] = {0x0C, 0x03, 'C', 0, 'H', 0, '5', 0, '7', 0, 'x', 0};

if_handler_t if_handlers[256]; // FIXME: here wasting 1KiB of ram
ep_handler_t ep_handlers[7];

void cfg_desc_append(void *desc)
{
	uint8_t cfg_len = 0;
	uint8_t len = ((uint8_t *)desc)[0];
	if (cfg_desc) // attempting to add a non-Configuration Descriptor
		cfg_len = ((USB_CFG_DESCR *)cfg_desc)->wTotalLength;
	uint8_t newlen = cfg_len + len;

	cfg_desc = realloc(cfg_desc, newlen); // TODO: add safe check here

	memcpy(cfg_desc + cfg_len, desc, len);

	((USB_CFG_DESCR *)cfg_desc)->wTotalLength = newlen;
	((USB_CFG_DESCR *)cfg_desc)->bNumInterfaces += ((uint8_t *)desc)[1] == 0x04;
}

int ep_register(int ep_num, ep_handler_t handler)
{
	if (ep_num > 8)
		return -1;
	if (ep_handlers[ep_num]) // already registerd
		return -1;

	ep_handlers[ep_num] = handler;
	return 0;
}

// TODO: rename to 'requests_register'
int if_register(uint8_t if_num, if_handler_t handler)
{
	if (if_handlers[if_num]) // already registered
		return -1;

	if_handlers[if_num] = handler;
	return 0;
}

void dma_register(uint8_t ep_num, void *buf)
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
	// the next ep0
	if (ep_num != 4)
		*p_regs[ep_num] = (uint16_t)(uint32_t)buf;

	*ctrl_regs[ep_num] = UEP_R_RES_ACK | UEP_T_RES_NAK;
}

static void init(void)
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
	R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
	R16_PIN_ANALOG_IE |= RB_PIN_USB_IE | RB_PIN_USB_DP_PU;
	R8_USB_INT_FG = 0xFF;
	R8_UDEV_CTRL = RB_UD_PD_DIS | RB_UD_PORT_EN;
	R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;
}

void mouse_init();
void key_init();
void ctrl_init();
void hiddev_init();

void usb_start() {
	cfg_desc_append(&cfg_static);

	ctrl_init();

	/* This should be placed first, the python script always looks
	for the first interface (not the interface number) */
	hiddev_init();

	key_init();
	mouse_init();

	init();
	PFIC_EnableIRQ(USB_IRQn);
}