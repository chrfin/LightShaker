/*
 * drvDisplay.c
 *
 *  Created on: Aug 30, 2016
 *      Author: Christian
 */
#include <drvAccelerometer.h>
#include <drvNeopixels.h>
#include "main.h"
#include "drvDisplay.h"
#include "stm32f0xx_exti.h"
#include "stm32f0xx_rcc.h"
#include "drvNvMemory.h"

volatile int8_t rowStep = 0;
volatile uint8_t RowNumber = 0;
volatile uint8_t RowsLogic;						//should be 2^n
volatile uint8_t RowsOverscan;
volatile uint8_t RowsVisible;

volatile uint16_t DispRowMasks[32];

volatile enum {
	DISP_POS_ROW_START, DISP_POS_GAP_START
} DisplayPosition;

//state-variable
volatile enum {
	STATE_UNKNOWN,
	STATE_LEFT_END,
	STATE_MOVE_FORW,
	STATE_RIGHT_END,
	STATE_MOVE_BACKW
} movementState;

/*
 * 2 Timers running with same timebase:
 * Timer A measures t_frame (in as much ticks as possible) -> the timers have to run very fast
 * Timer B has its max value set to 1/rowCount/2 of the count value from timer A (at each end point)
 *
 * So the following has to happen in every end point:
 * take the counter value from timer A
 * divide it by rowCount*2 (with efficient shift operation)
 * load the result into compare match register of Timer B
 * set line number and direction variables
 * reset both timers to 0
 *
 *
 *TODO: test and optimize all the timings in this class
 */

void displayInit() {
	//set up mma8653 with +-4g-Range, low res and high sampling rate
	//set up interrupt-driven sensor readout
	Accelerometer_setRange(RANGE_8G);
	Accelerometer_setDataWidth(ACC_DATAWIDTH_8);
	Accelerometer_setDataRate(RATE_200Hz);
	Accelerometer_initIrq();

	Neopixels_Off();

	//set up timer TIM2 for measuring t_frame
	//upcounting,12MHz-> prescaler = 4,
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM2->CR1 = 0;
	TIM2->PSC = 48;
	//reset the counter:
	TIM2->EGR |= TIM_EGR_UG;
	TIM2->CR1 |= TIM_CR1_CEN;

	//setup TIM2 Compare Channel 1 for setting the length of the LockTime!
	//the data-ready interrupt from the sensor is ignored during that time
	//set to 30 ms (a faster swipe doesn't seem to be possible by hand)
	TIM2->DIER |= TIM_DIER_CC1IE;
	TIM2->CCR1 = 30000;
	TIM2->CCER |= TIM_CCER_CC1E;
	NVIC_EnableIRQ(TIM2_IRQn);

	//set up timer TIM3 for triggering the lines with t_line (= t_frame/LINECOUNT)
	//upcounting 12Mhz, autoreload @ t_frame/LINECOUNT, interrupt @ overflow
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	TIM3->CR1 = 0;
	TIM3->PSC = 48;
	TIM3->DIER |= TIM_DIER_UIE;
	//clear interrupt flag
	TIM3->SR &= ~TIM_SR_UIF;
	//enable the IRQ
	NVIC_EnableIRQ(TIM3_IRQn);
	//don't activate this timer now, because an overflow would produce a line on the display!

	//if the device is unconfigured (rowsVisible = 0), set a smiley as default picture
	//this is not needed anymore! the picture data is planted into the flash directly (drvNvMemory.h)
//	if (NvMem_read(NVMEM_AD_ROWS_VISIBLE) > 32
//			|| NvMem_read(NVMEM_AD_ROWS_VISIBLE) == 0) {
//		NvMem_write(NVMEM_AD_ROWS_VISIBLE, 16);
//		NvMem_write(NVMEM_AD_OVERSCAN, 0);
//		NvMem_write(NVMEM_AD_PICTURE_START + 0, 0b0000001111000000);
//		NvMem_write(NVMEM_AD_PICTURE_START + 1, 0b0000110000110000);
//		NvMem_write(NVMEM_AD_PICTURE_START + 2, 0b0001000000001000);
//		NvMem_write(NVMEM_AD_PICTURE_START + 3, 0b0010000000000100);
//		NvMem_write(NVMEM_AD_PICTURE_START + 4, 0b0100010000010010);
//		NvMem_write(NVMEM_AD_PICTURE_START + 5, 0b0100010000010010);
//		NvMem_write(NVMEM_AD_PICTURE_START + 6, 0b1000000000001001);
//		NvMem_write(NVMEM_AD_PICTURE_START + 7, 0b1000000110001001);
//		NvMem_write(NVMEM_AD_PICTURE_START + 8, 0b1000000000001001);
//		NvMem_write(NVMEM_AD_PICTURE_START + 9, 0b1000000000001001);
//		NvMem_write(NVMEM_AD_PICTURE_START + 10, 0b0100010000010010);
//		NvMem_write(NVMEM_AD_PICTURE_START + 11, 0b0100010000010010);
//		NvMem_write(NVMEM_AD_PICTURE_START + 12, 0b0010000000000100);
//		NvMem_write(NVMEM_AD_PICTURE_START + 13, 0b0001000000001000);
//		NvMem_write(NVMEM_AD_PICTURE_START + 14, 0b0000110000110000);
//		NvMem_write(NVMEM_AD_PICTURE_START + 15, 0b0000001111000000);
//	}
//	//if the color has never been configured (or if somebody configured it to 0,0,0) the Lightshaker doesn't do anything visible, so let's change that
//	if (!NvMem_read(NVMEM_AD_GLOBAL_BLUE) && !NvMem_read(NVMEM_AD_GLOBAL_GREEN)
//			&& !NvMem_read(NVMEM_AD_GLOBAL_RED)) {
//		NvMem_write(NVMEM_AD_GLOBAL_BLUE, 255);
//		NvMem_write(NVMEM_AD_GLOBAL_GREEN, 255);
//		NvMem_write(NVMEM_AD_GLOBAL_RED, 255);
//	}

	//init the display data
	RowsOverscan = NvMem_read(NVMEM_AD_OVERSCAN);
	RowsVisible = NvMem_read(NVMEM_AD_ROWS_VISIBLE);

	RowsLogic = RowsVisible + 2 * RowsOverscan;

	//set the color to whatever is defined in NvMem
	Neopixels_SetColorRGB(NvMem_read(NVMEM_AD_GLOBAL_RED),
			NvMem_read(NVMEM_AD_GLOBAL_GREEN),
			NvMem_read(NVMEM_AD_GLOBAL_BLUE));
	Neopixels_SetBrightness(31);

}

