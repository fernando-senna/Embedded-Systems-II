/*
 *	Authors:		Fernando Do Nascimento
 *					Isaac Ramirez
 *	Assignment:		Real Time Data Acquisition System with Network Communication
 *	Course:			CSE 4342 - Embedded Systems II
 *	Date:			4-30-2016
 *	Description:
 */

//Included Libraries (Server & DT9816)
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "oldaapi.h"
#include <fstream>
#include <iostream>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <time.h>
#include <vector>
#include <WinSock.h>

using namespace std;

/* Server Setup */
#define ERR_CODE_NONE	0	//No Error
#define ERR_CODE_SWI	1	//Software Error

#define CMD_LENGTH		5

#define ARG_NONE		1
#define ARG_NUMBER		2

#define RES				16
#define SWITCH_MIN		45000
#define FILTER_LEN		101

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
/* End Server Setup */

/* Global Variables */
int counter = 0;
int lastVal = 0;
int firstVal = 1;
int currVal = 0;
int serverCounter = 0;
int clientCounter = 0;
int lengthResult = 0;
int display = 0;

double filterCoefficient[FILTER_LEN];
double sampleRate;

double *convolutionResult;
double *currBuffer;
double *prevBuffer;

double transient = 0.0;

bool done = true;
bool beginSampling = false;
bool switchStatus = true;		//True = On. False = Off
bool switchFlag = true;

char inputChar[110] = "";

DBL gain = 1.0;
DBL max = 1;
DBL min = 0;
DBL volt;
DBL voltSW;

HDASS hAD = NULL;

ULNG value;
ULNG switchValue;
ULNG sample;

UINT channel = 0;
UINT encoding = 0;
UINT resolution = 0;

PDWORD pBuffer32 = NULL;

PWORD pBuffer = NULL;
/* End Global Variables */

//Server Setup Global Variables
struct sp_struct profiler;
struct sockaddr_in saddr;
struct hostent *hp;
sp_comm_t comm;

/* Function Prototypes */
void set_dt9816(void);
void activate_led(int num);
double minimum();
double maximum();
double average();
double variance();
void convolution(double *currBuf, int lenCurr, double coef[], double *convolutionResult);
//void realTimeConvolution(double currBuf[], int lenCurr, double prevBuf[], int lenPrev, double coef[], int lenCoef, double convResult[], int lenResult);
/* End Function Prototypes */

/* DT9816 Setup */
//DT9816 Preprocessor Declarations
#define NUM_OL_BUFFERS 4
#define NUM_BUFFERS	4
#define STRLEN		80

//DT9816 Variables
char str[STRLEN];

//Error Handling Macros
#define SHOW_ERROR(ecode) MessageBox(HWND_DESKTOP,olDaGetErrorString(ecode,\
                  str,STRLEN),"Error", MB_ICONEXCLAMATION | MB_OK);

#define CHECKERROR(ecode) if ((board.status = (ecode)) != OLNOERROR)\
                  {\
                  SHOW_ERROR(board.status);\
                  olDaReleaseDASS(board.hdass);\
                  olDaTerminate(board.hdrvr);\
                  return ((UINT)NULL);}

#define CHECKERROR( ecode )                           \
do                                                    \
{                                                     \
   ECODE olStatus;                                    \
   if( OLSUCCESS != ( olStatus = ( ecode ) ) )        \
   {                                                  \
      printf("OpenLayers Error %d\n", olStatus );  \
      exit(1);                                        \
   }                                                  \
}                                                     \
while(0)  

//Structures Used with Board
typedef struct tag_board
{
	HDEV hdrvr;			//Device Handle
	HDASS hdass;		//Subsystem Handle
	ECODE status;		//Board Error Status
	HBUF hbuf;			//Subsystem Buffer Handle
	PWORD lpbuf;		//Buffer Pointer

	char name[MAX_BOARD_NAME_LENGTH];	//String for Board Name
	char entry[MAX_BOARD_NAME_LENGTH];	//String for Board Name
}BOARD;
/* End Structures Used with Board */

typedef BOARD *LPBOARD;

static BOARD board;

