#include <stdint.h>

#include "usb.h"

__attribute__((aligned(4))) uint8_t ep_buf[64 + 64];      //ep3_out(64)+ep3_in(64)
#define ep_out      (ep_buf)
#define ep_in       (ep_buf + 64)

void DevEP3_OUT_Deal(uint8_t l) {
	uint8_t i;
	for(i = 0; i < l; i++) {
		ep_in[i] = ~ep_out
	[i];
	}
	usb_res_ack(3, l);
}

void cdc_init()
{
	usb_dma_config(3, ep_out); // TODO: not seem legit from this point of view
}