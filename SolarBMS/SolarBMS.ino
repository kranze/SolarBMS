/* A123 BMS */
/*

   |__| \    / o
   |  |  \/\/  o

 GND--- S -----.___      .---- D ---[BATT-]
                |__\_ __|
                 _______                      ___________________
                    |     _________          |    ARDUINO UNO    |         _____________
    +12V          GATE 0 | ULN2803 |  +12V---| VIN               |        |   MCP2515   |
     |	            |    |         |         |               +5V |--------| VCC         |
     0--| R2k2 |----0----| 18    1 |<--------| A0/14          13 |------->| SCK         |_      _
     |                   |         |         |                12 |<-------| MISO   CANH | \/\/\/
     |           GATE 1  |         |         |                11 |------->| MOSI   CANL |_/\/\/\_
     |              |    |         |         |                10 |------->| CS          |
     0--| R2k2 |----0----| 17    2 |<--------| A1/15           2 |<-------| INT         |
     |                   |         |         |               GND |--------| GND         |
     |           GATE 2  |         |         |                   |        |_____________|
     |              |    |         |         |                   |
     0--| R2k2 |----0----| 16    3 |<--------| A2/16             |                       //RED
     |                   |         |         |                 7 |------->| R560 |----| LED |------0
     |           GATE 3  |         |         |                   |                       //YELLOW  |
     |              |    |         |         |                 6 |------->| R560 |----| LED |------0
     0--| R2k2 |----0----| 15    4 |<--------| A3/17             |                       //GREEN   |
     |                   |         |         |                 5 |------->| R560 |----| LED |------0
     0-------------------| 10    9 |----0----| GND               |                                 |
                         |_________|    |    |                 1 |------->| TX                     |
                                        |    |                 0 |<-------| RX                     |
                                       _|_   |___________________|                                _|_
                                       GND                                                        GND
      __
     (__ \    / o
     ___) \/\/  o

Grobe Funktionsbeschreibung:

StartUp:
Nach den nötigen Initialisierungroutinen wird  über den CAN-Bus eine Nachricht mit der ID 0x50 verschickt.
Das versenden der Nachricht erledigt die Methode "send_balance()" der Klasse "A123Module"
Alle Angeschlossenen A123 Module sollten daraufhin antworten.
Die Methode "Decode_CAN()" decocodiert die NAchrichten und legt alle Module und Signale in der Struktur "modules" ab.

*/

//#include <Event.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <mcp_can_dfs.h>
#include <mcp_can.h>
#include "A123Module.h"

#define ABSMAXVOLTAGE 3650
#define ABSMINVOLTAGE 2100

#define MAXMODULES 15
#define INSTALLEDMODULES 8

#define RedLED 7
#define YellowLED 6
#define GreenLED 5

A123Module Battery(10);

uint16_t maxcellvoltage;
uint16_t mincellvoltage;
uint8_t myid;
uint8_t installedmudules=8;

uint16_t EEMEM eeMAXCELLVOLTAGE;
uint16_t EEMEM eeMINCELLVOLTAGE;
uint8_t EEMEM eeMYID;
uint8_t EEMEM eeINSTALLEDMODULES;
uint8_t EEMEM eePORTPIN[16];

uint8_t errorcounter=0;
uint16_t balance_target=3600;
uint16_t avl_maxcellvoltage=0;
uint16_t avl_mincellvoltage=3600;
uint8_t counter=0;
StaticJsonBuffer<200> jsonOutBuffer;
JsonObject& root_out = jsonOutBuffer.createObject();
JsonArray& modules = root_out.createNestedArray("modules");

uint8_t moduleIDs[MAXMODULES];
uint8_t moduleCount=0;

//Zuordnung von PortPin auf ModulID
uint8_t PortPin[16];

void CAN_ISR()
{
	Battery.ReceiveCAN();
}


