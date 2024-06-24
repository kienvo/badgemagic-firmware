#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>

/* HID request types */
#define DEF_USB_GET_IDLE           0x02 /* get idle for key or mouse */
#define DEF_USB_GET_PROTOCOL       0x03 /* get protocol for bios */
#define DEF_USB_SET_REPORT         0x09 /* set report for key */
#define DEF_USB_SET_IDLE           0x0A /* set idle for key or mouse */
#define DEF_USB_SET_PROTOCOL       0x0B /* set protocol for bios */

extern uint8_t ep0buf[]; //ep0(64)+ep4_out(64)+ep4_in(64)
extern uint8_t ep3buf[]; //ep3_out(64)+ep3_in(64)

void usb_res_ack(uint8_t ep_num, uint8_t len);
void usb_dma_config(uint8_t endpoint_number, void *buf_addr);

extern const uint8_t MouseRepDesc[];
extern const uint8_t MouseRepDesc_len;
extern uint8_t HIDMouse[];

extern const uint8_t KeyRepDesc[];
extern const uint8_t KeyRepDesc_len;
extern uint8_t HIDKey[];

void usb_start();
void key_hidReport(uint8_t key);

#endif /* __USB_H__ */
