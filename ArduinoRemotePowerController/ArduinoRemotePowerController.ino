/*
 Name:		ArduinoRemotePowerController.ino
 Created:	2016/8/14 16:18:12
 Author:	Qian
*/

#include <SPI.h>         
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ICMPPing.h>
#include <TrueRandom.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;

#pragma region Global Constant Initialize
byte pingAddr[] = { 192,168,0,99 };//Remote PC IP Address
IPAddress ip(192, 168, 0, 201); //Arduino IP Address
unsigned int udpPort = 8888; //Arduino UDP Communication Port
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //Mac Address for the Ethernet Shield

const Uint32 MESSAGE_SIZE = 10;
const Uint8 REPEAT = 0;
const Uint8 EXCHANGE_KEY = 1;
const Uint8 MESSAGE = 2;
const Uint8 WAIT = 3;
const Uint32 HOLD_TIME_SHORT = 500;
const Uint32 HOLD_TIME_LONG = 4000;
const Uint32 WAIT_INTERVAL = 1000;

//Command
const Uint64 CHECK_STATUS = 0x9053870b774f8abcuLL;
const Uint64 PRESS_SHORT = 0x8cd38d4705b6e7d3uLL;
const Uint64 PRESS_LONG = 0x8478c2b6d2e8dc4cuLL;
const Uint64 STATUS_ON = 0xba0cd692655ba3d2uLL;
const Uint64 STATUS_OFF = 0x8df6c823ce1e3610uLL;
const Uint64 PRESS_RESTART = 0x3278c2bad2e8cd4cuL;


const Uint64 p = 0xca8532ff1b7e4881uLL;// The modulus
const Uint64 g = 0x89e94f1b04822985uLL;// The generator

const int Relay_in1 = 2;
const int Relay_in2 = 3;
const int Startup_ready = 4;
const int Command_ready = 5;
#pragma endregion


EthernetUDP Udp;

SOCKET pingSocket = 1;
char iobuffer[256];
byte udpBuffer[MESSAGE_SIZE];
byte& command = udpBuffer[0];
byte& sequence = udpBuffer[1];
unsigned long long& data = *reinterpret_cast<unsigned long long *>(udpBuffer + 2);
IPAddress remoteIP; //Received packet's originating IP
uint16_t remotePort; //Received packet's originating port


unsigned long long lastResponse;
byte lastSequence;
byte lastCommand;
unsigned long long key;
unsigned long long A, B, r;

#include "calc.h"

unsigned long long rand64()
{
	unsigned long long r;
	unsigned char * c = reinterpret_cast<unsigned char *>(&r);
	for (long i = 0; i < sizeof(unsigned long long); ++i) c[i] = TrueRandom.randomByte();
	return r;
}

void printHex(unsigned long long x)
{
	char hex[3];
	for (int i = 0; i < sizeof(unsigned long long); ++i)
	{
		sprintf(hex, "%.2x", ((uint8_t *)&x)[7 - i]);
		Serial.print(hex);
	}
	Serial.println("");
}

unsigned long long exchangeKey()
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

unsigned long long checkStatus()
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
	}
	Serial.println(iobuffer);
	udpBuffer[0] = MESSAGE;
	if (result) return STATUS_ON ^ key;
	else return STATUS_OFF ^ key;
}

unsigned long long pressButton(unsigned long holdTime)
{
	Serial.println("Press Button");
	//Button Actions
	digitalWrite(Relay_in1, LOW);
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
		}
	}
	udpBuffer[0] = MESSAGE;
	//Release Actions
	Serial.println("Button Released");
	digitalWrite(Relay_in1, HIGH);
	return data;
}

unsigned long long pressRestartButton(void)
{
	//Serial.println("Press Button");
	//Button Actions
	digitalWrite(Relay_in2, LOW);
	int start = millis();
	int k = 1;
	udpBuffer[0] = WAIT;
	while (true)
	{
		int t = millis() - start;
		if (t >= HOLD_TIME_SHORT) break;
		if (t > WAIT_INTERVAL*k)
		{
			k++;
			Serial.println("Sending wait.");
			Udp.beginPacket(remoteIP, remotePort);
			Udp.write(udpBuffer, MESSAGE_SIZE);
			Udp.endPacket();
		}
	}
	udpBuffer[0] = MESSAGE;
	//Release Actions
	//Serial.println("Button Released");
	digitalWrite(Relay_in2, HIGH);
	return data;
}

void setup()
{
	Serial.begin(9600);
	Serial.println("Reseting");
	key = rand64();
	Ethernet.begin(mac, ip);
	Udp.begin(udpPort);
	//Initialize Pins
	pinMode(Relay_in1, OUTPUT);
	pinMode(Relay_in2, OUTPUT);
	pinMode(Startup_ready, OUTPUT);
	pinMode(Command_ready, OUTPUT);
	digitalWrite(Relay_in1, HIGH);
	digitalWrite(Relay_in2, HIGH);
	digitalWrite(Startup_ready, LOW);
	digitalWrite(Command_ready, LOW);
}

void loop()
{
	//Serial.println("In loop");
	int packetSize = Udp.parsePacket(); //This includes the UDP header
	if (packetSize)
	{
		//Serial.println("Get UDP Message\n");
		digitalWrite(Command_ready, HIGH);
		packetSize = packetSize - 8;      //subtract the 8 header
		remoteIP = Udp.remoteIP();
		remotePort = Udp.remotePort();
		Udp.read(reinterpret_cast<char*>(udpBuffer), MESSAGE_SIZE);
		sprintf(iobuffer, "Received packet of size %d from %u.%u.%u.%u:%u", packetSize, remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3], remotePort);
		Serial.println(iobuffer);
		unsigned long long response = 0;
		if (command == EXCHANGE_KEY)
		{
			Serial.println("Exchange key.");
			response = exchangeKey();
		}
		else if (command == MESSAGE)
		{
			unsigned long long message = data ^ key;
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
			else if (message == PRESS_RESTART)
			{
				Serial.println("Press restart.");
				response = pressRestartButton();
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
		}
		digitalWrite(Command_ready, LOW);
	}
}
