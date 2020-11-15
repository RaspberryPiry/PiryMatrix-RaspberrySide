#include <wiringPi.h>
#include <stdio.h>
#include <wiringShift.h>

// pin defines
//
// ---- Shift REG PINS ----
// DS 0
// OE 1(PWM)
// STCP 2
// SHCP 3
// MR : VCC+
//

#define DATA 0
#define OE 1

#define LATCH 2
#define CLOCK 3


// main

int main(void){
	wiringPiSetup();

	unsigned char row[16] = {
		0b00000001, 0b00000010, 0b00000100, 0b00001000, 
	       	0b00010000, 0b00100000, 0b01000000, 0b10000000,
		0b00000001, 0b00000010, 0b00000100, 0b00001000,
		0b00010000, 0b00100000, 0b01000000, 0b10000000};
	unsigned char col[16] = {
		0b01111111, 0b11111110, 
		0b11111111, 0b11111111,
		0b11111111, 0b11111111, 
		0b11111111, 0b11111111,
		0b11111111, 0b11111111, 
		0b11111111, 0b11111111,
		0b11111111, 0b11111111, 
		0b01111111, 0b11111110};


	// for PWM(OE)
	pinMode(OE, PWM_OUTPUT);
	
	pinMode(LATCH, OUTPUT);
	pinMode(CLOCK, OUTPUT);
	pinMode(DATA, OUTPUT);
	

	// 
	while(1){
		
		for(int i = 0; i <16; i++){
		//printf("col data : %c \n", col[15-i]);
		//printf("row data : %c \n", row[i]);
		digitalWrite(LATCH, LOW);
		shiftOut(DATA, CLOCK, LSBFIRST, col[15-i]);
		shiftOut(DATA, CLOCK, LSBFIRST, row[i]);	
		digitalWrite(LATCH, HIGH);
		//delay(1);	
		}
	}



}

