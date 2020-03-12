//  square.c: Uses timer 2 interrupt to generate a square wave in pin
//  P2.0 and a 75% duty cycle wave in pin P2.1
//  Copyright (c) 2010-2018 Jesus Calvino-Fraga
//  ~C51~

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <EFM8LB1.h>

// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L

#define OUT0 P2_0
#define OUT1 P2_1

volatile unsigned char pwm_count=0;
int pwm1 = 0;
int pwm2 = 0;
int direction = 0;
int power = 0;

char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key

	VDM0CN=0x80;       // enable VDD monitor
	RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD

	#if (SYSCLK == 48000000L)	
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif
	
	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)	
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif
	
	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)                     
	XBR1     = 0X00;
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	// Configure Uart 0
	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	SCON0 = 0x10;
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready

	// Initialize timer 2 for periodic interrupts
	TMR2CN0=0x00;   // Stop Timer2; Clear TF2;
	CKCON0|=0b_0001_0000; // Timer 2 uses the system clock
	TMR2RL=(0x10000L-(SYSCLK/10000L)); // Initialize reload value
	TMR2=0xffff;   // Set to reload immediately
	ET2=1;         // Enable Timer2 interrupts
	TR2=1;         // Start Timer2 (TMR2CN is bit addressable)

	EA=1; // Enable interrupts

  	
	return 0;
}

void Timer2_ISR (void) interrupt 5
{
	TF2H = 0; // Clear Timer2 interrupt flag
	
	pwm_count++;
	if(pwm_count>100) pwm_count=0;

	if(direction == 1)
	{
		pwm1 = power;
		pwm2 = 0;
	}
	else
	{
		pwm1 = 0;
		pwm2 = power;
	}
	
	OUT0=pwm_count>pwm1?0:1;
	OUT1=pwm_count>pwm2?0:1;
}

int getsn (char * buff, int len)
{
	int j;
	char c;
	
	for(j=0; j<(len-1); j++)
	{
		c=getchar();
		if ( (c=='\n') || (c=='\r') )
		{
			buff[j]=0;
			return j;
		}
		else
		{
			buff[j]=c;
		}
	}
	buff[j]=0;
	return len;
}

// Raises x to the power of y (apparently this doesn't exist in math.h)
// Does not work for negative y because I am lazy
int pow_(int x, int y)
{
	int temp = x;
	if (y < 0) return 0;
	if (y == 0) return 1;
	while(y > 1)
	{
		temp *= x;
		y--;
	}
	return temp;
}

// Parses string into positive integer between min and max
// Returns integer parsed or -1 on fail
int parse_input (char * input, int min, int max)
{
	int length = strlen(input);
	int val = 0;
	int temp;
	int i = 0;

	// Failure if input is null
	if(input == NULL) return -1;

	// Loop through each character in string
	while(input[i] != '\0')
	{
		// Check if character is a number
		temp = input[i] - '0';
		if((temp >= 0) && (temp <= 9))
		{
			// Value of current char is temp*10^(length-i-1), add that to return value
			val += temp * pow_(10, length - i - 1);
		}
		else 
		{
			printf("\r\nInvalid input: Wrong format or not integers");
			return -1;
		}
		i++;
	}

	// Failure if value is not between min and max
	if((val < min) || (val > max))
	{
		printf("\r\nInvalid input: Numbers outside range");
		return -1;
	}

	return val;
}

void main (void)
{
	char buff[33];
	int temp_direction = 0;
	int temp_power = 0;

	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
	printf("\x1b[;f"); // Reset cursor position

	while(1)
	{
		// Get direction from user and parse
		printf("Enter 0 to turn clockwise, 1 to turn counterclockwise: ");
		getsn(buff, sizeof(buff));
		temp_direction = parse_input(buff, 0, 1);

		// Get power from user and parse
		printf("\r\nEnter power from 0 to 100: ");
		getsn(buff, sizeof(buff));
		temp_power = parse_input(buff, 0, 100);

		if((temp_direction != -1) && (temp_power != -1))
		{
			printf("\r\nSuccessfully received: %s, %d%% power", temp_direction?"counterclockwise":"clockwise", temp_power);
			direction = temp_direction;
			power = temp_power;
		}
		else printf("\r\nFailure, one or more errors detected.");

		printf("\r\nPress Enter to continue...");
		getsn(buff, sizeof(buff));
		printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
		printf("\x1b[;f"); // Reset cursor position
	}
}
