// <<<BREAKDOWN VERSION ONE>>>


// includes
#include <ctype.h>		// Functions for character classification
#include <errno.h>		// Macros for reporting and retrieving error conditions
#include <stdio.h>		// Input and output facilites
#include <stdlib.h>		// General utilities: memory mangement, program utilities, string conversions, random numbers 
#include <termios.h>	// Terminal I/O interfaces 
#include <unistd.h>		// Standard symbolic constants and types

// data
struct termios orig_termios;	// Structure to hold original terminal attributes

// terminal
void die(const char *s) {	// Function to handle fatal errors
	perror(s);	// Print a system error message
	exit(1);	// Terminate the program with an error status
}

void disableRawMode(void) {		// Function to restore original terminal attributes
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tscetattr");	// If setting the terminal attributes fails, terminate the progrsm
}

void enableRawMode(void) {		// Function to set the terminal to raw mode
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");	// If getting the terminal attributes fails, terminate the program
	atexit(disableRawMode);		// Register disableRawMode to be called when the program exits
	
	struct termios raw = orig_termios;	// Copy the original terminal attributes
	// Modify the copy to set the terminal to raw mode
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcgetattr");	// If setting the terminal attributes fails, terminate the program
}

// init
int main(void) {	// Main function
	enableRawMode();	// Set the terminal to raw mode
	
	while(1) {	// Infinite loop
	
		char c = '\0';	// Initialize a character variable
		if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");	// If reading from the terminal fails, terminate the program
		// Print the ASCII value of the character, and the character itself if it's not a control character
		if (iscntrl(c)) {
			printf("%d\r\n", c);
		}
		else {
			printf("%d ('%c')\r\n", c, c);
		}
		if (c == 'q') break;	// If the character is 'q', break the loop
	}
	
	return 0;	// Return a success status
}


/** ADDITOINAL INFORMATION:



[ raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); ]

this^ line modifies the input flags of the terminal settings:

RKINT: This flag is used to cause a break condition to send a SIGNINT signal, similar to an interrupt  Disabling this means break conditions are ignored

ICRNL: This flag is used to translate carriage return to newline on input. Disabling this means carriage return is read as a carriage return, not as a newline

INPCK: This flag enables parity checking. Disabling this turns off parity checking

ISTRIP: This flag casuses the 8th bit of each input byte to be stripped, meaning it's set to zero. Disabling this stops the stripping

IXON: This flag enables XON/XOFF flow control on output. Disabling this turns off these start/stop output control, meaning the terminal won't interpet CRT-S & CTRL-Q specially



[ raw.c_oflag &= ~(OPOST); ]

this^ line modifiles the output flags of the terminal settings:

OPOST: This flag enables post-processing of output. Disabling this mean the output is sent to the terminal as-is, without any post-processing



[ raw.c_cflag |= (CS8); ]

this^ line modifies the control flags of the terminal settings

CS8: this flag sets the character size (CS) to 8 bits per byte



[ raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); ]

this^ line modifies the local flags of the terminal settings

ECHO: This flag causes the terminal to echo input character. Disabling this mean the terminal won't automatically echo input characters

ICANON: This flag enables canonical mode. Disabling this meas the terminal will be in non-canonical mode, i.e, input is available immediately rather than after a newline

IEXTEN: This flag enables extended input character processing. Disabling this turns off extended processing

ISIG: This flag enables the generation of signals. Disabling this means the terminal won't generate signals like SIGNIT for CTRL-C & SIGTSTP for CTRL-Z



[ raw.c_cc[VMIN] = 0; ]
[ raw.c_cc[VTIME] = 1; ]

these^ lines set the minimum of characters for non-canonical read (VMIN) and the timeout is deciseconds for non-canonical read (VTIME)



[ if (tcgetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcgetattr"); ]

this^ line applies the modified terminal settings/ TCSAFLUSH option is used to change the atributes after all the output has been transmitted, and all input that has been received but not read will be discarded. If there's an error, the program will terminate with a message



**/