BOOL CALLBACK
GetDriver(LPSTR lpszName, LPSTR lpszEntry, LPARAM lParam)
/*
 *	This is a callback function of olDaEnumBoards, it gets the strings of the
 *	Open Layers board and attemps to initialize the board. If successful,
 *	enumeration is halted.
 */
{
	LPBOARD lpboard = (LPBOARD)(LPVOID)lParam;

	//Fill in Board Strings
	lstrcpyn(lpboard->name, lpszName, MAX_BOARD_NAME_LENGTH - 1);
	lstrcpyn(lpboard->entry, lpszEntry, MAX_BOARD_NAME_LENGTH - 1);

	//Try to Open Board
	lpboard->status = olDaInitialize(lpszName, &lpboard->hdrvr);

	if (lpboard->hdrvr != NULL)
		return FALSE;	//False to Stop Enumerating

	else
		return TRUE;	//True to Continue Enumerating
}

/*
 *	Data Acquisition
 */
LRESULT WINAPI
WndProc(HWND hWnd, UINT msg, WPARAM hAD, LPARAM lParam)
{
	int i, j;
	i = j = 0;
	switch (msg)
	{
		case OLDA_WM_BUFFER_DONE:
			HBUF hBuf;

			CHECKERROR(olDaGetBuffer((HDASS)hAD, &hBuf));
		
			if (hBuf && beginSampling == true)
			{
				olDaGetRange((HDASS)hAD, &max, &min);
				olDaGetEncoding((HDASS)hAD, &encoding);
				olDaGetResolution((HDASS)hAD, &resolution);
				olDmGetValidSamples(hBuf, &sample);

				if (resolution > RES)
				{
					olDmGetBufferPtr(hBuf, (LPVOID *)&pBuffer32);
					value = pBuffer32[sample - 1];
				}

				else
				{
					olDmGetBufferPtr(hBuf, (LPVOID *)&pBuffer);
					switchValue = pBuffer[sample - 1];

					ULNG newValue;
					double newVolt;

					currBuffer = (double *)malloc(sizeof(double) * (int)sampleRate);
					//prevBuffer = (double *)malloc(sizeof(double) * (int)sampleRate);
					convolutionResult = (double *)malloc(sizeof(double) * ((int)sampleRate + FILTER_LEN - 1));
					
					for (i = 0; i < sample; i += 2)
					{

							newValue = pBuffer[i];
							newVolt = ((float)max - (float)min) / (1L << resolution) * newValue + (float)min;
							currBuffer[j] = newVolt;

						j++;
					}

					
					if (switchValue >= SWITCH_MIN)
					{
						if (switchFlag == true)
						{
							int val = 0;

							send(comm->datasock, (char *)&val, sizeof(val), 0);
							
							sprintf(inputChar, "Real Time Data Acquisition Started!");

							if (display == 0)
							{
								display = 1;
								cout << inputChar << endl;
							}

							send(comm->datasock, inputChar, sizeof(inputChar), 0);
							
							//LED ON - DATA ACQUISITION STARTED
							activate_led(1);
						}

						//Convolution
						int convLen = (int)sampleRate + FILTER_LEN - 1;
						convolution(currBuffer, (int)sampleRate, filterCoefficient, convolutionResult);
						//realTimeConvolution();

						//Calculates Max, Min, Avg, and Variance.
						//Sends the results to the client
						int serverFlag = 1;
						send(comm->datasock, (char *)&serverFlag, sizeof(serverFlag), 0);
						sprintf(inputChar, "Min = %.4f Max = %.4f Avg = %.4f Var = %.4f", minimum(), maximum(), average(), variance());
						send(comm->datasock, inputChar, sizeof(inputChar), 0);
						
						//Sends the convolution result to the client
						for (int index = 0; index < (2000 + 101 - 1); index++)
							send(comm->datasock, (char *)&convolutionResult[index], sizeof(convolutionResult[index]), 0);
						
						//Frees memory in order to avoid the program from crashing and preventing memory leaks
						free(currBuffer);
						free(convolutionResult);
						
						switchStatus = false;
						switchFlag = false;
					}

					else
					{
						//LED OFF - DATA ACQUISITION STOPPED
						activate_led(0);

						if (switchStatus == false)
						{
							int val = 0;

							send(comm->datasock, (char *)&val, sizeof(val), 0);

							sprintf(inputChar, "Real Time Data Acquisition Stopped!");
							send(comm->datasock, inputChar, sizeof(inputChar), 0);

							switchStatus = true;
							switchFlag = true;
						}
					}
				}
				
				olDaPutBuffer((HDASS)hAD, hBuf);
			}

			break;

		case OLDA_WM_QUEUE_DONE:
			printf("\nReal Time Data Acquisition Stopped!");
			PostQuitMessage(0);
			break;

		case OLDA_WM_TRIGGER_ERROR:
			printf("\nTrigger Error: Real Time Data Acquisition Stopped!");
			PostQuitMessage(0);
			break;

		case OLDA_WM_OVERRUN_ERROR:
			printf("\nInput Overrun ErrorL Real Time Data Acquisition Stopped");
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, msg, hAD, lParam);
	}

	return 0;
}

