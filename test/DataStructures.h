typedef struct
{
	byte QueueTime[2];
	byte Priority;
	byte MessageType;
	char Message[100];
	byte Status;
} rockBlockMessage;