void setup()
{

  /* add setup code here */

	/* EEPROM Einlesesen */

	//einlesen der Zuordnung welches Modul hinter welchem MOSFET hängt

	for (int i=0; i<16;i++){
		PortPin[i]=eeprom_read_byte(&eePORTPIN[i]);
	}
	// Einlesen der Spannungsgrenzen vom EEPROM
	maxcellvoltage=eeprom_read_word(&eeMAXCELLVOLTAGE);

	// Sicherheitschecks falls der EEPROM unitiallisiert ist (erste Ausführung):
	if (maxcellvoltage > ABSMAXVOLTAGE || maxcellvoltage < ABSMINVOLTAGE){
		maxcellvoltage=ABSMAXVOLTAGE;
		eeprom_update_word(&eeMAXCELLVOLTAGE, maxcellvoltage);
	}
	mincellvoltage=eeprom_read_word(&eeMINCELLVOLTAGE);
	if (mincellvoltage < ABSMINVOLTAGE || mincellvoltage > ABSMAXVOLTAGE){
		mincellvoltage=ABSMINVOLTAGE;
		eeprom_update_word(&eeMINCELLVOLTAGE, mincellvoltage);
	}

	// Einlesen der eingenen ID vom EEPROM
	myid=eeprom_read_byte(&eeMYID);

	pinMode(RedLED, OUTPUT);
	pinMode(YellowLED, OUTPUT);
	pinMode(GreenLED, OUTPUT);

	//GATE Mosfets
	for (int i=0; i<6;i++){
		pinMode(i+14, OUTPUT); digitalWrite(i+14,HIGH);
	}
	//Alle LEDs an!
	digitalWrite(RedLED, HIGH);
	digitalWrite(YellowLED, HIGH);
	digitalWrite(GreenLED, HIGH);

	//befuellen der JSON daten
	root_out["myID"]=myid;

	//Mit der Aussenwelt reden
	Serial.begin(115200);
	delay(100);
	Serial.println("Auf gehts!");
	Serial.print("Meine ID: "); Serial.println(myid);
	Serial.println("Konfiguration:");
	Serial.print("Maximale Zellspannung(mV): "); Serial.println(maxcellvoltage);
	Serial.print("Minimale Zellspannung(mV): "); Serial.println(mincellvoltage);

	//INIT die CAN Kommunikation
	Battery.start();
	attachInterrupt(INT0, CAN_ISR, FALLING);

	delay(100);
	//Mal checken was für Module da so sind...
	//Wir brauchen exakt
	while (moduleCount != installedmudules){
		Battery.send_balance(3600);
		delay(16);
		Battery.DecodeCAN();
		moduleCount=0;
		for (int i=0; i<16; i++){

			if ( Battery.modules[i].alive ){
				Serial.print("Modul "); Serial.print(moduleCount); Serial.print(": 0x"); Serial.print(i,HEX);
				Serial.print(" -> "); Serial.println(PortPin[i]);
				moduleIDs[moduleCount]=i;
				moduleCount++;
			}
		}
		Serial.print(moduleCount); Serial.println(" Module gefunden");
		delay(1000);
	}

	for (int i=0; i<16; i++){

		if ( Battery.modules[i].alive ){
			modules.add(i);
		}
	}
}

void loop(){

	delay(100);
	step();
}

