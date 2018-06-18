#pragma once

typedef struct
{
	byte QueueTime[2];
	byte Priority;
	byte MessageType;
	char Message[MESSAGE_LENGTH];
	byte Status;
} rockBlockMessage;
