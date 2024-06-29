#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>

#include "CH58x_common.h" // FIXME: redesign headers

enum RESPONSE_TYPE {
	ACK = 0,
	NAK,
	STALL,
};

typedef void (*ep_handler_t)(USB_SETUP_REQ *request);
typedef void (*if_handler_t)(USB_SETUP_REQ *request);
int ep_register(int ep_num, ep_handler_t handler);
int if_register(uint8_t if_num, if_handler_t handler);
void dma_register(uint8_t endpoint_number, void *buf_addr);

void clear_handshake_sent_flag();
int handshake_sent();
void send_handshake(uint8_t ep_num, int dir, int type, int toggle, uint8_t len);
void ctrl_send_nak();
void ctrl_load_short_chunk(void *buf, uint16_t len);

void ep_send(uint8_t ep_num, uint8_t len);

void cfg_desc_append(void *desc);
void usb_start();
void key_hidReport(uint8_t key);

#endif /* __USB_H__ */