//called 2 times for every row
//first call should switch on the LEDs according to rowData, 2nd should switch them off to insert a blank row
//without this blank row, the rows are squeezed together too much
void displaySendLine() {
	//clear interrupt flag
	TIM3->SR &= ~TIM_SR_UIF;

	//left side is the row, right side the gap
	if (DisplayPosition == DISP_POS_ROW_START) {
		//if row is in the visible area
		if (RowNumber >= RowsOverscan
				&& RowNumber < RowsOverscan + RowsVisible) {
			Neopixels_Pattern(NvMem_read(NVMEM_AD_PICTURE_START + RowNumber));
		} else {
			//switch off the display
			Neopixels_Off();
		}
		DisplayPosition = DISP_POS_GAP_START;
	} else //if(DisplayPosition == DISP_POS_GAP_START)
	{
		//switch off the display
		Neopixels_Off();

		//next row
		RowNumber += rowStep;
		DisplayPosition = DISP_POS_ROW_START;
		//prevent an overshoot if something with the timing doesn't fit
		if (RowNumber >= RowsLogic) {
			//prevent TIM3 from triggering a new row by stopping it
			TIM3->CR1 &= ~TIM_CR1_CEN;
		}

	}

}

//triggered by TIM2 CompareMatch
void displayEndOfLocktime() {
	//clear the flag and read the data (so the sensor releases the INT line)
	EXTI->PR |= EXTI_PR_PR2;
	Accelerometer_read8();

	//reenable the EXTI interrupt
	EXTI->IMR |= EXTI_EMR_MR2;

	//clear Flag for this Interrupt
	TIM2->SR &= ~TIM_SR_CC1IF;
}

