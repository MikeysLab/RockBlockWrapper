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

#include <SoftwareSerial.h>

typedef struct
{
  byte queueTime[2];
  byte Priority;
  byte messageType;
  char Message[MESSAGE_LENGTH];
  byte Status;
} rockBlockMessage;

rockBlockMessage Messages[MESSAGE_QUEUE_LENGTH];

SoftwareSerial Sat(RB_TX_PIN,RB_RX_PIN);

byte lastError = ERROR_NO_ERROR;
unsigned long lastSendAttempt = 0;

void setup()
{
  Setup_Serial(); 
  Setup_Pins();
  Setup_Module();

  Serial.println("==RockBlock 9603 Wrapper Start up==");

  Test();
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
  for (int x=5; x>0; x--)
  {
    int swaps = 0;
    rockBlockMessage msgAux;
    for(int i=1; i<x; i++)
    {
      if (Messages[i-1].Priority < Messages[i].Priority)
      {
        msgAux = Messages[i-1];
        Messages[i-1] = Messages[i];
        Messages[i] = msgAux;
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

int SendMessage(int MsgID)
{
  Serial.print("Attempting to send message ID: ");
  Serial.println(MsgID, DEC);
  if(CheckAsleep()) WakeUp();
  Serial.println("  Module is awake");
  //if(!CheckNetwork()) return MESSAGE_STATUS_QUEUED; //keep message in queue
  if(!CheckModuleComm())
  {
    Serial.println("  Module can not communicate");
    lastError = ERROR_NO_MODULE_COMM;
    return MESSAGE_STATUS_QUEUED;
  }
  Serial.println("  Module can Communicate");
  String msg = "AT+SBDWT=\'Test Message - from queue - and space!\'";
  if(SendCommandToModule(msg))
  {
    if(StartSatComm())
    {
      Serial.println("  Message sent");
      return MESSAGE_STATUS_SENT;
    }
  }
}

bool StartSatComm()
{
  String resp = "";
  Sat.println("AT+SBDIX");
  delay(30000);
  while(Sat.available() > 0)
  {
    resp = Sat.readString();
  }
  Serial.print("got response: ");
  Serial.println(resp);
  if(resp.indexOf("+SBDIX: 3,") > 0 || resp.indexOf("+SBDIX: 2,") > 0 || resp.indexOf("+SBDIX: 0,") > 0 || resp.indexOf("+SBDIX: 18,") > 0)
  { 
    Serial.println("    Send successful");
    return true;
  }
  Serial.println("    Send Failed");
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


bool AddMsgToQueue(rockBlockMessage msg)
{
  int MessageSlot = DetermineNextSlot();
  if(MessageSlot != ERROR_QUEUE_FULL)
  {
    Messages[MessageSlot] = msg;
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
  Messages[slot].queueTime[0] = 0;
  Messages[slot].Priority = 0;
  Messages[slot].messageType = 0;
  Messages[slot].Message[0] = 0;
  SortQueue();
}

void Test()
{
  rockBlockMessage TestMsg;
  String MsgTxt = "Test Message - From Space!";
  
  TestMsg.Status = MESSAGE_STATUS_QUEUED;
  TestMsg.queueTime[1] = 12;
  TestMsg.queueTime[2] = 13;
  TestMsg.Priority = MESSAGE_PRIORITY_NORMAL;
  TestMsg.messageType = MESSAGE_TYPE_TEXT;
  MsgTxt.toCharArray(TestMsg.Message, MESSAGE_LENGTH);
  Serial.println("Adding Message");
  AddMsgToQueue(TestMsg);
}

void loop()
{
  delay(5000);
  Serial.println("in main loop");
  for(int i = 0; i < MESSAGE_QUEUE_LENGTH; i++)
  {
    if(Messages[i].Status == MESSAGE_STATUS_SENT) RemoveMsgFromQueue(i);
    if((Messages[i].Status == MESSAGE_STATUS_QUEUED) && ((lastSendAttempt + MIN_TIME_BETWEEN_TRANSMIT) < millis()))
    {
      Messages[i].Status = SendMessage(i);
      lastSendAttempt = millis();
    }
  }
}
