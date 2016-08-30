/*
 Name:		ArduinoRemotePowerController.ino
 Created:	2016/8/14 16:18:12
 Author:	Qian
*/
#include <SPI.h>         
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ICMPPing.h>
//#include <Udp.h>
#include <TrueRandom.h>
#include "Common.h"

Uint8 pingAddr[] = { 192,168,0,80 };//Remote PC IP Address
IPAddress ip(192, 168, 0, 201); //Arduino IP Address
Uint16 udpPort = 8888; //Arduino UDP Communication Port
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //Mac Address for the Ethernet Shield
EthernetUDP Udp;

SOCKET pingSocket = 1;
char iobuffer[256];
Uint8 udpBuffer[MESSAGE_SIZE];
Uint8& command = udpBuffer[0];
Uint8& sequence = udpBuffer[1];
Uint64& data = *reinterpret_cast<Uint64 *>(udpBuffer + 2);
//uint8_t remoteIP[4]; // holds received packet's originating IP
IPAddress remoteIP;
uint16_t remotePort; // holds received packet's originating port


Uint64 lastResponse;
Uint8 lastSequence;
Uint8 lastCommand;
Uint64 key;
Uint64 A, B, r;
//Uint8 outputPins[] = { 2, 3, 5, 6, 7, 8 };
//Uint8 outputVal = 0x2b;

#include "modular.h"

Uint64 rand64()
{
	Uint64 r;
	unsigned char * c = reinterpret_cast<unsigned char *>(&r);
	for (size_t i = 0; i < sizeof(Uint64); ++i) c[i] = TrueRandom.randomByte();
	return r;
}

void printHex(Uint64 x)
{
	char hex[3];
	for (int i = 0; i < sizeof(Uint64); ++i)
	{
		sprintf(hex, "%.2x", ((uint8_t *)&x)[7 - i]);
		Serial.print(hex);
	}
	Serial.println("");
}

Uint64 exchangeKey()
{
	int start = millis();
	int k = 0;
	udpBuffer[0] = WAIT;
	r = rand64();
	A = data;
	B = powm(g, r, p) % p;
	key = powm(A, r, p) % p;
	Serial.println("Key: ");
	printHex(key);
	udpBuffer[0] = EXCHANGE_KEY;
	return B;
}

Uint64 checkStatus()
{
	Serial.println("Pinging desktop");
	ICMPPing ping(pingSocket);
	bool result = false;
	udpBuffer[0] = WAIT;
	for (int i = 0; i < 4 && !result; ++i)
	{
		result = ping(1, pingAddr, iobuffer);
		Serial.println("Sending wait.");
		Udp.beginPacket(remoteIP, remotePort);
		Udp.write(udpBuffer, MESSAGE_SIZE);
		Udp.endPacket();
		//Udp.sendPacket((Uint8 *)udpBuffer, MESSAGE_SIZE, remoteIP, remotePort);
	}
	Serial.println(iobuffer);
	udpBuffer[0] = MESSAGE;
	if (result) return STATUS_ON ^ key;
	else return STATUS_OFF ^ key;
}

Uint64 pressButton(Uint32 holdTime)
{
	//TODO: Actions

	//for (int i = 0; i<sizeof(outputPins); ++i) digitalWrite(outputPins[i], outputVal & (1u << i) ? HIGH : LOW);
	int start = millis();
	int k = 1;
	udpBuffer[0] = WAIT;
	while (true)
	{
		int t = millis() - start;
		if (t >= holdTime) break;
		if (t > WAIT_INTERVAL*k)
		{
			k++;
			Serial.println("Sending wait.");
			Udp.beginPacket(remoteIP, remotePort);
			Udp.write(udpBuffer, MESSAGE_SIZE);
			Udp.endPacket();
			//Udp.sendPacket((Uint8 *)udpBuffer, MESSAGE_SIZE, remoteIP, remotePort);
		}
	}
	udpBuffer[0] = MESSAGE;
	//delay(holdTime);
	//TODO: Release Actions

	//for (int i = 0; i<sizeof(outputPins); ++i) digitalWrite(outputPins[i], LOW);
	return data;
}
// the setup function runs once when you press reset or power the board
void setup()
{
	Serial.begin(9600);
	Serial.println("Reseting");
	key = rand64();
	Ethernet.begin(mac, ip);
	Udp.begin(udpPort);
	//for (int i = 0; i<sizeof(outputPins); ++i)
	//{
	//	pinMode(outputPins[i], OUTPUT);
	//	digitalWrite(outputPins[i], LOW);
	//}
	//TODO: Initialize Pins

}

// the loop function runs over and over again until power down or reset
void loop()
{
	//Serial.println("In loop");
	int packetSize = Udp.parsePacket(); // note that this includes the UDP header
	if (packetSize)
	{
		//Serial.println("Get UDP Message\n");
		packetSize = packetSize - 8;      // subtract the 8 byte header
		//Udp.readPacket(reinterpret_cast<uint8_t *>(udpBuffer), MESSAGE_SIZE, remoteIP, &remotePort);
		remoteIP = Udp.remoteIP();
		remotePort = Udp.remotePort();
		Udp.read(reinterpret_cast<char*>(udpBuffer), MESSAGE_SIZE);
		sprintf(iobuffer, "Received packet of size %d from %u.%u.%u.%u:%u", packetSize, remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3], remotePort);
		Serial.println(iobuffer);
		Uint64 response = 0;
		if (command == EXCHANGE_KEY)
		{
			Serial.println("Exchange key.");
			response = exchangeKey();
		}
		else if (command == MESSAGE)
		{
			Uint64 message = data ^ key;
			if (message == CHECK_STATUS)
			{
				Serial.println("Check status.");
				response = checkStatus();
			}
			else if (message == PRESS_SHORT)
			{
				Serial.println("Press button (short).");
				response = pressButton(HOLD_TIME_SHORT);
			}
			else if (message == PRESS_LONG)
			{
				Serial.println("Press button (long).");
				response = pressButton(HOLD_TIME_LONG);
			}
			key = rand64();
		}
		else if (command == REPEAT)
		{
			Serial.println("Resending packet.");
			response = lastResponse;
			command = lastCommand;
			sequence = lastSequence;
		}
		if (response)
		{
			lastCommand = command;
			lastSequence = sequence;
			lastResponse = response;
			data = response;
			Udp.beginPacket(remoteIP, remotePort);
			Udp.write(udpBuffer, MESSAGE_SIZE);
			Udp.endPacket();
			//Udp.sendPacket((Uint8 *)udpBuffer, MESSAGE_SIZE, remoteIP, remotePort);
		}
	}
}
