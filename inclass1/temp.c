/*
/*
 *  File Name: Pingpong_template_spring24.c
 */

//  3-14-2024
// ECE530 Spring 2023-2024
// Version 4.0
// This is a template for Lab #6 Pingpong Game with Zybo Z7-20
// you are free to revise it to make your own solution.
// you are also free to write your own code from scratch

// Change the file name to lab6pingpong24jjs.c
// where JJS is your name initials
// Add your name and date below
// Author:
// Date:

#include "xparameters.h"
#include "xgpio.h"
#include "led_ip.h"
// Include scutimer header file
#include "xscutimer.h"
//====================================================

typedef enum {OVER, START} GameStatus;
typedef enum {LEFT, RIGHT} Direction;
typedef enum {LATEHIT, EARLYHIT, HIT, RESET, CONTINUOUS} PlayStatus;

//push buttons
#define RESETBUTTON 0b0100
#define STARTBUTTON 0b0010
#define LEFTPADDLE  0b1000
#define RIGHTPADDLE 0b0001

//ball at serve
#define LEFT_SERVE 0b1000
#define RIGHT_SERVE 0b0001
#define NO_BALL 0b0000
#define ALL_BALL 0b1111

//global variable definitions
uint32_t push_button_value, slide_switch_check, Status;
uint32_t scoreright, scoreleft;
XGpio slideSwitches, push_buttons;

// PS Timer related definitions
#define ONE_TENTH 33333334 // half of the CPU clock speed/10 of 667MHz
XScuTimer Timer;		/* Cortex A9 SCU Private Timer Instance */
XScuTimer_Config *ConfigPtr;
XScuTimer *TimerInstancePtr = &Timer;

//Function prototypes
void setTimer(void);
void RightPlay(PlayStatus *RightPlayerResult);
void LeftPlay(PlayStatus *LeftPlayerResult);

void setSwitches(XGpio *switches, u32 Device_ID, u32 Direction) {
	XGpio_Initialize(switches, Device_ID);
    //For Channel 1 only
	XGpio_SetDataDirection(switches, 1, Direction);
}
void setButtons(XGpio *push, u32 Device_ID, u32 Direction){
	XGpio_Initialize(push, Device_ID);
	XGpio_SetDataDirection(push, 1, Direction);
}

void UpdateLEDs(char LED_Value)
{
	LED_IP_mWriteReg(XPAR_LED_IP_S_AXI_BASEADDR, 0, LED_Value);
}


int main (void)
{
    uint32_t i;
	PlayStatus LeftPlayerResult, RightPlayerResult;
	GameStatus Game;
	Direction BallDirection;
	//configure push buttons and slide switches with setSwitches() function
	setSwitches(&slideSwitches, XPAR_SWITCHES_DEVICE_ID, 0xffffffff);
	setButtons(&push_buttons, XPAR_BUTTONS_DEVICE_ID, 0xffffffff);

	//Initialize internal timer
	xil_printf("Timer init() called\r\n");
	ConfigPtr = XScuTimer_LookupConfig(XPAR_PS7_SCUTIMER_0_DEVICE_ID);
	Status = XScuTimer_CfgInitialize(TimerInstancePtr, ConfigPtr, ConfigPtr->BaseAddr);
	if(Status != XST_SUCCESS){
	    xil_printf("Timer Failed to Init\r\n");
	    return XST_FAILURE;
    }
	xil_printf("Timer Success Init\r\n");


    xil_printf("-- Pingpong Game by Owen Sapp Spring 2023-2024 --\r\n");
    scoreright = 0; scoreleft = 0;
    xil_printf("Left Player Score = %d   Right Player Score = %d\r\n", scoreright, scoreleft);
    BallDirection = RIGHT; Game=OVER;
    //turn off LEDs
    UpdateLEDs(NO_BALL);

    while (1){
	// check reset or start buttons
	    while(1) {
	        push_button_value = XGpio_DiscreteRead(&push_buttons, 1);
            switch (push_button_value) {
                case RESETBUTTON:	//reset game
                xil_printf("\n\rNew Game - Scores Reset\r\n");
	    	    Game=OVER;
	    	    scoreright = 0; scoreleft = 0;
	    	    xil_printf("Left Player Score = %d   Right Player Score = %d\r\n", scoreright, scoreleft);
	    	    //turn on all LEDs
	    	    UpdateLEDs(ALL_BALL);
	    	    for(i=0;i<3000000;i++);	//keep ALL LEDs on for a while
	    	    while(push_button_value == RESETBUTTON)
	    		    push_button_value = XGpio_DiscreteRead(&push_buttons, 1);

	    	    //turn off all LEDs
	    	    UpdateLEDs(NO_BALL);
	    	    break;


	  	    case STARTBUTTON:
			    Game=START;	//start game
			    if(BallDirection==RIGHT) BallDirection=LEFT; else BallDirection = RIGHT;
			    break;
	  	    default: Game=OVER; break;
	        }
	        if(Game==START) break;
        } //end  inner while(1) to exit when Game == START

	  //************play pingong game
	  	while(Game==START){
	  		if(BallDirection==RIGHT) {
	  		    RightPlayerResult = CONTINUOUS;
	  			RightPlay(& RightPlayerResult);
	  			if(RightPlayerResult==HIT) BallDirection=LEFT;
	  			else Game=OVER;
	  		}
	  		else {
	  			LeftPlayerResult = CONTINUOUS;
	  			LeftPlay(& LeftPlayerResult);
	  			if(LeftPlayerResult==HIT) BallDirection=RIGHT;
	  			else Game=OVER;
	  		}
	  	  }//end while(Game==START)

	//update score after the game
	    switch(BallDirection) {
	    case RIGHT:
		scoreleft++;
		//display how the game was played on Terminal
		switch(RightPlayerResult) {
		    case LATEHIT: 	
            xil_printf("\n\rGame Over because Right Player hit too late.\n\r");
            break;
		 	case EARLYHIT: 	
            xil_printf("\n\rGame Over because Right Player hit too early.\n\r");
		 	break;
		 	default: 
            xil_printf("\n\rGame Over because Right Player lost.\n\r");
		 	break;
		}
		break;
	    case LEFT:
		scoreright++;
        switch(LeftPlayerResult) {
		    case LATEHIT: 	
            xil_printf("\n\rGame Over because Left Player hit too late.\n\r");
            break;
		 	case EARLYHIT: 	
            xil_printf("\n\rGame Over because Left Player hit too early.\n\r");
		 	break;
		 	default: 
            xil_printf("\n\rGame Over because Left Player lost.\n\r");
		 	break;
		}
		break;
	    //display how the game was played on Terminal
	    }
	    xil_printf("Left Player Score = %d   Right Player Score = %d\r\n", scoreleft, scoreright);
   }	//while(1)
} //main()