/*
 *	Initialize DT9816 Board
 */
BOOL CALLBACK
EnumBrdProc(LPSTR lpszBrdName, LPSTR lpszDriverName, LPARAM lParam)
{

	// Make sure we can Init Board
	if (OLSUCCESS != (olDaInitialize(lpszBrdName, (LPHDEV)lParam)))
	{
		return TRUE;  // try again
	}

	// Make sure Board has an A/D Subsystem 
	UINT uiCap = 0;
	olDaGetDevCaps(*((LPHDEV)lParam), OLDC_ADELEMENTS, &uiCap);
	if (uiCap < 1)
	{
		return TRUE;  // try again
	}

	printf("%s succesfully initialized.\n", lpszBrdName);
	return FALSE;    // all set , board handle in lParam
}

int main(void)
{
	int i;
	int res = 0;

	//DT9816 Setup
	set_dt9816();

	//Server Setup
	memset(&profiler, 0, sizeof(profiler));
   comm = &profiler.comm;

	if ((res = WSAStartup(0x202, &wsaData)) != 0)
	{
		fprintf(stderr, "WSAStartup failed with error %d\n", res);
		WSACleanup();

		return ERR_CODE_SWI;
	}

	//Setup Data Transmition Socket to Broadcast Data
	hp = (struct hostent*)malloc(sizeof(struct hostent));
	hp->h_name = (char*)malloc(sizeof(char) * 17);
	hp->h_addr_list = (char**)malloc(sizeof(char*) * 2);
	hp->h_addr_list[0] = (char*)malloc(sizeof(char) * 5);

	strcpy(hp->h_name, "lab_example\0");

	hp->h_addrtype = 2;
	hp->h_length = 4;

	//Broadcast in 255.255.255.255 Network
	hp->h_addr_list[0][0] = (signed char)169;	//192;129
	hp->h_addr_list[0][1] = (signed char)254;	//168;107
	hp->h_addr_list[0][2] = (signed char)56;	//0;255  
	hp->h_addr_list[0][3] = (signed char)144;	//140;255
	hp->h_addr_list[0][4] = 0;
	
	//Setup a Socket for Broadcasting Data
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = hp->h_addrtype;

	memcpy(&(saddr.sin_addr), hp->h_addr, hp->h_length);
	saddr.sin_port = htons(1500);

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

	//Setup and Bind a Socket to Listen for Commands from Client
	memset(&saddr, 0, sizeof(struct sockaddr_in));

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(1024);

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

	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = "ServerClass";
	RegisterClass(&wc);

	HWND hWnd = CreateWindow(wc.lpszClassName,
		NULL,
		NULL,
		0, 0, 0, 0,
		NULL,
		NULL,
		NULL,
		NULL);

	if (!hWnd)
		exit(1);

	while (beginSampling == false)
		;

	HDEV hDev = NULL;
	CHECKERROR(olDaEnumBoards(EnumBrdProc, (LPARAM)&hDev));

	CHECKERROR(olDaGetDASS(hDev, OLSS_AD, 0, &hAD));

	CHECKERROR(olDaSetWndHandle(hAD, hWnd, 0));

	CHECKERROR(olDaSetDataFlow(hAD, OL_DF_CONTINUOUS));		//Continuous Data
	CHECKERROR(olDaSetChannelListSize(hAD, 2));				//2 Analog Channels Used
	CHECKERROR(olDaSetChannelListEntry(hAD, 0, 0));			//Channel 0 Entry
	CHECKERROR(olDaSetChannelListEntry(hAD, 1, 1));			//Channel 1 Entry
	CHECKERROR(olDaSetGainListEntry(hAD, 0, 1));			//Channel 0 Gain
	CHECKERROR(olDaSetGainListEntry(hAD, 1, 1));			//Channel 1 Gain
	CHECKERROR(olDaSetTrigger(hAD, OL_TRG_SOFT));	
	CHECKERROR(olDaSetClockSource(hAD, OL_CLK_INTERNAL));
	CHECKERROR(olDaSetClockFrequency(hAD, sampleRate));		//Sample Rate Set by Client
	CHECKERROR(olDaSetWrapMode(hAD, OL_WRP_NONE));
	CHECKERROR(olDaConfig(hAD));

	HBUF hBufs[NUM_OL_BUFFERS];
	for (i = 0; i < NUM_OL_BUFFERS; i++)
	{
		if (OLSUCCESS != olDmAllocBuffer(GHND, sampleRate * 2, &hBufs[i]))
		{
			for (i--; i >= 0; i--)
				olDmFreeBuffer(hBufs[i]);

			exit(1);
		}
		olDaPutBuffer(hAD, hBufs[i]);
	}

	if (OLSUCCESS != (olDaStart(hAD)))
		printf("A/D Operation Start Failed ... hit any key to terminate.\n");

	MSG msg;
	SetMessageQueue(50);		

	while (GetMessage(&msg, hWnd, 0, 0))
	{
		TranslateMessage(&msg);			//Translate virtual key codes
		DispatchMessage(&msg);			//Dispatch message to window

		if (_kbhit())
		{
			_getch();
			PostQuitMessage(0);
		}
	}

	while (1)
		;

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
	comm = &profiler->comm;
	INT retval;
	struct sockaddr_in saddr;
	int saddr_len;
	char ParamBuffer[110];

	//Variables
	double temp;
	char start[6];

	printf("Server Program\n");
	printf("--------------\n");

	while (ParamBuffer[0] != '!')
	{
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len = sizeof(saddr);

		if (serverCounter == 0)
		{
			recv(comm->cmdrecvsock, (char *)&sampleRate, sizeof(sampleRate), 0);

			for (int index = 0; index < FILTER_LEN; index++)
			{
				recv(comm->cmdrecvsock, (char *)&temp, sizeof(temp), 0);
				filterCoefficient[index] = temp;
			}

			cout << "Sample Rate & Filter Coefficients Received!" << endl;

			serverCounter = 1;
		}

		//Waiting to receive the start signal
		if (serverCounter == 1)
		{
			recv(comm->cmdrecvsock, (char *)&start, sizeof(start), 0);

			if (strcmp(start, "Start") == 0)
			{
				int val = 0;

				send(comm->datasock, (char *)&val, sizeof(val), 0);

				sprintf(inputChar, "\nSampling Process Started! Waiting for the user to press the switch!");

				send(comm->datasock, inputChar, sizeof(inputChar), 0);

				beginSampling = true;

				serverCounter = 2;
			}
		}

		if (clientCounter == 0)
		{
			while (done == true)
			{
				if (_kbhit())
				{
					scanf("%s", inputChar);
					send(comm->datasock, inputChar, sizeof(inputChar), 0);
				}
			}
		}
	}
	done = false;
}

