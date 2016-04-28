#ifndef DT9816_H
#define DT9816_H

//DT9816 Preprocessor Declarations
#define NUM_OL_BUFFERS 4
#define NUM_BUFFERS	4
#define STRLEN		80

//DT9816 Variables
char str[STRLEN];

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

#endif