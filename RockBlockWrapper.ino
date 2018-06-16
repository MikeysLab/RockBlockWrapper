#define I2C_SLAVE_ADDRESS         0x02

#define MESSAGE_QUEUE_LENGTH      1
#define MESSAGE_LENGTH            200

#define ERROR_NO_ERROR            -1
#define ERROR_QUEUE_FULL          0
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

//#include <Wire.h>
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

bool hasNetwork = false;
bool isAsleep = true;
byte lastError = ERROR_NO_ERROR;
unsigned long lastSendAttempt = 0;
unsigned long lastInterrupt = millis();

void setup()
{
  Setup_Serial(); 
  Setup_Pins();
  Setup_Interrupts();
  Sleep();
  //Setup_I2C();
  Setup_Module();
   

  Serial.print("Size of message: ");
  Serial.println(sizeof(rockBlockMessage));
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

void Setup_Interrupts()
{
  attachInterrupt(digitalPinToInterrupt(RB_SAT_INT_PIN), ISR_SatViewChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(TEST_INT_PIN), ISR_Test, CHANGE);
}

void Setup_I2C()
{
//  Wire.begin(I2C_SLAVE_ADDRESS);
//  Wire.onRequest(I2CRequestEvent);
//  Wire.onReceive(I2CReceiveEvent);
}

void ISR_SatViewChange()
{
  if(CheckNetwork())
  {
    hasNetwork = true;
    Serial.println("Acquired Network!");
    return;
  }
  hasNetwork = false;
  Serial.println("Lost Network");
}

bool CheckNetwork()
{
  if(digitalRead(RB_SAT_INT_PIN) == HIGH) return true;
  return false;
}

bool CheckAsleep()
{
  if(digitalRead(RB_SLEEP_PIN) == HIGH) return true;
  return false;
}

void I2CRequestEvent()
{

}

void I2CReceiveEvent(int bytesReceived)
{

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

String messageToString(int MsgID)
{
  String retVal = "";
  for (int i=0; i < (sizeof(Messages[MsgID].Message) / sizeof(char)); i++)
  {
    retVal += Messages[MsgID].Message[i];
  }
  return retVal;
}

int SendMessage(int MsgID)
{
  Serial.print("Attempting to send message ID: ");
  Serial.println(MsgID, DEC);
  if(isAsleep) WakeUp();
  Serial.println("  Module is awake");
  if(!hasNetwork) return MESSAGE_STATUS_QUEUED; //keep message in queue
  if(!CheckModuleComm())
  {
    lastError = ERROR_NO_MODULE_COMM;
    return MESSAGE_STATUS_QUEUED;
  }
  String msg = "AT+SBDWT=\'Test Message - from queue - and space!\'";
  if(SendCommandToModule(msg))
  {
    Serial.print("  Data sent to module:");
    Serial.println(msg);
  }
  
  if(StartSatComm())
  {
    Serial.println("  Message sent");
    return MESSAGE_STATUS_SENT;
  }
}

bool StartSatComm()
{
  Serial.println("StartSatcom()");
  String resp = "";
  Sat.println("AT+SBDIX");
  delay(30000);
  Serial.println("Awaiting response");
  if(Sat.available() > 0)
  {
    resp = Sat.readString();
  }
  Serial.print("got response: ");
  Serial.println(resp);
  //if(resp.indexOf("+SBDIX: 3,") > 0 || resp.indexOf("+SBDIX: 2,") > 0 || resp.indexOf("+SBDIX: 0,") > 0 || resp.indexOf("+SBDIX: 18,") > 0)
  //{ 
  //  Serial.println("    Send successful");
  //  return true;
  //}
  Serial.println("    Send Failed");
  return false;
}

bool SendCommandToModule(String cmd)
{
  String resp = "";
  Sat.println(cmd);
  delay(500);
  
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
  Serial.println("  Checking module communication...");
  if (SendCommandToModule("AT"))
  {
    Serial.println("    Communication is successful");
    return true;
  }
  Serial.println("    Communication Failed, unexpected response");
  return false;
}

void Sleep()
{
  Serial.println("Putting Module to sleep");
  digitalWrite(RB_SLEEP_PIN,LOW);
  isAsleep = true;
}

void WakeUp()
{
  Serial.println("Waking module up"); 
  digitalWrite(RB_SLEEP_PIN,HIGH);
  delay(RB_WAKEUP_CHARGE_TIMEOUT);
  isAsleep = false;
}

bool AddMsgToQueue(rockBlockMessage msg)
{
  Serial.print("Adding message to queue: ");
  Serial.println(msg.Message);
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

void ISR_Test()
{
  if(lastInterrupt + 200 > millis()) return;
  
  rockBlockMessage TestMsg;
  String MsgTxt = "Test Message - From Space!";
  
  TestMsg.Status = MESSAGE_STATUS_NONE;
  TestMsg.queueTime[1] = 12;
  TestMsg.queueTime[2] = 13;
  TestMsg.Priority = MESSAGE_PRIORITY_NORMAL;
  TestMsg.messageType = MESSAGE_TYPE_TEXT;
  MsgTxt.toCharArray(TestMsg.Message, MESSAGE_LENGTH);

  AddMsgToQueue(TestMsg);
  lastInterrupt = millis();
}

void loop()
{
  for(int i = 0; i < MESSAGE_QUEUE_LENGTH; i++)
  {
    if(((Messages[i].Status != MESSAGE_STATUS_SENT) || (Messages[i].Status != MESSAGE_STATUS_NONE))  && ((lastSendAttempt + MIN_TIME_BETWEEN_TRANSMIT) < millis()))
    {
      Messages[i].Status = SendMessage(i);
      if(Messages[i].Status == MESSAGE_STATUS_SENT) RemoveMsgFromQueue(i);
      lastSendAttempt = millis();
    }
  }
}