void displayFrameStart() {

	//deactivate the peak detection for some time after a valid return point
	//by deactivating the EXTI interrupt
	EXTI->IMR &= ~EXTI_EMR_MR2;

	//take the counter value from timer2:
	uint32_t tFrame = TIM2->CNT;

	//calculate t_row and save it to the auto-reload register of tim3
	TIM3->ARR = (uint16_t) (tFrame / (RowsLogic * 2));
	//clear interrupt flag for TIM3
	TIM3->SR &= ~TIM_SR_UIF;
	//reset both timers
	TIM2->EGR |= TIM_EGR_UG;
	TIM3->EGR |= TIM_EGR_UG;
	//start both timers
	TIM2->CR1 |= TIM_CR1_CEN;
	TIM3->CR1 |= TIM_CR1_CEN;
}

//now this should be called with the sensors IRQ
void displayFindReturnPoint() {
	//the Interrupt-flag is not reset until the work in this ISR is done!

	//read the output of the sensor
	//the low-pass filtering is done by the sensor!
	//this also lets the sensor release the interrupt line
	int8_t acc = Accelerometer_read8().x;

	//separate value and sign for faster calculations
	uint8_t accAbs;
	if (acc >= 0) {
		accAbs = acc;
	} else {
		accAbs = -acc;
	}

	/**
	 * the return doesn't happen in one point, but takes some time (>10ms)
	 * ->stop the time measurement as soon as the acc gets higher that the threshold,
	 * ->start the next meas and the display if the acc gets lower than the threshold again
	 */

	switch (movementState) {
	case STATE_UNKNOWN:
		if (accAbs > ACC_RETURN_TH) {	//this could be the first return point
										//stop TIM2
			TIM2->CR1 &= ~TIM_CR1_CEN;
			//prevent TIM3 from triggering a new row by stopping it
			TIM3->CR1 &= ~TIM_CR1_CEN;

			if (acc > 0) {
				movementState = STATE_RIGHT_END;
			} else {
				movementState = STATE_LEFT_END;
			}
		}
		break;
	case STATE_LEFT_END:
		if (accAbs < ACC_RETURN_TH) {
			rowStep = 1;
			RowNumber = 0;
			DisplayPosition = DISP_POS_ROW_START;
			//end of return phase
			displayFrameStart();
			movementState = STATE_MOVE_FORW;
		}
		break;
	case STATE_RIGHT_END:
		if (accAbs < ACC_RETURN_TH) {
			//end of return phase
			rowStep = -1;
			RowNumber = RowsLogic;
			DisplayPosition = DISP_POS_GAP_START;
			displayFrameStart();

			movementState = STATE_MOVE_BACKW;
		}
		break;
	case STATE_MOVE_FORW:
		if (acc > ACC_RETURN_TH) {
			//stop TIM2
			TIM2->CR1 &= ~TIM_CR1_CEN;
			//prevent TIM3 from triggering a new row by stopping it
			TIM3->CR1 &= ~TIM_CR1_CEN;
			//switch off the display
			Neopixels_Off();
			movementState = STATE_RIGHT_END;
		}
		break;
	case STATE_MOVE_BACKW:
		if (acc < -ACC_RETURN_TH) {
			//stop TIM2
			TIM2->CR1 &= ~TIM_CR1_CEN;
			//prevent TIM3 from triggering a new row by stopping it
			TIM3->CR1 &= ~TIM_CR1_CEN;
			//switch off the display
			Neopixels_Off();
			movementState = STATE_LEFT_END;
		}
		break;
	}

	//no state other than STATE_UNKNOWN should last longer than T_FRAME_MAX
	//-> if TIM2 exceeds this time, stop it and
	//if this value is out of bounds, reset TIM2 and return to STATE_UNKNOWN
	if (TIM2->CNT > T_FRAME_MAX) {	//reset the counter:
		TIM2->EGR |= TIM_EGR_UG;
		//and stop it
		TIM2->CR1 |= TIM_CR1_CEN;
		//switch off the display
		Neopixels_Off();
		//set state
		movementState = STATE_UNKNOWN;
	}

	EXTI_ClearITPendingBit(EXTI_Line2);

}

