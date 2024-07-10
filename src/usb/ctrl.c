
#include "CH58x_common.h"

#include "usb.h"
#include "debug.h"

// A response a has been set, if not, that transaction hasn't been handled
int res_sent;

uint8_t *p_desc; // FIXME: resolve to local variable

//ep0(64)+ep4_out(64)+ep4_in(64)
__attribute__((aligned(4))) uint8_t ep0buf[64 + 64 + 64];

static volatile uint8_t *len_regs[] = {
	&R8_UEP0_T_LEN,
	&R8_UEP1_T_LEN,
	&R8_UEP2_T_LEN,
	&R8_UEP3_T_LEN,
	&R8_UEP4_T_LEN,
	&R8_UEP5_T_LEN,
	&R8_UEP6_T_LEN,
	&R8_UEP7_T_LEN
};
static volatile uint8_t *ctrl_regs[] = {
	&R8_UEP0_CTRL,
	&R8_UEP1_CTRL,
	&R8_UEP2_CTRL,
	&R8_UEP3_CTRL,
	&R8_UEP4_CTRL,
	&R8_UEP5_CTRL,
	&R8_UEP6_CTRL,
	&R8_UEP7_CTRL
};

// TODO: needs rework
void send_handshake(uint8_t ep_num, int dir, int type, int datax, uint8_t len)
{
	if (ep_num > 7)
		return;

	uint8_t ctrl = type << (dir ? 0 : 2);
	ctrl |= (datax) ? RB_UEP_T_TOG : RB_UEP_R_TOG;

	char *type_mean[] = { "ACK", "NAK", "STALL"};
	PRINT("Loaded %d byte, DATA%d with an %s to EP%dIN.\n",
			len, datax != 0, type_mean[type], ep_num);
	// FIXME: actually, when sending a respone with NAK or STALL, no length needed

	res_sent = 1;
	*len_regs[ep_num] = len;
	*ctrl_regs[ep_num] = ctrl;
}

void ctrl_send_nak()
{
	PRINT("Returning NAK\n");
	send_handshake(0, 1, NAK, 1, 0);
}

// TODO: create a function, manually toggle instead of specifying dataX

uint16_t ctrl_load_chunk(void *buf, uint16_t len2load, uint16_t req_len, int datax)
{
	_TRACE();
	uint16_t remain_len = 0;

	if (req_len > MAX_PACKET_SIZE)
		req_len = MAX_PACKET_SIZE;

	if (len2load >= req_len) {
		remain_len = len2load - req_len;
		len2load = req_len;
	}
	p_desc = buf;

	memcpy(ep0buf, p_desc, len2load);
	p_desc += len2load;

	send_handshake(0, 1, ACK, datax, len2load);

	PRINT("remain_len: %d\n", remain_len);
	return remain_len;
}

// TODO: ctrl_send_chunk_auto()

static void ctrl_load_short_chunk(void *buf, uint16_t len)
{
	_TRACE();
	ctrl_load_chunk(buf, len, len, 1);
}

void ep_send(uint8_t ep_num, uint8_t len)
{
	if (ep_num > 7) // CH582 support only 8-pair endpoint
		return;
	*len_regs[ep_num] = len;
	*ctrl_regs[ep_num] = (*ctrl_regs[ep_num] & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

void clear_handshake_sent_flag()
{
	res_sent = 0;
}

int handshake_sent()
{
	return res_sent;
}

extern uint8_t *cfg_desc;
static uint8_t address;

void set_address(uint8_t ad)
{
	address = ad;
}

static uint16_t remain_len, req_len, data_tog;

void start_send_block(void *buf, uint16_t len)
{
	req_len = len;
	remain_len = ctrl_load_chunk(buf, len, len, 1);
}

static void send_next_chunk()
{
	if (remain_len) {
		remain_len = ctrl_load_chunk(p_desc, remain_len, req_len, data_tog);
		data_tog = !data_tog;
	} else {
		data_tog = 0;
	}
}

static void ep_handler(USB_SETUP_REQ *request)
{
	_TRACE();

	uint8_t req = request->bRequest;
	PRINT("bRequest: 0x%02x (%s)\n", req, bRequest_parse(req));

	/* Each interface will have their own request handler */
	uint8_t recip = request->bRequestType & USB_REQ_RECIP_MASK;
	if (recip == USB_REQ_RECIP_INTERF) {
		handle_ifreq(request);
		return;
	}

	uint8_t token = R8_USB_INT_ST & MASK_UIS_TOKEN;
	switch(token) {

	case UIS_TOKEN_SETUP:
		if (recip == USB_REQ_RECIP_DEVICE) {
			handle_devreq(request);
		}
		PRINT("received a setup packet\n");
		break;

	case UIS_TOKEN_OUT:
		if (req == USB_CLEAR_FEATURE) {
			send_handshake(0, 1, ACK, 1, 0);
		}

	case UIS_TOKEN_IN:
		R8_USB_DEV_AD = address;
		send_next_chunk();
		break;
	
	default:
		break;
	}
}

void ctrl_init()
{
	ep_register(0, ep_handler);
	dma_register(0, ep0buf);
}