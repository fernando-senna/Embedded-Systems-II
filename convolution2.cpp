#include <iostream>
#include <stdio.h>
using namespace std;

void convolution(double arrayA[], double arrayB[], int lenA, int lenB, double arrayC[]);

int main(void)
{
	double A[7] = {1,2,3,7,8,9};
	double B[4] = {4,5};
	double C[4];
	int index = 0;

	convolution(A,B,6,2,C);

	for(index = 0;index < 7;index++)
		printf("C[%d] = %.2f\n", index, C[index]);

	return 0;
}

/*
 *	Function:
 *	Parameter(s):
 *	Returns:
 *	Description:
 *	Source: http://toto-share.com/2011/11/cc-convolution-source-code/
 */
void convolution(double arrayA[], double arrayB[], int lenA, int lenB, double arrayC[])
{
	int i, j, k, newConv;
	double sum;

	//Allocates space for the convolution array
	newConv = lenA + lenB - 1;
	
	//Convolution process
	for (i = 0; i < newConv; i++)
	{
		k = i;
		sum = 0;

		for (j = 0; j < lenB; j++)
		{
			if (k >= 0 && k < lenA)
				sum = sum + (arrayA[k] * arrayB[j]);

			k = k - 1;
			arrayC[i] = sum;
		}
	}

	//*lenC = newConv;

	//return result;
}