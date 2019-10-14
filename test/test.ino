#include <Wire.h>
#include <SoftwareSerial.h>
#include "test.h"
#include "DataStructures.h"

rockBlockMessage Messages[MESSAGE_QUEUE_LENGTH];

SoftwareSerial Sat(RB_TX_PIN, RB_RX_PIN);

unsigned long lastInterrupt;
unsigned long lastSendAttempt = millis();
unsigned long lastModuleAction;

int MessageWaiting = 0;

void setup()
{
	Setup_Serial();
	Setup_Pins();
	Setup_Module();
	Setup_Interrupts();

	Serial.println("==RockBlock 9603 Wrapper Start up==");
}

void Setup_Interrupts()
{
	attachInterrupt(digitalPinToInterrupt(TEST_INT_PIN), ISR_Test, CHANGE);
}

void Setup_Module()
{
	SendCommandToModule("AT E0"); //turn serial echo off
}

void Setup_Pins()
{
	pinMode(RB_SLEEP_PIN, OUTPUT);
	pinMode(RB_SAT_PIN, INPUT);
	pinMode(TEST_INT_PIN, INPUT);
}

void Setup_Serial()
{
	Serial.begin(57600);
	Sat.begin(19200);
}

bool CheckNetwork()
{
	return digitalRead(RB_SAT_PIN) == HIGH;
}

bool CheckAsleep()
{
	return digitalRead(RB_SLEEP_PIN) == LOW;
}

bool TurnOffFlowControl()
{
	return SendCommandToModule("AT&K0");
}

bool PrepareToSend()
{
	if (CheckAsleep()) WakeUp();
	Serial.println("  Module is awake");

	if (!TurnOffFlowControl())
	{
		return false;
	}

	if (!CheckNetwork())
	{
		Serial.println("  Network not found");
		return false;
	}
	else
	{
		Serial.println("  Network found");
	}

	if (!CheckModuleComm())
	{
		Serial.println("  Module can not communicate");
		return false;
	}
	Serial.println("  Module can Communicate");
	return true;
}

uint16_t CalcChecksum(rockBlockMessage* pMessage, int byteCount)
{
	uint16_t checkSum = 0;
	for (int i = 0; i < byteCount; i++)
	{
		checkSum += pMessage->Message[i];
	}
	return checkSum;
}

void Wait(int Seconds)
{
	unsigned long Start = millis();
	while ((Start + (Seconds * 1000) > millis()))
	{
		Serial.print(".");
		delay(1000);
	}
}

int SendBinaryMessage(int MsgID)
{
	char resp[200] = "";
	char msg[20];

	Serial.print("Attempting to send binary message ID: ");
	Serial.println(MsgID, DEC);

	if (!PrepareToSend())
	{
		return MESSAGE_STATUS_QUEUED;
	}

	rockBlockMessage* pMessage = &Messages[MsgID];
	int messageLength = strlen(pMessage->Message);

	int msgSize = messageLength * sizeof(char);

	sprintf(msg, "AT+SBDWB=%d", msgSize);

	Sat.println(msg);

	Wait(5);

	int i = 0;
	while (Sat.available() > 0 && i < 200)
	{
		resp[i] = Sat.read();
		i++;
	}

	if (strstr(resp, "READY") > 0)
	{
		uint16_t checkSum = CalcChecksum(pMessage, messageLength);
		Sat.write(Messages[MsgID].Message);
		Sat.write(checkSum >> 8);
		Sat.write(checkSum & 0xFF);

		Wait(2);

		int i = 0;
		while (Sat.available() > 0 && i < 200)
		{
			resp[i] = Sat.read();
			i++;
		}

		if (strstr(resp, "0") > 0 && strstr(resp, "OK"))
		{
			if (StartSatComm())
			{
				Serial.println("  Message sent");
				return MESSAGE_STATUS_SENT;
			}
		}
	}
	return MESSAGE_STATUS_QUEUED;
}

int SendTextMessage(int MsgID)
{
	Serial.print("Attempting to send text message ID: ");
	Serial.println(MsgID, DEC);

	if (!PrepareToSend())
	{
		return MESSAGE_STATUS_QUEUED;
	}

	char msg[19 + MESSAGE_LENGTH];

	sprintf(msg, "AT+SBDWT=\"%d:%d - %s\"", Messages[MsgID].QueueTime[0], Messages[MsgID].QueueTime[1], Messages[MsgID].Message);
	Serial.println(msg);

	if (SendCommandToModule(msg))
	{
		if (StartSatComm())
		{
			Serial.println("  Message sent");
			return MESSAGE_STATUS_SENT;
		}
	}
}

