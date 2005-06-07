//////////////////////////////////////////////////////////////////////
// Simple ARM7 stub (sends RTC, TSC, and X/Y data to the ARM 9)
// -- joat
// -- modified by Darkain and others
//////////////////////////////////////////////////////////////////////

 
#include "nds.h"
 
#include "nds/bios.h"
#include "nds/arm7/touch.h"
#include "nds/arm7/clock.h"

//////////////////////////////////////////////////////////////////////

#define SCREEN_WIDTH	256
#define SCREEN_HEIGHT	192

s32 TOUCH_WIDTH; 
s32 TOUCH_HEIGHT; 
s32 TOUCH_OFFSET_X;
s32 TOUCH_OFFSET_Y; 

void startSound(int sampleRate, const void* data, uint32 bytes, u8 channel, u8 vol,  u8 pan, u8 format) {
	SCHANNEL_TIMER(channel)  = SOUND_FREQ(sampleRate);
	SCHANNEL_SOURCE(channel) = (uint32)data;
	SCHANNEL_LENGTH(channel) = bytes;
	SCHANNEL_CR(channel)     = SOUND_ENABLE | SOUND_ONE_SHOT | SOUND_VOL(vol) | SOUND_PAN(pan) | (format==1?SOUND_8BIT:SOUND_16BIT);
}


//---------------------------------------------------------------------------------
s8 getFreeSoundChannel() {
//---------------------------------------------------------------------------------
	int i;
	for (i=0; i<16; i++) {
		if ( (SCHANNEL_CR(i) & SOUND_ENABLE) == 0 ) return i;
	}
	return -1;
}

//---------------------------------------------------------------------------------
void InterruptHandler(void) {
//---------------------------------------------------------------------------------
	static int heartbeat = 0;
 
	if (IF & IRQ_VBLANK) {
		uint16 but=0, but2=0, x=0, y=0, xpx=0, ypx=0, z1=0, z2=0, batt=0, aux=0;
		int t1=0, t2=0;
		uint32 temp=0;
		uint8 ct[sizeof(IPC->curtime)];
		u32 i;
		
		// Update the heartbeat
		heartbeat++;
 
		// Read the X/Y buttons and the /PENIRQ line
		but = XKEYS;
		if (!(but & 0x40)) {
			// Read the touch screen
			x = touchRead(TSC_MEASURE_X);
			y = touchRead(TSC_MEASURE_Y);
			xpx = ( ((SCREEN_WIDTH -60) * x) / TOUCH_WIDTH  ) - TOUCH_OFFSET_X;
			ypx = ( ((SCREEN_HEIGHT-60) * y) / TOUCH_HEIGHT ) - TOUCH_OFFSET_Y;
			z1 = touchRead(TSC_MEASURE_Z1);
			z2 = touchRead(TSC_MEASURE_Z2);
		}

		but2 = XKEYS;
		if (!(but & 0x40) && (but2 & 0x40)) {
			x		= 0;
			y		= 0;
			xpx	= 0;
			ypx	= 0;
			z1	= 0;
			z2	= 0;
			but	= but2;
		}
		
		batt = touchRead(TSC_MEASURE_BATTERY);
		aux  = touchRead(TSC_MEASURE_AUX);

		// Read the time
		rtcGetTime((uint8 *)ct);
		BCDToInteger((uint8 *)&(ct[1]), 7);
 
		// Read the temperature
		temp = touchReadTemperature(&t1, &t2);
 
		// Update the IPC struct
		IPC->heartbeat	= heartbeat;
		IPC->buttons		= but;
		IPC->touchX			= x;
		IPC->touchY			= y;
		IPC->touchXpx		= xpx;
		IPC->touchYpx		= ypx;
		IPC->touchZ1		= z1;
		IPC->touchZ2		= z2;
		IPC->battery		= batt;
		IPC->aux				= aux;

		for(i=0; i<sizeof(ct); i++) {
			IPC->curtime[i] = ct[i];
		}

		IPC->temperature = temp;
		IPC->tdiode1 = t1;
		IPC->tdiode2 = t2;


		//sound code  :)
		TransferSound *snd = IPC->soundData;
		IPC->soundData = 0;

		if (snd) {
			for (i=0; i<snd->count; i++) {
				s8 chan = getFreeSoundChannel();

				if (chan >= 0) {
					startSound(snd->data[i].rate, snd->data[i].data, snd->data[i].len, chan, snd->data[i].vol, snd->data[i].pan, snd->data[i].format);
				}
			}
		}
	}
 
	// Acknowledge interrupts
	IF = IF;
} 

//////////////////////////////////////////////////////////////////////
 

int main(int argc, char ** argv) {

	TOUCH_WIDTH  = PersonalData->calX2 - PersonalData->calX1; //TOUCH_CAL_X2 - TOUCH_CAL_X1;
	TOUCH_HEIGHT = PersonalData->calY2 - PersonalData->calY1; //TOUCH_CAL_Y2 - TOUCH_CAL_Y1;
	TOUCH_OFFSET_X = ( ((SCREEN_WIDTH -60) * PersonalData->calX1) / TOUCH_WIDTH  ) - 28;
	TOUCH_OFFSET_Y = ( ((SCREEN_HEIGHT-60) * PersonalData->calY1) / TOUCH_HEIGHT ) - 28;

	// Reset the clock if needed
	rtcReset();

	//enable sound
	SOUND_CR = SOUND_ENABLE | SOUND_VOL(0x7F);
	IPC->soundData = 0;
 
	// Set up the interrupt handler
	IME = 0;
	IRQ_HANDLER = &InterruptHandler;
	IE = IRQ_VBLANK;
	IF = ~0;
	DISP_SR = DISP_VBLANK_IRQ;
	IME = 1;

	// Keep the ARM7 out of main RAM
	while (1) swiWaitForVBlank();
	return 0;
}

 
//////////////////////////////////////////////////////////////////////
