/*
 *	Server.h - Structures, constant variables, and global variables needed to run the
 *	server using windows sockets.
 *	Authors: 	Fernando Do Nascimento
 *			 	Isaac Ramirez
 *	Class: 	 	CSE 4342 - Embedded Systems II
 *	Lab:	 	Design of a Real-Data Acquisition System with Network Communications
 *	Due Date:	May 5, 2016	
 */
 
#ifndef SERVER_H
#define SERVER_H

//Constant Variables
#define ERR_CODE_NONE	0	//No Error
#define ERR_CODE_SWI	1	//Software Error

#define CMD_LENGTH		5

#define ARG_NONE		1
#define ARG_NUMBER		2

//Server Structures
typedef struct sp_comm {
	WSADATA wsaData;
	SOCKET cmdrecvsock;
	SOCKET cmdstatusock;
	SOCKET datasock;
	struct sockaddr_in server;
} *sp_comm_t;

typedef struct sp_flags {
	unsigned int start_system : 1;
	unsigned int pause_system : 1;
	unsigned int shutdown_system : 1;
	unsigned int analysis_started : 1;
	unsigned int restart : 1;
	unsigned int transmit_data : 1;
} *sp_flags_t;

typedef struct sp_struct {
	struct sp_comm		comm;
	struct sp_flags		flags;
} *sp_struct_t;

typedef struct {
	char cmd[CMD_LENGTH];
	int arg;
} cmd_struct_t;
WSADATA wsaData;

//Thread to Interface with the ProfileClient
HANDLE hClientThread;
DWORD dwClientThreadID;
VOID client_iface_thread(LPVOID parameters);

//Server Setup Global Variables
struct sp_struct profiler;
struct sockaddr_in saddr;
struct hostent *hp;

bool setUpDt9816 = false;
bool startSampling = false;
bool stopSampling = false;
#endif