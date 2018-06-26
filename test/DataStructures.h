#pragma once

typedef struct
{
	byte QueueTime[2];
	byte MessageType;
	char Message[MESSAGE_LENGTH];
	byte Status;
} rockBlockMessage;
