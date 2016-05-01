/*
 *	Authors: Fernando Do Nascimento
 *			 Isaac Ramirez
 *	CSE 4342 - Embedded Systems II
 *	Lab Project - Client Program
 */

//Client Libraries
#include <cmath>
#include <conio.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <WinSock.h>

using namespace std;

/* Client Preprocessors */
#define ERR_CODE_NONE	0	//No Error
#define ERR_CODE_SWI	1	//Software Error

#define CMD_LENGTH		5

#define ARG_NONE		1
#define ARG_NUMBER		2

#define COEF_MAX		101
/* End Client Preprocessors */

/* Client Structures */
typedef struct sp_comm 
{
	WSADATA wsaData;
	SOCKET cmdrecvsock;
	SOCKET cmdstatusock;
	SOCKET datasock;
	struct sockaddr_in server;
} *sp_comm_t;

typedef struct sp_flags 
{
	unsigned int start_system : 1;
	unsigned int pause_system : 1;
	unsigned int shutdown_system : 1;
	unsigned int analysis_started : 1;
	unsigned int restart : 1;
	unsigned int transmit_data : 1;
} *sp_flags_t;

typedef struct sp_struct 
{
	struct sp_comm		comm;
	struct sp_flags		flags;
} *sp_struct_t;

typedef struct 
{
	char cmd[CMD_LENGTH];
	int arg;
} cmd_struct_t;
WSADATA wsaData;
/* End Client Structures */

/* Client Global Variables */
bool done = true;
int clientCounter = 0;
double filterCoefficient[COEF_MAX];
/* End Client Global Variables */

/* Thread to interface with the ProfileClient */
HANDLE hClientThread;
DWORD dwClientThreadID;
VOID client_iface_thread(LPVOID parameters);
/* End Thread to Interface with the ProfileClient */

/* Function Prototypes */
void load_coefficient(char *filterFile);
double convert_string(char *buffer);
/* End Function Prototypes */