int ParseReturnCode(char* Presponse)
{
	char* start = strstr(Presponse, "+SBDIX: ");
	start += 8;

	return atoi(start);
}

bool StartSatComm()
{
	Serial.println("  Starting exchange.");
	char resp[200] = "";
	Sat.println("AT+SBDIX");

	Wait(30);

	int i = 0;
	while (Sat.available() > 0 && i < 200)
	{
		resp[i] = Sat.read();
		i++;
	}
	Serial.println(resp);
	int returnCode = ParseReturnCode(&resp[0]);

	if (returnCode >= 0 && returnCode <= 5)
	{
		Serial.println("true");
		return true;
	}
	Serial.println("false");
	return false;
}

bool SendCommandToModule(char cmd[20])
{
	char resp[100];
	Sat.println(cmd);
	Wait(5);

	int i = 0;
	while (Sat.available() > 0 && i < 99)
	{
		resp[i] = Sat.read();
		i++;
	}

	if (strstr(resp, "OK") != NULL)
	{
		return true;
	}

	Serial.println(resp);
	return false;
}

bool CheckModuleComm()
{
	return SendCommandToModule("AT");
}

void WakeUp()
{
	digitalWrite(RB_SLEEP_PIN, HIGH);
	delay(RB_WAKEUP_CHARGE_TIMEOUT);
}

void Sleep()
{
	digitalWrite(RB_SLEEP_PIN, LOW);
}

bool AddMsgToQueue(rockBlockMessage* Pmsg)
{
	int MessageSlot = DetermineNextSlot();
	if (MessageSlot != ERROR_QUEUE_FULL)
	{
		Messages[MessageSlot] = *Pmsg;
		return true;
	}
	return false;
}

int DetermineNextSlot()
{
	for (int i = 0; i < MESSAGE_QUEUE_LENGTH; i++)
	{
		if (Messages[i].Status == MESSAGE_STATUS_NONE)
		{
			return i;
		}
	}

	return ERROR_QUEUE_FULL;
}

void RemoveMsgFromQueue(int slot)
{
	Messages[slot].Status = MESSAGE_STATUS_NONE;
	Messages[slot].QueueTime[0] = 0;
	Messages[slot].MessageType = 0;
	Messages[slot].Message[0] = 0;
}

void ISR_Test()
{
	if (lastInterrupt + 200 > millis())
	{
		return;
	}

	rockBlockMessage TestMsg;

	TestMsg.Status = MESSAGE_STATUS_QUEUED;
	TestMsg.QueueTime[0] = 12;
	TestMsg.QueueTime[1] = 13;
	TestMsg.MessageType = MESSAGE_TYPE_BINARY;

	sprintf(TestMsg.Message, "Hello");

	Serial.println("Adding Message");
	AddMsgToQueue(&TestMsg);
	lastInterrupt = millis();
}

void loop()
{
	if ((lastSendAttempt + MIN_TIME_BETWEEN_TRANSMIT) < millis())
	{
		Serial.println("Exchange attempt");
		Serial.println("Check Outgoing");
		for (int i = 0; i < MESSAGE_QUEUE_LENGTH; i++)
		{
			if (Messages[i].Status == MESSAGE_STATUS_SENT)
			{
				RemoveMsgFromQueue(i);
			}

			if (Messages[i].Status == MESSAGE_STATUS_QUEUED)
			{
				if (Messages[i].MessageType == MESSAGE_TYPE_TEXT)
				{
					Messages[i].Status = SendTextMessage(i);
				}

				if (Messages[i].MessageType == MESSAGE_TYPE_BINARY)
				{
					Messages[i].Status = SendBinaryMessage(i);
				}
				lastModuleAction = millis();
			}
		}

		lastSendAttempt = millis();
	}
	
	if ((lastModuleAction + 300000) < millis())
	{
		if (!CheckAsleep())
		{
			Serial.println("Putting module to sleep.");
			Sleep();
		}
	}
	delay(1000);
}