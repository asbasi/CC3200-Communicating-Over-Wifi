// **********************************************************
// Arvinder Basi & Kelvin Lu
// EEC 172 - Embedded Systems
// Lab Assignment 4: Communication over Wifi using MQTT
// 2/25/2016
// **********************************************************

// Standard includes
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


// simplelink includes
#include "simplelink.h"

// driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "interrupt.h"
#include "rom_map.h"
#include "prcm.h"
#include "uart.h"
#include "timer.h"

#include "hw_common_reg.h"
#include "spi.h"
#include "rom.h"
#include "gpio.h"
#include "systick.h"

// Adafruit includes.
#include "Adafruit_SSD1351.h"
#include "glcdfont.h"
#include "Adafruit_GFX.h"
#include "test.h"

// common interface includes
#include "network_if.h"
#ifndef NOTERM
#include "uart_if.h"
#endif

#include "button_if.h"
#include "gpio_if.h"
#include "timer_if.h"
#include "common.h"
#include "utils.h"


#include "sl_mqtt_client.h"

// application specific includes
#include "pin_mux_config.h"
// "pinmux.h"

#define APPLICATION_VERSION 	"1.1.1"

/*Operate Lib in MQTT 3.1 mode.*/
#define MQTT_3_1_1              false /*MQTT 3.1.1 */
#define MQTT_3_1                true /*MQTT 3.1*/

#define WILL_TOPIC              "Client"
#define WILL_MSG                "Client Stopped"
#define WILL_QOS                QOS1
#define WILL_RETAIN             false

/*Defining Broker IP address and port Number*/
//#define SERVER_ADDRESS          "messagesight.demos.ibm.com"
// Enter your AWS Endpoint address as the SERVER ADDRESS
//#define SERVER_ADDRESS          "A3HZ1IW3KGGVT.iot.us-west-2.amazonaws.com"
#define SERVER_ADDRESS 		"A3HZ1IW3KGGVT.iot.us-east-1.amazonaws.com"

//#define PORT_NUMBER             1883
#define PORT_NUMBER             8883

#define MAX_BROKER_CONN         1

#define SERVER_MODE             MQTT_3_1_1
/*Specifying Receive time out for the Receive task*//*Specifying Receive time out for the Receive task*/
#define RCV_TIMEOUT             30

/*Background receive task priority*/
#define TASK_PRIORITY           3

/* Keep Alive Timer value*/
#define KEEP_ALIVE_TIMER        25

/*Clean session flag*/
#define CLEAN_SESSION           true

/*Retain Flag. Used in publish message. */
#define RETAIN                  0

/*Defining Publish Topic*/
#define PUB_TOPIC      		 	"/cc3200/RX_Kel"
//#define PUB_TOPIC				"/cc3200/RX_Arv"

/*Defining Number of topics*/
#define TOPIC_COUNT             1

/*Defining Subscription Topic Values*/
#define SUB_TOPIC				"/cc3200/RX_Arv"
//#define SUB_TOPIC				"/cc3200/RX_Kel"

/*Defining QOS levels*/
#define QOS1                    1

/*Spawn task priority and OSI Stack Size*/
#define OSI_STACK_SIZE          2048
#define UART_PRINT              Report

//NEED TO UPDATE THIS FOR IT TO WORK!
#define DATE                16   /* Current Date */
#define MONTH               02     /* Month 1-12 */
#define YEAR                2016  /* Current year */
#define HOUR                20   /* Time - hours */
#define MINUTE              19    /* Time - minutes */
#define SECOND              6    /* Time - seconds */

#define SYSTICK_WRAP_TICKS		16777216 // 2^24

#define SYS_CLK				    80000000
#define MILLISECONDS_TO_TICKS(ms)   ((SYS_CLK/1000) * (ms))
#define TICKS_TO_MILLISECONDS(ts)   (((double) 1000/(double) SYS_CLK) * (double) (ts))
#define SPI_IF_BIT_RATE  100000

#define NO_PROCESSING_MODE 		0
#define	SHORT_PROCESSING_MODE 	1
#define LONG_PROCESSING_MODE 	2

#define EDGE_TYPE_FALLING 		0
#define EDGE_TYPE_RISING 		1

void PhotoresistorIntDisable();
void EdgeTriggerHandler();
void PhotoresistorIntInit();
void PhotoresistorIntEnable();
void PhotoresistorIntDisable();
void SystickInit();
void SystickReset();
unsigned long tickDiff();
void TransitionTimerIntStart(unsigned long);
void FinishTimerIntStart(unsigned long);
void TransitionTimerIntStop();
void FinishTimerIntStop();
void TransitionTimerIntHandler();
void FinishTimerIntHandler();
void RepeatTimerIntHandler();
void ConfigureMessageDisplay();

unsigned int 		g_signalMode 	= NO_PROCESSING_MODE;
unsigned int 		g_finishMode 	= NO_PROCESSING_MODE;
unsigned long 		g_ticksLast 	= SYSTICK_WRAP_TICKS;
unsigned long 		g_ticksElapsed 	= 0UL;
unsigned long long 	g_bit_rep 		= 0ULL;
unsigned char 		g_last_toggle	= 0;
unsigned long long 	g_last_bit_rep	= 0ULL;
unsigned char		g_repeat        = 0;
unsigned char 		g_send_key		= 0;

/*******************************************************************
 * Declarations related to the TIC TAC TOE Mini-game.
 *******************************************************************/
#define TIC_TAC_TOE_CODE 176 // Remote key for tic tac toe message.

#define FREE		0
#define PLAYER_O 	1
#define PLAYER_X 	2

bool ttt_mode = false; 		 // Are we in tick tac toe mode.
bool your_turn = false; 	 // Is it your turn?
unsigned int move_count = 0; // The number of moves we've gone through.

int player = 0; 			 // X or O.
int opp = 0;

unsigned char playerChar;
unsigned char oppChar;

int board[10] = {0};			 // The game board.
/********************************************************************/