void step(){

	uint16_t temp_minCV=3600;
	uint16_t temp_maxCV=0;
	uint8_t temp_minCVMod=0;
	uint8_t temp_maxCVMod=0;

	float voltage=0;
	uint8_t mod_bal_cnt=0;
	uint8_t PinEnable[6]={0,0,0,0,0,0};
	if (avl_mincellvoltage > maxcellvoltage && avl_mincellvoltage < ABSMAXVOLTAGE){
		Battery.send_balance(avl_mincellvoltage+1);
	}
	else{
		Battery.send_balance(maxcellvoltage);
	}
	delay(16);
	Battery.DecodeCAN();

	for (int i=0; i<moduleCount; i++){
		uint8_t id = moduleIDs[i];
		if ( Battery.modules[id].alive ){

			if (Battery.modules[id].msg_id_recv != Battery.msg_id_send){
				Battery.modules[id].errorcounter++;
			}
			else{
				Battery.modules[id].errorcounter=0;
			}
			mod_bal_cnt+=Battery.modules[id].mod_bal_cnt;

			voltage+=Battery.modules[id].avevoltage;

			if (Battery.modules[id].minvoltage < temp_minCV){
				temp_minCV = Battery.modules[id].minvoltage;
				temp_minCVMod = id;
			}
			if (Battery.modules[id].maxvoltage > temp_maxCV){
				temp_maxCV = Battery.modules[id].maxvoltage;
				temp_maxCVMod = id;
			}

			if (Battery.modules[id].maxvoltage > ABSMAXVOLTAGE || Battery.modules[id].minvoltage <= ABSMINVOLTAGE || Battery.modules[id].errorcounter > 6 ){
				//Spannung ausserhalb des gültigen bereichs
				PinEnable[PortPin[id]-14]=HIGH;
			}
			else {
				//


				//if (digitalRead(PortPin[id]-14)==HIGH && Battery.modules[id].maxvoltage > maxcellvoltage){
				//	PinEnable[PortPin[id]-14]=HIGH;
			//	}
			}
		}
	}
	uint8_t redled=LOW;
	int bits=0;
	for (int i=14; i < 20; i++){
			if (PinEnable[i-14]==HIGH){
				redled=HIGH;
				bits|=1<<(i-14);
			}
			digitalWrite(i,PinEnable[i-14]);
	}
	digitalWrite(RedLED,redled);
	root_out["myID"]=myid;
	root_out["Portsa"]=bits;
	voltage=((voltage*14)/moduleCount)/1000.0;
	avl_mincellvoltage=temp_minCV;
	avl_maxcellvoltage=temp_maxCV;
	root_out["AvlMinCV"]=avl_mincellvoltage;
	root_out["AvlMinCVMod"]=temp_minCVMod;
	root_out["AvlMaxCV"]=avl_maxcellvoltage;
	root_out["AvlMaxCVMod"]=temp_maxCVMod;
	root_out["BalActiv"]=mod_bal_cnt;
	root_out["Voltage"]=voltage;
	balance_target=avl_mincellvoltage;

	if (mod_bal_cnt > 0){
		digitalWrite(YellowLED,HIGH);
	}
	else{
		digitalWrite(YellowLED,LOW);
	}

	if ( (avl_maxcellvoltage-avl_mincellvoltage) < 5 ){
		digitalWrite(GreenLED,HIGH);
	}
	else{
		digitalWrite(GreenLED,LOW);
	}
}

void serialEvent() {
	uint8_t id =0;
	String inputString = "";         // a string to hold incoming data
	inputString.reserve(200);
	while (Serial.available()) {
		inputString += (char)Serial.read();
		}
	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(inputString);
	if ( root["48VID"] == myid || root["48VID"] == 255 ){
		uint8_t cmd = root["CMD"];
		//digitalWrite(6,LOW);
		//digitalWrite(A0,LOW);
		switch(cmd){

			case 0: //not allowed
				break;
			case 1: //set new Voltage Borders {"48VID":255,"CMD":1, "maxCV":3500, "minCV":2500}
				Serial.println("Set new Voltage borders");
				maxcellvoltage = root["maxCV"];
				mincellvoltage = root["minCV"];
				Serial.print("new maxCV: "); Serial.println(maxcellvoltage);
				Serial.print("new minCV: "); Serial.println(mincellvoltage);
				eeprom_write_word(&eeMAXCELLVOLTAGE, maxcellvoltage);
				eeprom_write_word(&eeMINCELLVOLTAGE, mincellvoltage);
				Serial.println("OK");
				break;

			case 2: //send the default Values  {"48VID":255,"CMD":2}
				root_out.printTo(Serial);
				Serial.println("");
				break;

			case 3: //assign PortPins  {"48VID":255,"CMD":3,"ID":5,"PortPin": 16}
				id = root["ID"];
				Serial.println("Assigning Modules to a PortPin");

				PortPin[id]=root["PortPin"];
				Serial.print(id);Serial.print(" -> "); Serial.println(PortPin[id]);
				eeprom_write_byte(&eePORTPIN[id], root["PortPin"] );
				Serial.println("OK");
				break;

			case 4: //open all Ports
				for (int i=14; i<20;i++){

					digitalWrite(i, HIGH);
					delay(50);
				}
				Serial.println("OK");

				break;
			case 5: //close all Ports
				for (int i=14; i<20;i++){
					digitalWrite(i, LOW);
				}
				break;
			case 6: //STOPP CAN
				Battery.stop();
				break;
			case 7: //set new ID
				Serial.print("Setting new ID: ");
				myid = root["newID"];
				Serial.println(myid);
				eeprom_write_byte(&eeMYID, myid);
				Serial.println("OK");
				break;

		}
		delay(100);
		digitalWrite(6, HIGH);
	}
}