/*
 *	Function: set_dt9816
 *	Parameter(s): void
 *	Returns: void
 *	Description: After the server receives from the client the sampleFrequency and the filter coeffcient,
 *	this functions sets up the DT9816 in order for the program to be able to begin the data acquisition.
 */
void set_dt9816(void)
{
	//Setup DT9816
	board.hdrvr = NULL;

	CHECKERROR(olDaEnumBoards(GetDriver, (LPARAM)(LPBOARD)&board));
	CHECKERROR(board.status);

	if (board.hdrvr == NULL)
	{
		MessageBox(HWND_DESKTOP, "No Open Layer Boards!!!", "Error",
			MB_ICONEXCLAMATION | MB_OK);
	}

	CHECKERROR(olDaGetDASS(board.hdrvr, OLSS_DOUT, 0, &board.hdass));
	CHECKERROR(olDaSetDataFlow(board.hdass, OL_DF_SINGLEVALUE));
	CHECKERROR(olDaConfig(board.hdass));
	CHECKERROR(olDaGetResolution(board.hdass, &resolution));

	value = 0;

	CHECKERROR(olDaPutSingleValue(board.hdass, value, channel, gain));
	CHECKERROR(olDaReleaseDASS(board.hdass));
	CHECKERROR(olDaTerminate(board.hdrvr));
}