typedef struct
{
   /* time */
   unsigned long tm_sec;
   unsigned long tm_min;
   unsigned long tm_hour;
   /* date */
   unsigned long tm_day;
   unsigned long tm_mon;
   unsigned long tm_year;
   unsigned long tm_week_day; //not required
   unsigned long tm_year_day; //not required
   unsigned long reserved[3];
}SlDateTime;

typedef struct connection_config{
    SlMqttClientCtxCfg_t broker_config;
    void *clt_ctx;
    unsigned char *client_id;
    unsigned char *usr_name;
    unsigned char *usr_pwd;
    bool is_clean;
    unsigned int keep_alive_time;
    SlMqttClientCbs_t CallBAcks;
    int num_topics;
    char *topic[TOPIC_COUNT];
    unsigned char qos[TOPIC_COUNT];
    SlMqttWill_t will_params;
    bool is_connected;
}connect_config;

typedef enum events
{
    PUSH_BUTTON_SW2_PRESSED,
    PUSH_BUTTON_SW3_PRESSED,
    BROKER_DISCONNECTION
}osi_messages;

//*****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//*****************************************************************************
static void
Mqtt_Recv(void *app_hndl, const char  *topstr, long top_len, const void *payload,
          long pay_len, bool dup,unsigned char qos, bool retain);
static void sl_MqttEvt(void *app_hndl,long evt, const void *buf,
                       unsigned long len);
static void sl_MqttDisconnect(void *app_hndl);
void BoardInit(void);
static void DisplayBanner(char * AppName);
void MqttClient(void *pvParameters);

static int set_time();
//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
#ifdef USE_FREERTOS
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#endif

char *security_file_list[] = {"/cert/private.der", "/cert/client.der", "/cert/rootCA.der"};  //Order: Private Key, Certificate File, CA File, DH Key (N/A)
SlDateTime g_time;

unsigned short g_usTimerInts;
/* AP Security Parameters */
SlSecParams_t SecurityParams = {0};

/*Message Queue*/
OsiMsgQ_t g_PBQueue;

/* connection configuration */
connect_config usr_connect_config[] =
{
    {
        {
            {
                (SL_MQTT_NETCONN_URL|SL_MQTT_NETCONN_SEC),
                SERVER_ADDRESS,
                PORT_NUMBER,
				SL_SO_SEC_METHOD_TLSV1_2,	//Method (TLS1.2)
				SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA,	//Cipher
				3,							//number of files
				security_file_list 			//name of files
            },
            SERVER_MODE,
            true,
        },
        NULL,
        "asdpioagdjip_arv",
        NULL,//null
        NULL,
        true,
        KEEP_ALIVE_TIMER,
        {Mqtt_Recv, sl_MqttEvt, sl_MqttDisconnect},
        TOPIC_COUNT,
		{SUB_TOPIC}, //{TOPIC1, TOPIC2, TOPIC3},
		{QOS1}, //{QOS1, QOS1, QOS1},
        /*{WILL_TOPIC,WILL_MSG,WILL_QOS,WILL_RETAIN},*/
		NULL,
        false
    }
};

/* library configuration */
SlMqttClientLibCfg_t Mqtt_Client={
    1882,
    TASK_PRIORITY,
    30,
    true,
    (long(*)(const char *, ...))UART_PRINT
};

/*Publishing topics and messages*/
const char *pub_topic_sender = PUB_TOPIC;

