#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>

#include "CH58x_common.h" // FIXME: redesign headers

enum RESPONSE_TYPE {
	ACK = 0,
	NO_RESP,
	NAK,
	STALL,
};

typedef void (*ep_handler_t)(USB_SETUP_REQ *request);
typedef void (*if_handler_t)(USB_SETUP_REQ *request);
int ep_register(int ep_num, ep_handler_t handler);
int if_register(uint8_t if_num, if_handler_t handler);
void dma_register(uint8_t endpoint_number, void *buf_addr);

void set_handshake(uint8_t ep_num, int type, int tog, uint8_t len);
void ctrl_ack();

void ep_send(uint8_t ep_num, uint8_t len);

void cfg_desc_append(void *desc);
void usb_start();

void cdc_fill_IN(uint8_t *buf, uint8_t len);
int cdc_tx_poll(uint8_t *buf, int len, uint16_t timeout_ms);
void cdc_onWrite(void (*cb)(uint8_t *buf, uint16_t len));

void hiddev_fill_IN(uint8_t *buf, uint8_t len);
int hiddev_tx_poll(uint8_t *buf, int len, uint16_t timeout_ms);
void hiddev_onWrite(void (*cb)(uint8_t *buf, uint16_t len));

void usb_set_address(uint8_t ad);
void handle_devreq(USB_SETUP_REQ *request);
uint16_t usb_start_load_block(void *ep_in_buf, void *buf, uint16_t len, int tog);
void ctrl_start_load_block(void *buf, uint16_t len);
int usb_load_next_chunk();
void usb_flush();

void clear_handshake_sent_flag();
int handshake_sent();

#endif /* __USB_H__ */