/*
 *	Function: minimum
 *	Parameter(s): values - an array of type double that stores the results collected from the DT9816
 *	Returns: The minimum value of the data collected.
 *	Description: The function takes the the first value of the array and sets it to be its minimum,
 *	then the function loops thru the entire array and comparing the value with the arbitrary minimum value.
 *	If the value compared is less than the arbitrary value, then that number replaces the arbitrary value.
 *	Once the loop finishes, the function returns the minimum value of the array.
 */
double minimum()
{
	int len, index;
	double min;

	len = (int)sampleRate + FILTER_LEN - 1;
	index = 100;					//Reads data after 100 values

	min = convolutionResult[100];	//Sets arbitrary minimum value

	//Loops thru the array
	while (index < (len - 100))
	{
		//Checks if the value in the array is less than the arbitrary value
		if (convolutionResult[index] < min)
			min = convolutionResult[index];

		index++;
	}

	return min;
}

/*
 *	Function: maximum
 *	Parameter(s): void
 *	Returns: The maximum value from the array
 *	Description: The function sets an arbitrary maximum value, loops thru the entire array comparing values
 *	looking for the largest number of the array and updates the max variable.
 */
double maximum()
{
	int len, index;
	double max;

	max = convolutionResult[200];	//Sets arbitrary maximum value
	
	// (-2) due to first removing 1 from the filter coef and 2nd removing 1 from the max value.
	// (-200) because we want to get every 200 values
	len = (int)sampleRate + FILTER_LEN - 1 - 1 - 200;
	
	//Loops thru the entire array looking for the max value & updating the max variable
	//if the value compared is grater than the arbitrary max
	for (index = 200; index < len; index++)
		if (convolutionResult[index] > max)
			max = convolutionResult[index];

	return max;
}

/*
 *	Function: average
 *	Parameter(s): void
 *	Returns: The average result of the data collected.
 *	Description: The function takes the double array with the data collected, then sums all of the
 *	values, and finally calculates and returns the average.
 */
double average()
{
	int index, len, division;
	double sum;

	// (-1) due to removing 1 value from filter coef and -100 because we want to get
	// data after the first 100 numbers read
	len = (int)sampleRate + FILTER_LEN - 1 - 100;
	division = (int)sampleRate + FILTER_LEN - 1;
	sum = 0.0;

	//Adds all of the values of the array
	for (index = 100; index < len; index++)
		sum = sum + convolutionResult[index];

	//Calculates & Returns the average
	return sum / division;
}

/*
 *	Function: variance
 *	Parameter(s): values - an array of type double that stores the results collected from the DT9816
 *	Returns: The variance of the data collected from the DT9816
 *	Description: The function first calculates the average of the data collected, then for
 *	each number we subtract the average and square the result, and finally we calculate the
 *	average again to calculate the variance.
 *	Source: http://www.mathsisfun.com/data/standard-deviation.html
 */