int main(int argc, char *argv[])
{
	/* Declared Variables */
	struct sp_struct profiler;
	struct sockaddr_in saddr;
	struct hostent *hp;

	double sampleRate;
	char buffer[COEF_MAX] = "";
	char inputChar[100] = "";
	char saveFile[250];
	char filterFile[250];
	char start;
	int res = 0;
	
	/* End Declared Variables */

	memset(&profiler, 0, sizeof(profiler));
	sp_comm_t comm = &profiler.comm;

	if ((res = WSAStartup(0x202, &wsaData)) != 0) 
	{
		fprintf(stderr, "WSAStartup failed with error %d\n", res);
		WSACleanup();
	
		return ERR_CODE_SWI;
	}

	/**********************************************************************************
	* Setup data transmition socket to broadcast data
	**********************************************************************************/
	hp = (struct hostent*)malloc(sizeof(struct hostent));
	hp->h_name = (char*)malloc(sizeof(char) * 17);
	hp->h_addr_list = (char**)malloc(sizeof(char*) * 2);
	hp->h_addr_list[0] = (char*)malloc(sizeof(char) * 5);
	
	strcpy(hp->h_name, "lab_example\0");

	hp->h_addrtype = 2;
	hp->h_length = 4;

	//Broadcast in 255.255.255.255 Network 	
	hp->h_addr_list[0][0] = (signed char)255;	//192;129
	hp->h_addr_list[0][1] = (signed char)255;	//168;107
	hp->h_addr_list[0][2] = (signed char)255;	//0; 255
	hp->h_addr_list[0][3] = (signed char)255;	//140;255
	hp->h_addr_list[0][4] = 0;

	/**********************************************************************************
	* Setup a socket for broadcasting data
	**********************************************************************************/
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = hp->h_addrtype;

	memcpy(&(saddr.sin_addr), hp->h_addr, hp->h_length);
	saddr.sin_port = htons(1024);

	if ((comm->datasock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) 
	{
		fprintf(stderr, "socket(datasock) failed: %d\n", WSAGetLastError());
		WSACleanup();
	
		return ERR_CODE_NONE;
	}

	if (connect(comm->datasock, (struct sockaddr*)&saddr, sizeof(saddr)) == SOCKET_ERROR) 
	{
		fprintf(stderr, "connect(datasock) failed: %d\n", WSAGetLastError());
		WSACleanup();
	
		return ERR_CODE_SWI;
	}

	/**********************************************************************************
	* Setup and bind a socket to listen for commands from client
	**********************************************************************************/
	memset(&saddr, 0, sizeof(struct sockaddr_in));

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(1500);
	
	if ((comm->cmdrecvsock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) 
	{
		fprintf(stderr, "socket(cmdrecvsock) failed: %d\n", WSAGetLastError());
		WSACleanup();
	
		return ERR_CODE_NONE;
	}

	if (bind(comm->cmdrecvsock, (struct sockaddr*)&saddr, sizeof(saddr)) == SOCKET_ERROR) 
	{
		fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
		WSACleanup();
	
		return ERR_CODE_NONE;
	}

	hClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_iface_thread, (LPVOID)&profiler, 0, &dwClientThreadID);
	SetThreadPriority(hClientThread, THREAD_PRIORITY_LOWEST);

	//Prompt the user for input
	cout << "Client Program" << endl;
	cout << "--------------" << endl;
	
	printf("Enter the Filter Coefficients file name: ");
	scanf("%s", filterFile);

	printf("Enter the Sample Rate: ");+
	scanf("%lf", &sampleRate);

	printf("Enter the name of the save file: ");
	scanf("%s", saveFile);

	//Load b.txt to the filter coefficient array
	load_coefficient(filterFile);

	//Sends Data to the Server
	while (done == true)
	{
		if (_kbhit())
		{
			//Sends the Sample Rate and the Filter Coefficient to the Server
			if (clientCounter == 0)
			{
				//Sample Rate
				memcpy(inputChar, (char *)&sampleRate, 100);
				send(comm->datasock, inputChar, sizeof(inputChar), 0);

				//Filter Coefficient
				for (int index = 0; index < COEF_MAX; index++)
				{
					double val = filterCoefficient[index];
					memcpy(buffer, (char *)&val, 101);
					send(comm->datasock, buffer, sizeof(buffer), 0);
				}

				clientCounter = 1;
			}

			//Asks the user if he/she wishes to start the A/D Conversion
			else if(clientCounter == 1)
			{
				do
				{
					cout << "Do you wish to START? (Y\\N) ";
					cin.ignore();
					cin >> start;

				} while (start == 'n' || start == 'N');

				strcpy(inputChar, "Start");

				send(comm->datasock, inputChar, sizeof(inputChar), 0);

				clientCounter = 2;
			}

			else
			{
				scanf("%s", inputChar);
				send(comm->datasock, inputChar, sizeof(inputChar), 0);
			}
		}
	}

	return 0;
}

/************************************************************************************
* VOID client_iface_thread(LPVOID)
*
* Description: Thread communicating commands from client and the status of their
*				completion back to client.
*
*
************************************************************************************/
VOID client_iface_thread(LPVOID parameters) //LPVOID parameters)
{
	sp_struct_t profiler = (sp_struct_t)parameters;
	sp_comm_t comm = &profiler->comm;
	INT retval;
	struct sockaddr_in saddr;
	int saddr_len;
	int counter = 0;
	char ParamBuffer[110];



	while (ParamBuffer[0] != '!')
	{
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len = sizeof(saddr);
		retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
		printf("%s \n", ParamBuffer);
	}

	done = false;
}

/*
 *	Function: load_coefficient
 *	Parameter(s): filterFile - pointer of type char that contains the name of the file to be read.
 *	Returns: void
 *	Description: The function opens the file (filter coefficient 'b.txt') that the user entered, then it
 *	reads line by line of the file and sends the data read to the function convert_string to conver the
 *	string read to the equivalent double value.
 *	Finally, when the function receives the double value it saves it to the filterCoefficient array. Once
 *	it finished reading the file and converting the values, the function closes the file and returns to
 *	the main function.
 */
void load_coefficient(char *filterFile)
{
	int index = 0;
	char buffer[1000];

	FILE *fp = fopen(filterFile, "r");

	while (fgets(buffer, COEF_MAX, fp) != NULL)
	{
		double val = convert_string(buffer);

		filterCoefficient[index] = val;

		index++;
	}

	fclose(fp);
}

/*
 *	Function: convert_string
 *	Parameter(s): buffer - pointer of type char that holds the value in string format to be converted to double
 *	Returns: value - a double variable that was converted from a character array.
 *	Description: This function converts a character array holding a numerical value to a double.
 */
double convert_string(char *buffer)
{
	int index, sign, decimal, ctr;
	int expSign, exp, expCtr;
	double value, number;

	value = number = 0.0;
	exp = expCtr = ctr = 0;

	//Determines if the number in the buffer is positive of negative
	sign = buffer[0] == '-' ? -1 : 1;

	//Gets the numbers in the buffer before the decimal point
	for (index = 0; index < strlen(buffer); index++)
	{
		//Converts the character number to a double value
		if (buffer[index] >= '0' && buffer[index] <= '9')
			value = (buffer[index] - 48) * sign / 1.0;

		//Checks if the buffer at a certain location has the decimal point and breaks the loop
		if (buffer[index] == '.')
		{
			decimal = index;
			break;
		}
	}

	//Gets all of the digits after the decimal point and stops when it reaches the exponential character 'e'
	decimal++;
	for (index = decimal; index < strlen(buffer); index++)
	{
		//Converts the character digits to actual numbers
		if (buffer[index] >= '0' && buffer[index] <= '9')
		{
			ctr--;

			number = buffer[index] - 48;

			number = number * pow(10, ctr);

			value = value + number;
		}

		//Breaks the loop if it reaches the exponential character
		if (buffer[index] == 'e')
		{
			expCtr = index;

			break;
		}
	}

	//Gets all of the digits after the exponential character
	expCtr++;
	for (index = expCtr; index < strlen(buffer); index++)
	{
		//Determines if the exponential is a positive or negative value
		expSign = buffer[expCtr] == '-' ? -1 : 1;
		
		//Converts the character number to a numerical value
		if (buffer[index] >= '0' && buffer[index] <= '9')
		{
			exp = exp + (buffer[index] - 48);
			
			exp = exp * expSign;
		}
	}

	//Finishes calculating the double value that was computed from the previous steps
	value = value * pow(10,exp) / 1.0;

	return value;
}