void *app_hndl = (void*)usr_connect_config;
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//****************************************************************************
//! Defines Mqtt_Pub_Message_Receive event handler.
//! Client App needs to register this event handler with sl_ExtLib_mqtt_Init 
//! API. Background receive task invokes this handler whenever MQTT Client 
//! receives a Publish Message from the broker.
//!
//!\param[out]     topstr => pointer to topic of the message
//!\param[out]     top_len => topic length
//!\param[out]     payload => pointer to payload
//!\param[out]     pay_len => payload length
//!\param[out]     retain => Tells whether its a Retained message or not
//!\param[out]     dup => Tells whether its a duplicate message or not
//!\param[out]     qos => Tells the Qos level
//!
//!\return none
//****************************************************************************
static void
Mqtt_Recv(void *app_hndl, const char  *topstr, long top_len, const void *payload,
                       long pay_len, bool dup,unsigned char qos, bool retain)
{
    
    char *output_str=(char*)malloc(top_len+1);
    memset(output_str,'\0',top_len+1);
    strncpy(output_str, (char*)topstr, top_len);
    output_str[top_len]='\0';

    UART_PRINT("\n\rPublish Message Received");
    UART_PRINT("\n\rTopic: ");
    UART_PRINT("%s",output_str);
    free(output_str);
    UART_PRINT(" [Qos: %d] ",qos);
    if(retain)
      UART_PRINT(" [Retained]");
    if(dup)
      UART_PRINT(" [Duplicate]");
    
    output_str=(char*)malloc(pay_len+1);
    memset(output_str,'\0',pay_len+1);
    strncpy(output_str, (char*)payload, pay_len);
    output_str[pay_len]='\0';
    UART_PRINT("\n\rData is: ");
    UART_PRINT("%s",(char*)output_str);
    UART_PRINT("\n\r");


    if(!ttt_mode && !strcmp(output_str, "TTT#MODE")) // Tic tac toe time.
    {
    	fillScreen(BLACK);
    	ttt_mode = true;
    	player = PLAYER_O;
    	opp = PLAYER_X;
    	playerChar = 'O';
    	oppChar = 'X';
    	move_count = 0;
    	your_turn = false;

    	setCursor(0,0);
		Outstr(" | |     Player: O");
		setCursor(0,8);
		Outstr("-+-+-");
		setCursor(0,16);
		Outstr(" | | ");
		setCursor(0,24);
		Outstr("-+-+-");
		setCursor(0,32);
		Outstr(" | | ");

		setCursor(0,48);
		setTextColor(RED,BLACK);
		Outstr("Opponents turn");

		setTextColor(BLUE,BLACK);
		setCursor(0,64);
		Outstr("Rules:");
		setCursor(0,72);
		Outstr("1) Wait for your turn");
		setCursor(0,80);
		Outstr("2) Use number keys to");
		setCursor(0,88);
		Outstr("   select your choice");
    }
    else if(ttt_mode) // Currently in tic tac toe mode.
    {
    	int val = atoi(output_str); // Get the square opponent selected.
    	board[val] = opp;
    	move_count++;

    	// Update board.
    	switch(val)
    	{
    		case 1:
    			drawChar(0, 0, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 2:
    			drawChar(12, 0, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 3:
    			drawChar(24, 0, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 4:
    			drawChar(0, 16, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 5:
    			drawChar(12, 16, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 6:
    			drawChar(24, 16, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 7:
    			drawChar(0, 32, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 8:
    			drawChar(12, 32, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    		case 9:
    			drawChar(24, 32, oppChar, RED, BLACK, (unsigned char) 1);
    			break;
    	}

    	int i;
    	// Check if the opponent won.
		if((board[1] == board[5] && board[1] == board[9] && board[1] != 0) ||
	       (board[3] == board[5] && board[3] == board[7] && board[3] != 0) ||
		   (board[1] == board[2] && board[1] == board[3] && board[1] != 0) ||
	       (board[4] == board[5] && board[4] == board[6] && board[4] != 0) ||
		   (board[7] == board[8] && board[7] == board[9] && board[7] != 0) ||
		   (board[1] == board[4] && board[4] == board[7] && board[1] != 0) ||
		   (board[2] == board[5] && board[5] == board[8] && board[2] != 0) ||
		   (board[3] == board[6] && board[6] == board[9] && board[3] != 0))
	    {
			UtilsDelay(20000000);
    		ConfigureMessageDisplay();
    		setCursor(12,80);
    		Outstr("You lose!");
    		ttt_mode = false;
	   		for(i = 1; i <= 9; i++)
	   			board[i] = FREE;
	   		setTextColor(WHITE,BLACK);
    	}
    	else if(move_count == 9)
    	{
    		UtilsDelay(20000000);
    		ConfigureMessageDisplay();
    		setCursor(12,80);
    		Outstr("Tie game!");
    		ttt_mode = false;
	   		for(i = 1; i <= 9; i++)
	   			board[i] = FREE;
	   		setTextColor(WHITE,BLACK);
    	}
    	else
    	{
    		your_turn = true;
    		setCursor(0,48);
    		setTextColor(BLUE,BLACK);
    		Outstr("Your turn         ");
    	}
    }
    else
    {
    	// Clear reciever side of OLED.
    	fillRect(12, 80, SSD1351WIDTH, SSD1351HEIGHT, BLACK);

    	int rec_x_cursor = 12;
    	int rec_y_cursor = 80;

    	int i = 0;
    	while(output_str[i] != '\0')
    	{
			if(rec_x_cursor > 120) // About to go off right edge.
			{
				rec_x_cursor = 12;
				rec_y_cursor += 8; // go to next line.
			}

			if(rec_y_cursor > 120) // About to go off bottom edge.
			{
				fillRect(12, 80, SSD1351WIDTH, SSD1351HEIGHT, BLACK);
				rec_x_cursor = 12;
				rec_y_cursor = 80; // start at top.
			}

			drawChar(rec_x_cursor, rec_y_cursor, output_str[i], BLUE, BLACK, (unsigned char) 1);
			rec_x_cursor += 6;

			++i;
    	}
    }

    free(output_str);
    
    return;
}

//****************************************************************************
//! Defines sl_MqttEvt event handler.
//! Client App needs to register this event handler with sl_ExtLib_mqtt_Init 
//! API. Background receive task invokes this handler whenever MQTT Client 
//! receives an ack(whenever user is in non-blocking mode) or encounters an error.
//!
//! param[out]      evt => Event that invokes the handler. Event can be of the
//!                        following types:
//!                        MQTT_ACK - Ack Received 
//!                        MQTT_ERROR - unknown error
//!                        
//!  
//! \param[out]     buf => points to buffer
//! \param[out]     len => buffer length
//!       
//! \return none
//****************************************************************************
static void
sl_MqttEvt(void *app_hndl, long evt, const void *buf,unsigned long len)
{
    int i;
    switch(evt)
    {
      case SL_MQTT_CL_EVT_PUBACK:
        UART_PRINT("PubAck:\n\r");
        UART_PRINT("%s\n\r",buf);
        break;
    
      case SL_MQTT_CL_EVT_SUBACK:
        UART_PRINT("\n\rGranted QoS Levels are:\n\r");
        
        for(i=0;i<len;i++)
        {
          UART_PRINT("QoS %d\n\r",((unsigned char*)buf)[i]);
        }
        break;
        
      case SL_MQTT_CL_EVT_UNSUBACK:
        UART_PRINT("UnSub Ack \n\r");
        UART_PRINT("%s\n\r",buf);
        break;
    
      default:
        break;
  
    }
}

//****************************************************************************
//
//! callback event in case of MQTT disconnection
//!
//! \param app_hndl is the handle for the disconnected connection
//!
//! return none
//
//****************************************************************************
static void
sl_MqttDisconnect(void *app_hndl)
{
    connect_config *local_con_conf;
    osi_messages var = BROKER_DISCONNECTION;
    local_con_conf = app_hndl;
    sl_ExtLib_MqttClientUnsub(local_con_conf->clt_ctx, local_con_conf->topic,
                              TOPIC_COUNT);
    UART_PRINT("disconnect from broker %s\r\n",
           (local_con_conf->broker_config).server_info.server_addr);
    local_con_conf->is_connected = false;
    sl_ExtLib_MqttClientCtxDelete(local_con_conf->clt_ctx);

    //
    // write message indicating publish message
    //
    osi_MsgQWrite(&g_PBQueue,&var,OSI_NO_WAIT);

}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
void BoardInit(void)
{
    /* In case of TI-RTOS vector table is initialize by OS itself */
    #ifndef USE_TIRTOS
    //
    // Set vector table base
    //
    #if defined(ccs)
        IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
    #endif
    #if defined(ewarm)
        IntVTableBaseSet((unsigned long)&__vector_table);
    #endif
    #endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}
//*****************************************************************************
// Interrupt Handlers
//*****************************************************************************
void SystickInit()
{
	SysTickPeriodSet(SYSTICK_WRAP_TICKS);
	SysTickIntRegister(SystickReset);
	SysTickEnable();
}

void SystickReset()
{
	SysTickPeriodSet(SYSTICK_WRAP_TICKS);
	SysTickEnable();
}

unsigned long tickDiff()
{
	// Note that systick counts down, so lesser tick values
	// represent a succeeding point in time

	unsigned long ticksCurrent = SysTickValueGet();
	unsigned long ticksLast = g_ticksLast;

	// Set up for next call
	g_ticksLast = ticksCurrent;

	// No wrap; most probable case, so we short circuit a branch here
	if (ticksCurrent < ticksLast) return ticksLast - ticksCurrent;

	// Wrapped ticks. We assume a max wrap of SYSTICK_MAX_TICKS.
	// The order of operations here is important to prevent arithmetic overflow.
	return (SYSTICK_WRAP_TICKS - ticksCurrent) + ticksLast;
}

void EdgeTriggerHandler()
{
	PhotoresistorIntDisable();

	if (!GPIOPinRead(GPIOA1_BASE,GPIO_PIN_0))
	{
		// Falling edges
		switch (g_signalMode)
		{
			case NO_PROCESSING_MODE:
				g_bit_rep = 0ULL;
				g_signalMode = SHORT_PROCESSING_MODE;
				TransitionTimerIntStart(3);
				break;
			case SHORT_PROCESSING_MODE:
				TransitionTimerIntStop();
				FinishTimerIntStart(3);
				g_ticksElapsed = tickDiff();
				g_bit_rep <<= 2;
				if (g_ticksElapsed > 50000) g_bit_rep |= 3ULL;
				else if (g_ticksElapsed > 37500) g_bit_rep |= 2ULL;
				else if (g_ticksElapsed > 25000) g_bit_rep |= 1ULL;
				break;
			case LONG_PROCESSING_MODE:
				TransitionTimerIntStop();
				FinishTimerIntStart(5);
				g_ticksElapsed = tickDiff();
				g_bit_rep <<= 1;
				g_bit_rep |= g_ticksElapsed > 90000 ? 1ULL : 0ULL;
				break;
			default:
				break;
		}
	} else {
		// Rising edges
		g_ticksLast = SysTickValueGet();
	}

	PhotoresistorIntEnable();
}

void PhotoresistorIntInit()
{
	GPIOIntTypeSet(GPIOA1_BASE,GPIO_PIN_0,GPIO_BOTH_EDGES);
	GPIOIntRegister(GPIOA1_BASE, EdgeTriggerHandler);

	IntPrioritySet(INT_GPIOA1, INT_PRIORITY_LVL_3);

	GPIOIntClear(GPIOA1_BASE,GPIO_PIN_0);
	GPIOIntEnable(GPIOA1_BASE,GPIO_INT_PIN_0);
}

void PhotoresistorIntEnable()
{
    //Enable GPIO Interrupt
    GPIOIntClear(GPIOA1_BASE,GPIO_PIN_0);
    IntPendClear(INT_GPIOA1);
    IntEnable(INT_GPIOA1);
    GPIOIntEnable(GPIOA1_BASE,GPIO_PIN_0);
}

void PhotoresistorIntDisable()
{
    //Clear and Disable GPIO Interrupt
    GPIOIntDisable(GPIOA1_BASE,GPIO_PIN_0);
    GPIOIntClear(GPIOA1_BASE,GPIO_PIN_0);
    IntDisable(INT_GPIOA1);
}

void AllTimersIntInit()
{
    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_A_ONE_SHOT | TIMER_CFG_B_ONE_SHOT, TIMER_BOTH, 0);
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_BOTH, TransitionTimerIntHandler);

    Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_A_ONE_SHOT | TIMER_CFG_B_ONE_SHOT, TIMER_BOTH, 0);
    Timer_IF_IntSetup(TIMERA1_BASE, TIMER_BOTH, FinishTimerIntHandler);

    Timer_IF_Init(PRCM_TIMERA2, TIMERA2_BASE, TIMER_CFG_A_ONE_SHOT | TIMER_CFG_B_ONE_SHOT, TIMER_BOTH, 0);
    Timer_IF_IntSetup(TIMERA2_BASE, TIMER_BOTH, RepeatTimerIntHandler);
}

void TransitionTimerIntStart(unsigned long ms)
{
    Timer_IF_Start(TIMERA0_BASE, TIMER_BOTH, ms); // in milliseconds, not ticks.
}

void FinishTimerIntStart(unsigned long ms)
{
    Timer_IF_Start(TIMERA1_BASE, TIMER_BOTH, ms); // in milliseconds, not ticks.
}

void RepeatTimerIntStart(unsigned long ms)
{
    Timer_IF_Start(TIMERA2_BASE, TIMER_BOTH, ms); // in milliseconds, not ticks.
}

void TransitionTimerIntStop()
{
	Timer_IF_Stop(TIMERA0_BASE, TIMER_BOTH);
}

void FinishTimerIntStop()
{
	Timer_IF_Stop(TIMERA1_BASE, TIMER_BOTH);
}

void RepeatTimerIntStop()
{
	Timer_IF_Stop(TIMERA2_BASE, TIMER_BOTH);
}

void TransitionTimerIntHandler()
{
    Timer_IF_InterruptClear(TIMERA0_BASE);

    switch (g_signalMode)
	{
		case SHORT_PROCESSING_MODE:
			g_signalMode = LONG_PROCESSING_MODE;
			g_bit_rep = 0ULL;
			TransitionTimerIntStart(15);
			break;
		case LONG_PROCESSING_MODE:
			// Something is wrong after 3+15=18 ms!
			g_signalMode = NO_PROCESSING_MODE;
			FinishTimerIntStop();
			break;
	}
}

void FinishTimerIntHandler()
{
    Timer_IF_InterruptClear(TIMERA1_BASE);

    g_finishMode = g_signalMode;
	g_signalMode = NO_PROCESSING_MODE;
	g_ticksElapsed = 0UL;
}

void RepeatTimerIntHandler()
{
    Timer_IF_InterruptClear(TIMERA2_BASE);

    g_repeat = 0;
	g_send_key = 1;
}
//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS
//*****************************************************************************

unsigned long long SignalRoutine()
{
	unsigned long long bit_rep;
	unsigned char toggle_bit;

	while(!g_finishMode);

	switch (g_finishMode)
	{
		case SHORT_PROCESSING_MODE:
			// Get toggle bit
			toggle_bit = (g_bit_rep & 0x8000ULL) >> 15;
			// Always set the 16th least-significant bit, the toggle bit, to high
			g_bit_rep |= 0x8000ULL;
			// Only keep the 32+4=36 least significant bits
			g_bit_rep &= 0xFFFFFFFFFULL;
			break;
		case LONG_PROCESSING_MODE:
			// Only keep the 32 least significant bits
			g_bit_rep &= 0xFFFFFFFFULL;
			break;
	}

	g_finishMode = NO_PROCESSING_MODE;

	bit_rep = g_bit_rep;
	g_bit_rep = 0ULL;

	if (bit_rep == g_last_bit_rep && toggle_bit == g_last_toggle)
		return 0ULL;

	g_last_bit_rep = bit_rep;
	g_last_toggle = toggle_bit;

	return bit_rep;
}

typedef enum
{
	BUTTON_INVALID = 0,

	BUTTON_1,
	BUTTON_2,
	BUTTON_3,
	BUTTON_4,
	BUTTON_5,
	BUTTON_6,
	BUTTON_7,
	BUTTON_8,
	BUTTON_9,
	BUTTON_0,

	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_RIGHT,

	BUTTON_CHANNEL_UP,
	BUTTON_CHANNEL_DOWN,

	BUTTON_MUTE,
} button_type;

button_type ButtonRoutine()
{
	unsigned long long bit_rep = SignalRoutine();

	switch (bit_rep)
	{
		case 591439360ULL: return BUTTON_0;
		case 591439361ULL: return BUTTON_1;
		case 591439362ULL: return BUTTON_2;
		case 591439363ULL: return BUTTON_3;
		case 591439364ULL: return BUTTON_4;
		case 591439365ULL: return BUTTON_5;
		case 591439366ULL: return BUTTON_6;
		case 591439367ULL: return BUTTON_7;
		case 591439368ULL: return BUTTON_8;
		case 591439369ULL: return BUTTON_9;

		case 50190375ULL: return BUTTON_UP;
		case 50198535ULL: return BUTTON_DOWN;
		case 50165895ULL: return BUTTON_LEFT;
		case 50157735ULL: return BUTTON_RIGHT;

		case 591439392ULL: return BUTTON_CHANNEL_UP;
		case 591439393ULL: return BUTTON_CHANNEL_DOWN;

		case 50137335ULL: return BUTTON_MUTE;

		default: return BUTTON_INVALID;
	}
}

typedef struct button_press
{
	button_type 	button;
	unsigned char 	control;
	unsigned char 	cycle;
	unsigned char 	ascii;
} button_press;

button_type	g_last_type = BUTTON_INVALID;

button_press PressRoutine()
{
	button_press press;
	button_type button = ButtonRoutine();

	press.button = button;

	if (!button) return press;

	switch (button)
	{
		case BUTTON_1:
		case BUTTON_2:
		case BUTTON_3:
		case BUTTON_4:
		case BUTTON_5:
		case BUTTON_6:
		case BUTTON_7:
		case BUTTON_8:
		case BUTTON_9:
		case BUTTON_0:
		case BUTTON_CHANNEL_UP:
		case BUTTON_CHANNEL_DOWN:
			press.control = 0;
			break;
		default:
			press.control = 1;
			break;
	}

	if(g_last_type == button)
	{
		press.cycle = g_repeat;
		++g_repeat;
	}
	else
	{
		press.cycle = 0;
		g_repeat = 1;
		g_last_type = button;
		g_send_key = 1;
	}
	RepeatTimerIntStart(1000);

	// Do not read from flag g_repeat after this point.
	// Use press.cycle instead.

	switch (button)
	{
		case BUTTON_1:
			if(ttt_mode)
				press.ascii = '1';
			else
				press.ascii = (unsigned char[]){'.', ',', '?', '!', '@', '#', '$', '&', '1'}[press.cycle % 9];
			break;
		case BUTTON_2:
			if(ttt_mode)
				press.ascii = '2';
			else
				press.ascii = (unsigned char[]){'a', 'b', 'c', 'A', 'B', 'C', '2'}[press.cycle % 7];
			break;
		case BUTTON_3:
			if(ttt_mode)
				press.ascii = '3';
			else
				press.ascii = (unsigned char[]){'d', 'e', 'f', 'D', 'E', 'F', '3'}[press.cycle % 7];
			break;
		case BUTTON_4:
			if(ttt_mode)
				press.ascii = '4';
			else
				press.ascii = (unsigned char[]){'g', 'h', 'i', 'G', 'H', 'I', '4'}[press.cycle % 7];
			break;
		case BUTTON_5:
			if(ttt_mode)
				press.ascii = '5';
			else
				press.ascii = (unsigned char[]){'j', 'k', 'l', 'J', 'K', 'I', '5'}[press.cycle % 7];
			break;
		case BUTTON_6:
			if(ttt_mode)
				press.ascii = '6';
			else
				press.ascii = (unsigned char[]){'m', 'n', 'o', 'M', 'N', 'O', '6'}[press.cycle % 7];
			break;
		case BUTTON_7:
			if(ttt_mode)
				press.ascii = '7';
			else
				press.ascii = (unsigned char[]){'p', 'q', 'r', 's', 'P', 'Q', 'R', 'S', '7'}[press.cycle % 9];
			break;
		case BUTTON_8:
			if(ttt_mode)
				press.ascii = '8';
			else
				press.ascii = (unsigned char[]){'t', 'u', 'v', 'T', 'U', 'V', '8'}[press.cycle % 7];
			break;
		case BUTTON_9:
			if(ttt_mode)
				press.ascii = '9';
			else
				press.ascii = (unsigned char[]){'w', 'x', 'y', 'z', 'W', 'X', 'Y', 'Z', '9'}[press.cycle % 9];
			break;
		case BUTTON_0:
			press.ascii = (unsigned char[]){' ', '0'}[press.cycle % 2];
			break;
		case BUTTON_CHANNEL_UP:
			press.ascii = '\n';
			break;
		case BUTTON_CHANNEL_DOWN:
			press.ascii = '\b';
			break;
		case BUTTON_MUTE:
			press.ascii = 176;
			break;
	}

	return press;
}

void ConfigureMessageDisplay()
{
	fillScreen(BLACK);
	drawFastHLine(0, 64, 128, WHITE);
	drawChar(0, 0, '>', WHITE, BLACK, (unsigned char) 1);
	drawChar(6, 0, ' ', WHITE, BLACK, (unsigned char) 1);
	drawChar(0, 80, '>', WHITE, BLACK, (unsigned char) 1);
	drawChar(6, 80, ' ', WHITE, BLACK, (unsigned char) 1);
}

//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void
DisplayBanner(char * AppName)
{

    UART_PRINT("\n\n\n\r");
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\t\t    CC3200 %s Application       \n\r", AppName);
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\n\n\n\r");
}
  
//*****************************************************************************
//
//! Task implementing MQTT client communication to other web client through
//!    a broker
//!
//! \param  none
//!
//! This function
//!    1. Initializes network driver and connects to the default AP
//!    2. Initializes the mqtt library and set up MQTT connection configurations
//!    3. set up the button events and their callbacks(for publishing)
//!    4. handles the callback signals
//!
//! \return None
//!
//*****************************************************************************
void MqttClient(void *pvParameters)
{
    
    long lRetVal = -1;
    int iCount = 0;
    int iNumBroker = 0;
    int iConnBroker = 0;
    osi_messages RecvQue;
    
    unsigned char cChar;
    button_press curr;

    int send_x_cursor = 6;
    int send_y_cursor = 0;

    char sendBuffer[100] = {0};
    int send_index = 0;

    int i;

    connect_config *local_con_conf = (connect_config *)app_hndl;

    //
    // Reset The state of the machine
    //
    Network_IF_ResetMCUStateMachine();

    //
    // Start the driver
    //
    lRetVal = Network_IF_InitDriver(ROLE_STA);
    if(lRetVal < 0)
    {
       UART_PRINT("Failed to start SimpleLink Device\n\r",lRetVal);
       LOOP_FOREVER();
    }

    // Initialize AP security params
    SecurityParams.Key = (signed char *)SECURITY_KEY;
    SecurityParams.KeyLen = strlen(SECURITY_KEY);
    SecurityParams.Type = SECURITY_TYPE;

    //
    // Connect to the Access Point
    //
    lRetVal = Network_IF_ConnectAP(SSID_NAME, SecurityParams);
    if(lRetVal < 0)
    {
       UART_PRINT("Connection to an AP failed\n\r");
       LOOP_FOREVER();
    }

    set_time();
     if(lRetVal < 0)
         {
             UART_PRINT("Unable to set time in the device");
     }

    UtilsDelay(20000000);
    //
    // Initialze MQTT client lib
    //
    lRetVal = sl_ExtLib_MqttClientInit(&Mqtt_Client);
    if(lRetVal != 0)
    {
        // lib initialization failed
        UART_PRINT("MQTT Client lib initialization failed\n\r");
        LOOP_FOREVER();
    }
    
    /******************* connection to the broker ***************************/
    iNumBroker = sizeof(usr_connect_config)/sizeof(connect_config);
    if(iNumBroker > MAX_BROKER_CONN)
    {
        UART_PRINT("Num of brokers are more then max num of brokers\n\r");
        LOOP_FOREVER();
    }

    while(iCount < iNumBroker)
    {
        //create client context
        local_con_conf[iCount].clt_ctx =
        sl_ExtLib_MqttClientCtxCreate(&local_con_conf[iCount].broker_config,
                                      &local_con_conf[iCount].CallBAcks,
                                      &(local_con_conf[iCount]));

        //
        // Set Client ID
        //
        sl_ExtLib_MqttClientSet((void*)local_con_conf[iCount].clt_ctx,
                            SL_MQTT_PARAM_CLIENT_ID,
                            local_con_conf[iCount].client_id,
                            strlen((char*)(local_con_conf[iCount].client_id)));

        //
        // Set will Params
        //
        if(local_con_conf[iCount].will_params.will_topic != NULL)
        {
            sl_ExtLib_MqttClientSet((void*)local_con_conf[iCount].clt_ctx,
                                    SL_MQTT_PARAM_WILL_PARAM,
                                    &(local_con_conf[iCount].will_params),
                                    sizeof(SlMqttWill_t));
        }

        //
        // setting username and password
        //
        if(local_con_conf[iCount].usr_name != NULL)
        {
            sl_ExtLib_MqttClientSet((void*)local_con_conf[iCount].clt_ctx,
                                SL_MQTT_PARAM_USER_NAME,
                                local_con_conf[iCount].usr_name,
                                strlen((char*)local_con_conf[iCount].usr_name));

            if(local_con_conf[iCount].usr_pwd != NULL)
            {
                sl_ExtLib_MqttClientSet((void*)local_con_conf[iCount].clt_ctx,
                                SL_MQTT_PARAM_PASS_WORD,
                                local_con_conf[iCount].usr_pwd,
                                strlen((char*)local_con_conf[iCount].usr_pwd));
            }
        }

        //
        // connectin to the broker
        //
        if((sl_ExtLib_MqttClientConnect((void*)local_con_conf[iCount].clt_ctx,
                            local_con_conf[iCount].is_clean,
                            local_con_conf[iCount].keep_alive_time) & 0xFF) != 0)
        {
            UART_PRINT("\n\rBroker connect fail for conn no. %d \n\r",iCount+1);
            
            //delete the context for this connection
            sl_ExtLib_MqttClientCtxDelete(local_con_conf[iCount].clt_ctx);
            
            break;
        }
        else
        {
            UART_PRINT("\n\rSuccess: conn to Broker no. %d\n\r ", iCount+1);
            local_con_conf[iCount].is_connected = true;
            iConnBroker++;
        }

        //
        // Subscribe to topics
        //

        if(sl_ExtLib_MqttClientSub((void*)local_con_conf[iCount].clt_ctx,
                                   local_con_conf[iCount].topic,
                                   local_con_conf[iCount].qos, TOPIC_COUNT) < 0)
        {
            UART_PRINT("\n\r Subscription Error for conn no. %d\n\r", iCount+1);
            UART_PRINT("Disconnecting from the broker\r\n");
            sl_ExtLib_MqttClientDisconnect(local_con_conf[iCount].clt_ctx);
            local_con_conf[iCount].is_connected = false;
            
            //delete the context for this connection
            sl_ExtLib_MqttClientCtxDelete(local_con_conf[iCount].clt_ctx);
            iConnBroker--;
            break;
        }
        else
        {
            int iSub;
            UART_PRINT("Client subscribed on following topics:\n\r");
            for(iSub = 0; iSub < local_con_conf[iCount].num_topics; iSub++)
            {
                UART_PRINT("%s\n\r", local_con_conf[iCount].topic[iSub]);
            }
        }
        iCount++;
    }

    if(iConnBroker < 1)
    {
        //
        // no succesful connection to broker
        //
        goto end;
    }

    iCount = 0;

    // Set up all interrupts, handlers, etc.
    // related to the IR Remote decoding.
    ConfigureMessageDisplay();
	SystickInit();
	PhotoresistorIntInit();
	PhotoresistorIntEnable();
	AllTimersIntInit();

	// Begin sending messages over MQTT.
    for(;;)
    {
    	curr = PressRoutine();
    	PhotoresistorIntDisable();

    	if(curr.button != BUTTON_INVALID)
    	{
    		cChar = curr.ascii;
    		if(!ttt_mode)
    		{
				if(curr.cycle != 0 && cChar != '\b' && cChar != '\n') // Multiple presses.
				{
					send_index--;

					drawChar(send_x_cursor, send_y_cursor, cChar, WHITE, BLACK, (unsigned char) 1);
					sendBuffer[send_index++] = cChar;
				}
				else
				{
					if(cChar == '\n') // end of message.
					{
						sendBuffer[send_index++] = '\0';
						fillRect(12, 0, SSD1351WIDTH, SSD1351HEIGHT / 2, BLACK);
						send_x_cursor = 6;
						send_y_cursor = 0;

						sl_ExtLib_MqttClientSend((void*)local_con_conf[iCount].clt_ctx,
								pub_topic_sender,(const void*)sendBuffer,strlen(sendBuffer),QOS1,RETAIN);

						UART_PRINT("\n\r CC3200 Publishes the following message \n\r");
						UART_PRINT("Topic: %s\n\r", pub_topic_sender);
						UART_PRINT("Data: %s\n\r", sendBuffer);

						send_index = 0;
					}
					else if(cChar == '\b') 	// deleting a character.
					{
						drawChar(send_x_cursor, send_y_cursor, ' ', WHITE, BLACK, (unsigned char) 1);

						if(send_index >= 1) send_index--;

						send_x_cursor -= 6; 	// go back one
						if(send_x_cursor < 6) 	// if we're about to go off screen.
						{
							send_y_cursor -= 8; 	// previous line.
							send_x_cursor = 120; // last character of line.

							if(send_y_cursor < 0) // Went off top edge.
							{
								send_x_cursor = 6;
								send_y_cursor = 0;
							}
						}
					}
					else if(cChar == TIC_TAC_TOE_CODE) // Initiate Tic Tac Toe.
					{
						fillScreen(BLACK);

						send_index = 0;
						send_x_cursor = 6;
						send_y_cursor = 0;

						sl_ExtLib_MqttClientSend((void*)local_con_conf[iCount].clt_ctx,
								pub_topic_sender,(const void*)"TTT#MODE",strlen("TTT#MODE"),QOS1,RETAIN);

						UART_PRINT("\n\r CC3200 Publishes the following message \n\r");
						UART_PRINT("Topic: %s\n\r", pub_topic_sender);
						UART_PRINT("Data: %s\n\r", "TIC TAC TOE!");

						setCursor(0,0);
						Outstr(" | |     Player: X");
						setCursor(0,8);
						Outstr("-+-+-");
						setCursor(0,16);
						Outstr(" | | ");
						setCursor(0,24);
						Outstr("-+-+-");
						setCursor(0,32);
						Outstr(" | | ");

	    	    		setCursor(0,48);
	    	    		setTextColor(BLUE,BLACK);
	    	    		Outstr("Your turn         ");

	    	    		setCursor(0,64);
	    	    		Outstr("Rules:");
	    	    		setCursor(0,72);
	    	    		Outstr("1) Wait for your turn");
	    	    		setCursor(0,80);
	    	    		Outstr("2) Use number keys to");
	    	    		setCursor(0,88);
	    	    		Outstr("   select your choice");

						move_count = 0;
						your_turn = true;
						ttt_mode = true;
						player = PLAYER_X;
						opp = PLAYER_O;
						playerChar = 'X';
				    	oppChar = 'O';
					}
					else
					{
						send_x_cursor += 6;

						if(send_x_cursor > 120) // About to go off right edge.
						{
							send_x_cursor = 12;
							send_y_cursor += 8; // go to next line.
						}

						if(send_y_cursor > 56) // About to go off bottom edge.
						{
							fillRect(12, 0, SSD1351WIDTH, SSD1351HEIGHT / 2, BLACK);
							send_x_cursor = 12;
							send_y_cursor = 0; // start at top.
						}

						drawChar(send_x_cursor, send_y_cursor, cChar, WHITE, BLACK, (unsigned char) 1);
						sendBuffer[send_index++] = cChar;
					}
				}
    		}
    		else // We're in tic tac toe mode.
    		{
    			if(your_turn) // Only want to do something if it's actually your turn.
    			{
    				if(cChar >= '1' && cChar <= '9') // If cChar is a valid choice
    				{
    					sendBuffer[0] = cChar;
    					sendBuffer[1] = '\0';

    					if(board[atoi(sendBuffer)] == FREE) // If that board space is free.
    					{
    						move_count++;
    						board[atoi(sendBuffer)] = player;

    						// Update board.
    						switch(atoi(sendBuffer))
    						{
    						    case 1:
    						    	drawChar(0, 0, playerChar, BLUE, BLACK, (unsigned char) 1);
    						    	break;
    						    case 2:
    						    	drawChar(12, 0, playerChar, BLUE, BLACK, (unsigned char) 1);
    						    	break;
    						    case 3:
    						    	drawChar(24, 0, playerChar, BLUE, BLACK, (unsigned char) 1);
    						    	break;
    						    case 4:
    						    	drawChar(0, 16, playerChar, BLUE, BLACK, (unsigned char) 1);
    						    	break;
    						    case 5:
    					  			drawChar(12, 16, playerChar, BLUE, BLACK, (unsigned char) 1);
    					   			break;
    					   		case 6:
    				    			drawChar(24, 16, playerChar, BLUE, BLACK, (unsigned char) 1);
   					    			break;
    				    		case 7:
    				    			drawChar(0, 32, playerChar, BLUE, BLACK, (unsigned char) 1);
   					    			break;
 					    		case 8:
  					    			drawChar(12, 32, playerChar, BLUE, BLACK, (unsigned char) 1);
  					    			break;
 					    		case 9:
   					    			drawChar(24, 32, playerChar, BLUE, BLACK, (unsigned char) 1);
   					    			break;
  						    }

    						// Transmit choice.
    						sl_ExtLib_MqttClientSend((void*)local_con_conf[iCount].clt_ctx,
    								pub_topic_sender,(const void*)sendBuffer,strlen(sendBuffer),QOS1,RETAIN);

    						UART_PRINT("\n\r CC3200 Publishes the following message \n\r");
    						UART_PRINT("Topic: %s\n\r", "Your Tic Tac Toe Move");
    						UART_PRINT("Data: %s\n\r", sendBuffer);


    						// Check if you won
    						if((board[1] == board[5] && board[1] == board[9] && board[1] != 0) ||
    					       (board[3] == board[5] && board[3] == board[7] && board[3] != 0) ||
    						   (board[1] == board[2] && board[1] == board[3] && board[1] != 0) ||
    					       (board[4] == board[5] && board[4] == board[6] && board[4] != 0) ||
    						   (board[7] == board[8] && board[7] == board[9] && board[7] != 0) ||
    						   (board[1] == board[4] && board[4] == board[7] && board[1] != 0) ||
    						   (board[2] == board[5] && board[5] == board[8] && board[2] != 0) ||
    						   (board[3] == board[6] && board[6] == board[9] && board[3] != 0))
    					    {
    							UtilsDelay(20000000);
    					    	ConfigureMessageDisplay();
    					   		setCursor(12,80);
    					   		Outstr("You Win!");
    					   		ttt_mode = false;
    					   		for(i = 1; i <= 9; i++)
    					   			board[i] = FREE;
    					   		setTextColor(WHITE,BLACK);
    					    }
    					    else if(move_count == 9)
    					    {
    					    	UtilsDelay(20000000);
    					    	ConfigureMessageDisplay();
    					    	setCursor(12,80);
    					    	Outstr("Tie game!");
    					    	ttt_mode = false;
    					    	for(i = 1; i <= 9; i++)
    					    		board[i] = FREE;
    					    	setTextColor(WHITE,BLACK);
    					    }
    					    else
    					    {
    					    	your_turn = false;
    					    	setCursor(0,48);
    					    	setTextColor(RED,BLACK);
    					    	Outstr("Opponents turn");
    					    }
    					}
    				}
    			}
    		}
 		}
    	PhotoresistorIntEnable();

        if(BROKER_DISCONNECTION == RecvQue)
        {
            iConnBroker--;
            if(iConnBroker < 1)
            {
                //
                // device not connected to any broker
                //
                goto end;
            }
        }
    }
end:

    //
    // Deinitializating the client library
    //
    sl_ExtLib_MqttClientExit();
    UART_PRINT("\n\r Exiting the Application\n\r");
    
    LOOP_FOREVER();
}

//*****************************************************************************
//
//! Main 
//!
//! \param  none
//!
//! This function
//!    1. Invokes the SLHost task
//!    2. Invokes the MqttClient
//!
//! \return None
//!
//*****************************************************************************
void main()
{ 
    long lRetVal = -1;
    //
    // Initialize the board configurations
    //
    BoardInit();

    PinMuxConfig();

    PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    //
    // Configuring UART
    //
    InitTerm();

    ClearTerm();

    //
    // Reset the SPI peripheral
    //
    PRCMPeripheralReset(PRCM_GSPI);

    // SPI reset, initialize
    SPIReset(GSPI_BASE);
    SPIFIFOEnable(GSPI_BASE, SPI_TX_FIFO || SPI_RX_FIFO);
    SPIConfigSetExpClk(GSPI_BASE, PRCMPeripheralClockGet(PRCM_GSPI),
    				   SPI_IF_BIT_RATE, SPI_MODE_MASTER, SPI_SUB_MODE_0,
					   (SPI_SW_CTRL_CS | SPI_4PIN_MODE | SPI_TURBO_OFF |
					    SPI_CS_ACTIVELOW | SPI_WL_8));

    SPIEnable(GSPI_BASE);

    // Adafruit initialize
    Adafruit_Init();

    //
    // Display Application Banner
    //
    DisplayBanner("Wifi Communication w/ MQTT");

    //
    // Start the SimpleLink Host
    //
    lRetVal = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }

    //
    // Start the MQTT Client task
    //
    osi_MsgQCreate(&g_PBQueue,"PBQueue",sizeof(osi_messages),10);
    lRetVal = osi_TaskCreate(MqttClient,
                            (const signed char *)"Mqtt Client App",
                            OSI_STACK_SIZE, NULL, 2, NULL );

    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
    //
    // Start the task scheduler
    //
    osi_start();
}

//*****************************************************************************
//
//! This function updates the date and time of CC3200.
//!
//! \param None
//!
//! \return
//!     0 for success, negative otherwise
//!
//*****************************************************************************

static int set_time()
{
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}
