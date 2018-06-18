#include <Wire.h>
#include <SoftwareSerial.h>
#include "test.h"
#include "DataStructures.h"

rockBlockMessage Messages[MESSAGE_QUEUE_LENGTH];

SoftwareSerial Sat(RB_TX_PIN,RB_RX_PIN);

byte lastError = ERROR_NO_ERROR;
unsigned long lastInterrupt;
unsigned long lastSendAttempt;
unsigned long lastModuleAction;

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
	pinMode(RB_SAT_INT_PIN, INPUT);
	pinMode(TEST_INT_PIN, INPUT);
}

void Setup_Serial()
{
	Serial.begin(57600);
	Sat.begin(19200);
}

void SortQueue()
{
	for (int i=MESSAGE_QUEUE_LENGTH; i>0; i--)
	{
		int swaps = 0;
		rockBlockMessage msgAux;
		for(int x=1; x<i; x++)
		{
			if (Messages[x-1].Priority < Messages[x].Priority)
			{
				msgAux = Messages[x-1];
				Messages[x-1] = Messages[x];
				Messages[x] = msgAux;
				swaps++;
			}
		}
		if (!swaps)
		{
			break;
		}
	}
}

bool CheckNetwork()
{
	return digitalRead(RB_SAT_INT_PIN) == HIGH;
}

bool CheckAsleep()
{
	return digitalRead(RB_SLEEP_PIN) == LOW;
}

bool PrepareToSend()
{
	if(CheckAsleep()) WakeUp();
	Serial.println("  Module is awake");
  
	if(!CheckNetwork())
	{
		Serial.println("  Network not found");
		return false;
	}
	else
	{
		Serial.println("  Network found");
	}
  
	if(!CheckModuleComm())
	{
		Serial.println("  Module can not communicate");
		lastError = ERROR_NO_MODULE_COMM;
		return false;
	}
	Serial.println("  Module can Communicate");
	return true;
}

int SendBinaryMessage(int MsgID)
{
	Serial.print("Attempting to send binary message ID: ");
	Serial.println(MsgID, DEC);
  
	if (!PrepareToSend()) return MESSAGE_STATUS_QUEUED;
}

int SendTextMessage(int MsgID)
{
  Serial.print("Attempting to send text message ID: ");
  Serial.println(MsgID, DEC);
  
  if (!PrepareToSend()) return MESSAGE_STATUS_QUEUED;
  char msg[11 + MESSAGE_LENGTH];

  sprintf(msg, "AT+SBDWT=\"%s\"", Messages[MsgID].Message);
  
  if(SendCommandToModule(msg))
  {
    if(StartSatComm())
    {
      Serial.println("  Message sent");
      return MESSAGE_STATUS_SENT;
    }
  }
}

int ParseReturnCode(char *Presponse)
{
	int retCode;
	if (sscanf(Presponse, "+SBDIX: %d,", &retCode))
	{
		return retCode;
	}
	return -1;
}

bool StartSatComm()
{
	Serial.println("  Starting exchange.");
	char resp[100] = "";
	Sat.println("AT+SBDIX");
  
	unsigned long xmitStart = millis();
	while((xmitStart + 45000) > millis())
	{
		Serial.print(".");
		delay(1000);
	}
  
	int i = 0;
	while (Sat.available() > 0 && i < 100)
	{
		resp[i] = Sat.read();
		i++;
	}

	int returnCode=ParseReturnCode(&resp[0]);
  
	if(returnCode >= 0 && returnCode <= 5)
	{ 
		return true;
	}
	return false;
}

bool SendCommandToModule(char cmd[20])
{
	char resp[100];
	Sat.println(cmd);
	delay(5000);
  
	int i = 0;
	while(Sat.available() > 0 && i < 99)
	{ 
		resp[i] = Sat.read();
		i++;
	}

	if(strstr(resp, "OK") != NULL) 
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
	digitalWrite(RB_SLEEP_PIN,HIGH);
	delay(RB_WAKEUP_CHARGE_TIMEOUT);
}

void Sleep()
{
	digitalWrite(RB_SLEEP_PIN, LOW);
}

bool AddMsgToQueue(rockBlockMessage *Pmsg)
{
	int MessageSlot = DetermineNextSlot();
	if(MessageSlot != ERROR_QUEUE_FULL)
	{
		Messages[MessageSlot] = *Pmsg;
		SortQueue();
		return true;
	}
	return false;
}

int DetermineNextSlot()
{
	for(int i=0; i < MESSAGE_QUEUE_LENGTH; i++)
	{
		if (Messages[i].Status == MESSAGE_STATUS_NONE)
		{
			return i;
		}
	}
	lastError = ERROR_QUEUE_FULL;
	return ERROR_QUEUE_FULL;
}

void RemoveMsgFromQueue(int slot)
{
	Messages[slot].Status = MESSAGE_STATUS_NONE;
	Messages[slot].QueueTime[0] = 0;
	Messages[slot].Priority = 0;
	Messages[slot].MessageType = 0;
	Messages[slot].Message[0] = 0;
	SortQueue();
}

void ISR_Test()
{
	if(lastInterrupt + 200 > millis()) return;
	rockBlockMessage TestMsg;
  
	TestMsg.Status = MESSAGE_STATUS_QUEUED;
	TestMsg.QueueTime[1] = 12;
	TestMsg.QueueTime[2] = 13;
	TestMsg.Priority = MESSAGE_PRIORITY_NORMAL;
	TestMsg.MessageType = MESSAGE_TYPE_TEXT;
  
	char *pMsg = TestMsg.Message;
	pMsg = "Yay - I did not break something";
  
	Serial.println("Adding Message");
	AddMsgToQueue(&TestMsg);
	lastInterrupt = millis();
}

void loop()
{
	if ((lastSendAttempt + MIN_TIME_BETWEEN_TRANSMIT) < millis())
	{
		for(int i = 0; i < MESSAGE_QUEUE_LENGTH; i++)
		{
			if (Messages[i].Status == MESSAGE_STATUS_SENT)
			{
				RemoveMsgFromQueue(i);
			}

			if(Messages[i].Status == MESSAGE_STATUS_QUEUED)
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
  
	//if we have not used the module for 5 minutes put it to sleep
	if ((lastModuleAction + 300000) < millis())
	{
		if(!CheckAsleep())
		{
			Serial.println("Putting module to sleep.");
			Sleep();
		} 
	}
}
