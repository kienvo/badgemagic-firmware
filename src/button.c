#include "button.h"

#define DEBOUNCE_HIGH_THRES (200)
#define DEBOUNCE_LOW_THRES  (55)
#define LONGPRESS_THRES     (50)

#define IS_INRANGE(v, start, stop) ((v) > (start) && (v) < (stop))

static void do_nothing() {}

static volatile void (*onePressHandler[KEY_INDEX])(void);
static volatile void (*longPressHandler[KEY_INDEX])(void);

/**
 * Note: Always call btn_init() before setting up handlers.
 * Otherwise, handlers will be overrided by default (does nothing).
 * 
 */
void btn_init()
{
	GPIOA_ModeCfg(KEY1_PIN, GPIO_ModeIN_PD);
	GPIOB_ModeCfg(KEY2_PIN, GPIO_ModeIN_PU);

	for (int i=0; i<KEY_INDEX; i++) {
		onePressHandler[i] = do_nothing;
	}
	for (int i=0; i<KEY_INDEX; i++) {
		longPressHandler[i] = do_nothing;
	}

	TMR3_TimerInit(FREQ_SYS/100); // 1s/100
	TMR3_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
	PFIC_EnableIRQ(TMR3_IRQn);
}

void btn_onOnePress(int key, void (*handler)(void))
{
	if (key >= KEY_INDEX) return;
	onePressHandler[key] = handler;
}

void btn_onLongPress(int key, void (*handler)(void))
{
	if (key >= KEY_INDEX) return;
	longPressHandler[key] = handler;
}

static int debounce(int key, int is_press)
{
	static int y[KEY_INDEX], flag[KEY_INDEX];
	if (key >= KEY_INDEX) return 0;
	
	// RC filter
	y[key] -=  y[key] >> 2;
	y[key] += is_press ? 0x3F : 0;

	// Schmitt trigger
	if ((y[key] > DEBOUNCE_HIGH_THRES) && (flag[key] == 0))
		flag[key] = 1;
	if ((y[key] < DEBOUNCE_LOW_THRES) && (flag[key] == 1))
		flag[key] = 0;

	return flag[key];
}

__INTERRUPT
__HIGH_CODE
void TMR3_IRQHandler(void)
{
	static int hold[KEY_INDEX], is_longpress[KEY_INDEX];

	if (TMR3_GetITFlag(TMR0_3_IT_CYC_END)) {		
		if (debounce(KEY1, isPressed(KEY1))) {
			hold[KEY1]++;
			if (hold[KEY1] >= LONGPRESS_THRES && is_longpress[KEY1] == 0) {
				is_longpress[KEY1] = 1;
				longPressHandler[KEY1]();
			}
		} else {
			if (IS_INRANGE(hold[KEY1], 0, LONGPRESS_THRES)) {
				onePressHandler[KEY1]();
			}
			is_longpress[KEY1] = 0;
			hold[KEY1] = 0;
		}

		if (debounce(KEY2, isPressed(KEY2))) {
			hold[KEY2]++;
			if (hold[KEY2] >= LONGPRESS_THRES  && is_longpress[KEY2] == 0) {
				is_longpress[KEY2] = 1;
				longPressHandler[KEY2]();
			}
		} else {
			if (IS_INRANGE(hold[KEY2], 0, LONGPRESS_THRES)) {
				onePressHandler[KEY2]();
			}
			is_longpress[KEY2] = 0;
			hold[KEY2] = 0;
		}

		TMR3_ClearITFlag(TMR0_3_IT_CYC_END);
	}
}
