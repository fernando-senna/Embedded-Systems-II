#ifndef VARIABLES_H
#define VARIABLES_H

//Global Variables
int counter = 0;
int lastVal = 0;
int firstVal = 1;
int currVal = 0;
int serverCounter = 0;
int clientCounter = 0;

double filterCoefficient[101]; 
double sampleRate;
double buffer[2000];

bool done = true;
bool beginSampling = false;

char inputChar[110] = "";

sp_comm_t comm;

DBL gain = 1.0;
DBL max = 1;
DBL min = 0;
DBL volt;
DBL voltSW;

HDASS hAD = NULL;

ULNG value;
ULNG valSW;
ULNG sample;

UINT channel = 0;
UINT encoding = 0;
UINT resolution = 0;

PDWORD pBuffer32 = NULL;

PWORD pBuffer = NULL;
#endif