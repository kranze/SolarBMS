/* 
* A123Module.cpp
*
* Created: 14.03.2016 16:02:50
* Author: KZM
*/


#include "A123Module.h"


#define BCM_CMD_ID 0x50
#define MOD_V_OFFSET 1000
#define MOD_THERM_OFFSET -40

boolean Flag_Recv;
uint8_t globalbuffer[64];

uint16_t A123Module::getVoltage()
{
	return(avevoltage*7);
}

void A123Module::setBits(unsigned int startBit, unsigned int length, unsigned char *buf, unsigned int data)
{
	unsigned int endBit = startBit + length - 1;
	for  (int i = 0; i < length; i++) {
		unsigned maskedData = data & 1;
		if (maskedData) {
			unsigned char mask = maskedData << (7 - ((endBit - i) % 8));
			buf[(endBit - i) / 8] = buf[(endBit - i) / 8] | mask;
			} else {
			unsigned char mask = ~(~maskedData << (7 - ((endBit - i) % 8)));
			buf[(endBit - i) / 8] = buf[(endBit - i) / 8] & mask;
		}
		data = data >> 1;
	}
}

unsigned int A123Module::getBits(int startBit, int length, unsigned char *buf) {
	unsigned int val = 0;
	unsigned char startBitByte = startBit / 8;
	unsigned char bitShift = 0;
	unsigned char currentBit = startBit % 8;
	unsigned char currentByte = buf[startBitByte];
	
	if (length <= 8) {
		unsigned char mask = 0;
		for (char i = 0; i < length; i++) {
			mask += 1 << (currentBit + i);
		}
		val = (currentByte & mask) >> currentBit ;
		} else {
		while (length > 0) {
			val += (currentByte >> currentBit) << (bitShift);
			bitShift += 8 - currentBit;
			length -= 8 - currentBit;
			currentBit = 0;
			startBitByte -= 1;
			currentByte = buf[startBitByte];
		}
	}
	return val;
}


boolean A123Module::start()
{
	if ( CAN_OK != CAN.begin(CAN_500KBPS, MCP_16MHz) ){
		Serial.println("CAN NOT OK");
		return(false);
	}
	else {
		CAN.init_Mask(0,0,0x300);
		CAN.init_Mask(1,0,0x300);
		CAN.init_Filt(0,0,0x200);
		CAN.init_Filt(1,0,0x000);
		CAN.init_Filt(2,0,0x000);
		CAN.init_Filt(3,0,0x000);
		CAN.init_Filt(4,0,0x000);
		CAN.init_Filt(5,0,0x000);
		Serial.println("CAN OK");
		 // start interrupt
		return(true);
	}
}

void A123Module::stop(){
	running=false;
	SPI.end(); 
	pinMode(CAN_CS,OUTPUT); digitalWrite(CAN_CS,LOW);
}

void A123Module::send_balance(uint16_t voltage){
	
	unsigned char buf[8]={0x00, 0x00, 0x00, 0x00, 0x10, 0, 0, 0};
	
	unsigned int data;
	
	msg_id_send++;
	if (msg_id_send>15){
		msg_id_send=0;
	}
	setBits(0,4,buf,msg_id_send);
	voltage=min(voltage,3550);
		
	if (voltage<=2400){
		voltage=3550;
	}

	data=(voltage-MOD_V_OFFSET)/0.5;
	setBits(8,13,buf,data);
		
	this->CAN.sendMsgBuf(BCM_CMD_ID,0,8,buf);
}


void A123Module::ReceiveCAN(){
	
	uint8_t len=0;
	uint8_t i=0;	
	while (CAN_MSGAVAIL == CAN.checkReceive()) {
		CAN.readMsgBuf(&len, fastbuf[i]);    // read data,  id: message id, len: data length, buf: data buf
		bufID[i]=CAN.getCanId();
		i++;
		if (i>8){
			break;
		}
	}
	/*for (i=0;i<8;i++){
		Serial.print(bufID[i],HEX);
		Serial.print(" ");
	}
	Serial.println();
	*/
}

void A123Module::DecodeCAN(){
	
		uint8_t mux_chn;
		uint8_t num_mod=0;
		for (int i=0;i<8;i++){
			
			if ( (bufID[i] > 0x200) && (bufID[i] < 0x20F) ){
				num_mod = (bufID[i] & 0xFF);
				for (int j =0; j<8;j++){
				}				
				this->modules[num_mod].alive = true;
				mux_chn=getBits(0,4,fastbuf[i]);
				this->modules[num_mod].msg_id_recv = getBits(4, 4, fastbuf[i]);
				this->modules[num_mod].minvoltage = getBits(27, 13, fastbuf[i]) * 0.5 + MOD_V_OFFSET;
				this->modules[num_mod].maxvoltage = getBits(43, 13, fastbuf[i]) * 0.5 + MOD_V_OFFSET;
				this->modules[num_mod].avevoltage = getBits(59, 13, fastbuf[i]) * 0.5 + MOD_V_OFFSET;
				if (mux_chn==0xB)
					this->modules[num_mod].mod_bal_cnt = getBits(8, 8, fastbuf[i]);
				if ( getBits(24, 3, fastbuf[i]) == 2) {
					this->modules[num_mod].temperature = getBits(8, 8, fastbuf[i]) * 0.5 + MOD_THERM_OFFSET;
				}
			}
		}
	}


// default constructor
A123Module::A123Module():CAN(CAN_CS)
{
	this->minvoltage=3600;
} //A123Module

 A123Module::A123Module(uint8_t PinCS)
 :CAN(PinCS)
{
	this->minvoltage=3600;
}

// default destructor
A123Module::~A123Module()
{
} //~A123Module
