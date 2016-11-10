/* 
* A123Module.h
*
* Created: 14.03.2016 16:02:51
* Author: KZM
*/


#ifndef __A123MODULE_H__
#define __A123MODULE_H__
//#include "PinConfig.h"
#include <Arduino.h>
#include <mcp_can.h>
#include <SPI.h>


#define CAN_CS 10 //ChipSelect Pin

struct module{
	uint8_t alive;
	uint8_t errorcounter;
	uint16_t maxvoltage;
	uint16_t minvoltage;
	uint16_t avevoltage;
	int16_t temperature;
	uint8_t msg_id_recv;
	uint8_t msg_id_send;
	uint8_t mod_bal_cnt;
	boolean undervoltage;
	boolean overvoltage;
};

class A123Module
{
	//variables
	public:
		uint8_t msg_id_send;
		boolean running;
		struct module modules[15];
		MCP_CAN CAN;
	protected:
	private:
		uint8_t fastbuf[15][8];
		uint16_t bufID[8];

	//functions
	public:
		A123Module();
		A123Module(uint8_t PinCS);
		~A123Module();
		boolean start();
		void stop();
		void send_balance(uint16_t voltage);
		void ReceiveCAN();
		void DecodeCAN();
	protected:
	private:
		A123Module( const A123Module &c );
		A123Module& operator=( const A123Module &c );
		void setBits(unsigned int startBit, unsigned int length, unsigned char *buf, unsigned int data);
		unsigned int getBits(int startBit, int length, unsigned char *buf);
		void UpdateValues();
}; //A123Module

#endif //__A123MODULE_H__
