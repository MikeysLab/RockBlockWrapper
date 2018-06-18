#include <Wire.h>
#include <SoftwareSerial.h>
#include "DataStructures.h"

#define I2C_SLAVE_ADDRESS         0x02

#define MESSAGE_QUEUE_LENGTH      5
#define MESSAGE_LENGTH            100

#define ERROR_NO_ERROR            -2
#define ERROR_QUEUE_FULL          -1
#define ERROR_NO_SAT              1
#define ERROR_NO_MODULE_COMM      2

#define MIN_TIME_BETWEEN_TRANSMIT 40000

#define MESSAGE_TYPE_TEXT         1
#define MESSAGE_TYPE_BINARY       2

#define MESSAGE_PRIORITY_NORMAL   0
#define MESSAGE_PRIORITY_HIGH     1
#define MESSAGE_PRIORITY_CRITICAL 2

#define MESSAGE_STATUS_NONE       0
#define MESSAGE_STATUS_SENT       1
#define MESSAGE_STATUS_QUEUED     2
#define MESSAGE_STATUS_ERROR      3

#define RB_SLEEP_PIN              5
#define RB_SAT_INT_PIN            3
#define RB_TX_PIN                 10
#define RB_RX_PIN                 11
#define TEST_INT_PIN              2

#define RB_WAKEUP_CHARGE_TIMEOUT  2000

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
  if(digitalRead(RB_SAT_INT_PIN) == HIGH) return true;
  return false;
}

bool CheckAsleep()
{
  if(digitalRead(RB_SLEEP_PIN) == LOW) return true;
  return false;
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
  
  String msg = "AT+SBDWT=\"";
  msg += Messages[MsgID].Message;
  msg += "\"";
  
  if(SendCommandToModule(msg))
  {
    if(StartSatComm())
    {
      Serial.println("  Message sent");
      return MESSAGE_STATUS_SENT;
    }
  }
}

int ParseReturnCode(String response)
{
  if(response.indexOf("+SBDIX: ") > 0)
  {
    response = response.substring(response.indexOf("+SBDIX: ") + 8, response.indexOf(",",response.indexOf("+SBDIX: ") + 8)); 
  }
  return response.toInt();
}

bool StartSatComm()
{
  Serial.println("  Starting exchange.");
  String resp = "";
  Sat.println("AT+SBDIX");
  
  unsigned long xmitStart = millis();
  while((xmitStart + 45000) > millis())
  {
    Serial.print(".");
    delay(1000);
  }
  
  while(Sat.available() > 0)
  {
    resp = Sat.readString();
  }

  int returnCode=ParseReturnCode(resp);
  
  if(returnCode >= 0 && returnCode <= 5)
  { 
    return true;
  }
  return false;
}

bool SendCommandToModule(String cmd)
{
  String resp = "";
  Sat.println(cmd);
  delay(5000);
  
  if(Sat.available() > 0)
  {
    resp = Sat.readString();
  }

  if(resp.indexOf("OK") > 0) 
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
    if (Messages[i].Status == MESSAGE_STATUS_NONE) return i;
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
  String MsgTxt = "Yay - I did not break something";
  
  TestMsg.Status = MESSAGE_STATUS_QUEUED;
  TestMsg.QueueTime[1] = 12;
  TestMsg.QueueTime[2] = 13;
  TestMsg.Priority = MESSAGE_PRIORITY_NORMAL;
  TestMsg.MessageType = MESSAGE_TYPE_TEXT;
  MsgTxt.toCharArray(TestMsg.Message, MESSAGE_LENGTH);
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
      if(Messages[i].Status == MESSAGE_STATUS_SENT) RemoveMsgFromQueue(i);
      if(Messages[i].Status == MESSAGE_STATUS_QUEUED)
      {
        if(Messages[i].MessageType == MESSAGE_TYPE_TEXT) Messages[i].Status = SendTextMessage(i);
        if(Messages[i].MessageType == MESSAGE_TYPE_BINARY) Messages[i].Status = SendBinaryMessage(i);  
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
