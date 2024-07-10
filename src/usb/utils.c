#include "CH58x_common.h"

#include "usb.h"
#include "debug.h"

// A response a has been set, if not, that transaction hasn't been handled
int res_sent;
static uint8_t *current_in_buf;

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

static void ctrl_send_nak()
{
	PRINT("Returning NAK\n");
	send_handshake(0, 1, NAK, 1, 0);
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

static uint8_t *p_buf;
static uint16_t remain_len, req_len, _tog;

static uint16_t 
load_chunk(void *ep_buf, void *block, uint16_t block_len, uint16_t req_len, int datax)
{
	_TRACE();
	uint16_t remain_len = 0;

	if (req_len > MAX_PACKET_SIZE)
		req_len = MAX_PACKET_SIZE;

	if (block_len >= req_len) {
		remain_len = block_len - req_len;
		block_len = req_len;
	}
	p_buf = block;

	memcpy(ep_buf, p_buf, block_len);
	p_buf += block_len;

	send_handshake(0, 1, ACK, datax, block_len);

	PRINT("remain_len: %d\n", remain_len);
	return remain_len;
}

void usb_flush()
{
	remain_len = 0;
	_tog = 0;
}

void usb_load_next_chunk()
{
	if (remain_len) {
		remain_len = load_chunk(current_in_buf, p_buf, remain_len, req_len, _tog);
		_tog = !_tog;
	} else {
		_tog = 0;
	}
}

void usb_start_load_block(void *ep_in_buf, void *buf, uint16_t len, int tog)
{
	req_len = len;
	current_in_buf = ep_in_buf;
	remain_len = load_chunk(ep_in_buf, buf, len, len, tog);
	_tog = !tog;
}