//ball moves to the right
void RightPlay(PlayStatus *RightPlayerStatus)
{
char ball;
	ball = LEFT_SERVE;
	UpdateLEDs(ball);
	setTimer();
	while(*RightPlayerStatus == CONTINUOUS)
	{
	//The ball moves from left to right
		if(XScuTimer_IsExpired(TimerInstancePtr)){
			//Timer over
			if(ball == RIGHTPADDLE){
				*RightPlayerStatus = LATEHIT;
			    return;
			}
			ball = ball >> 1;
			UpdateLEDs(ball);
			setTimer();
		}
		else{
			//Timer not over
			push_button_value = XGpio_DiscreteRead(&push_buttons, 1);
			if(push_button_value == RIGHTPADDLE && ball == RIGHTPADDLE){
				*RightPlayerStatus = HIT;
				ball = RIGHT_SERVE;
				while(!XScuTimer_IsExpired(TimerInstancePtr)){}
				return;
			}
			else if(push_button_value == RESETBUTTON){
				*RightPlayerStatus = RESET;
			}
			else if(push_button_value != 0){
				*RightPlayerStatus = EARLYHIT;
			}
		}

	}//end while(*RightPlayerStatus == CONTINUOUS)
}//end RightPlay()

//ball moves to the left
//symmetric to RightPlay()
void LeftPlay(PlayStatus *LeftPlayerStatus)
{
char ball;
	ball = RIGHT_SERVE;
	UpdateLEDs(ball);
	setTimer();
	while(*	LeftPlayerStatus == CONTINUOUS)
	{
		if(XScuTimer_IsExpired(TimerInstancePtr)){
			//Timer over
			if(ball == LEFTPADDLE){
				*LeftPlayerStatus = LATEHIT;
				return;
			}
			ball = ball << 1;
			UpdateLEDs(ball);
			setTimer();
		}
		else{
			//Timer not over
			push_button_value = XGpio_DiscreteRead(&push_buttons, 1);
			if(push_button_value == LEFTPADDLE && ball == LEFTPADDLE){
				*LeftPlayerStatus = HIT;
				ball = LEFT_SERVE;
				while(!XScuTimer_IsExpired(TimerInstancePtr)){}
				return;
			}
			else if(push_button_value == RESETBUTTON){
				*LeftPlayerStatus = RESET;
			}
			else if(push_button_value != 0){
				*LeftPlayerStatus = EARLYHIT;
			}
		}
	}//end while(*LeftPlayerStatus == CONTINUOUS)

}//end LeftPlay(



void setTimer(void){
	// Read slide switch values
	int switch_status = XGpio_DiscreteRead(&slideSwitches, 1);
	// Load timer with delay in multiple of ONE_TENTH
	XScuTimer_LoadTimer(TimerInstancePtr, ONE_TENTH * switch_status);
	//start timer
	XScuTimer_Start(TimerInstancePtr);
}//end setTimer(void)