double variance()
{
	int index, len, division;
	double firstAvg, sum;

	// -1 due to filter coefficient & -200 due to reading after the 1st 200 values
	len = (int)sampleRate + FILTER_LEN - 1 - 200;
	division = (int)sampleRate + FILTER_LEN - 1;
	firstAvg = sum = 0.0;

	firstAvg = average();

	for (index = 200; index < len; index++)
		sum = sum + pow((convolutionResult[index] - firstAvg), 2);

	//Calculates & Returns the Variance (Average)
	return sum / division;
}

/*
 *	Function: activate_led
 *	Parameter(s): number - integer value with number of digital output channel
 *	to turn on or off an LED on the IDL-800
 *	Returns:
 *	Description:
 */
void activate_led(int number)
{
	//Get first available Open Layer Board
	board.hdrvr = NULL;
	CHECKERROR(olDaEnumBoards(GetDriver, (LPARAM)(LPBOARD)&board));

	//Check for error within callback function
	CHECKERROR(board.status);

	if (board.hdrvr == NULL)
	{
		MessageBox(HWND_DESKTOP, "No Open Layer Boards!!!",
			"Error", MB_ICONEXCLAMATION | MB_OK);

		exit(0);
	}

	//Get hanle to DOUT subsystem
	CHECKERROR(olDaGetDASS(board.hdrvr, OLSS_DOUT, 0, &board.hdass));

	//Set subsystem for single value operations
	CHECKERROR(olDaSetDataFlow(board.hdass, OL_DF_SINGLEVALUE));
	CHECKERROR(olDaConfig(board.hdass));

	//Put all 0's single value
	CHECKERROR(olDaGetResolution(board.hdass, &resolution));

	if (number == 1)
		number = (1L << resolution) - 1;

	CHECKERROR(olDaPutSingleValue(board.hdass, number, channel, gain));

	CHECKERROR(olDaReleaseDASS(board.hdass));
	CHECKERROR(olDaTerminate(board.hdrvr));
}


/*
 *	Function: convolution
 *	Parameter(s): 	currBuffer - array with the data sampled using the DT9816.
 *					lenCurr - length of the current buffer.
 *					filterCoef - array witt the data read from the file b.txt
 *					lenCoef - length of the filterCoef array
 *					resut - array storing the result of the convolution
 *					lenResult - length of the convolution result
 *	Returns:		void
 *	Description:	This function calculates the convolution of the data at
 *					the current buffer using the filter coefficient received
 *					from the client.
 *					The function then stores the sum of the convolution in
 *					the array convolutionResult.
 *	Source:			http://toto-share.com/2011/11/cc-convolution-source-code/
 */
void convolution(double *currBuf, int lenCurr, double coef[], double *convolutionResult)
{
	int i, j, k, newConv;
	double sum;
	

	newConv = (int)sampleRate + FILTER_LEN - 1;

	for (i = 0; i < newConv; i++)
	{
		k = i;
		sum = 0;

		for (j = 0; j < FILTER_LEN; j++)
		{
			if (k >= 0 && k < (int)sampleRate)
			{
				sum = sum + (currBuf[k] * coef[j]);
			}

			k = k - 1;
			convolutionResult[i] = sum;
		}
	}
}

/*
 *
 */
void realTimeConvolution(double currBuf[], int lenCurr, double prevBuf[], int lenPrev,double coef[], int lenCoef, double convResult[], int lenResult)
{
	int i, j, k;
	double sum;
	double *buffer;

	buffer = (double *)malloc(sizeof(double) * (lenCurr + lenPrev));

	//Merges both buffers together
	j = k = 0;
	for (i = 0; i < (lenCurr + lenPrev); i++)
	{
		if (i < lenCurr)
		{
			buffer[i] = prevBuf[j];
			j++;
		}

		else
		{
			buffer[i] = currBuf[k];
			k++;
		}
	}

	//Real Time Convolution Calculation
	for (i = 0; i < lenResult; i++)
	{
		k = i;
		sum = 0;

		for (j = 0; j < lenCoef; j++)
		{
			if (k >= 0 && k < lenCurr + lenPrev)
				sum = sum + (buffer[k] * coef[j]);

			k = k - 1;
			convResult[i] = sum;
		}

		//Stores transient for the next convolution
		if (i == lengthResult - 1)
			transient = sum;
	}

	free(buffer);
}