// Includes necessary libraries
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// note to self -- next time use conventional else statment blocking

// Defines constants
#define TEXTEDIT_VERSION "0.0.1"	// Defines the version of the text editor

#define CTRL_KEY(k) ((k) & 0x1f)	// Define a macro to map a character to its control key equivalent

// Define an enumeration for special keys
enum editorKey {
	ARROW_LEFT = 1000,		// Start from 1000 to avoid conflict with regular char values- incrementing by 1
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};
	
// Define a structure to hold the state of the editor
struct editorConfig {
	int cx, cy;		// curser position
	int screenrows;		// Number of rows in the screen
	int screencols;		// Number of columns in the screen
	struct termios orig_termios;	// Original terminal settings
};

// Declare a global instance of the editor configuration
struct editorConfig E;

// Function to handle errors
void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);		// Write the escape sequence "\x1b[2J" to the standard output. This clears the screen
	write(STDOUT_FILENO, "\x1b[H", 3);		// Write the escape sequence "\x1b[H" to the standard output. This moves the cursor to the top-left corner of the screen
	
	perror(s);		// Print the string pointed to by 's' and a description of the last error that occurred
	exit(1);		// Terminate the program and return the value 1 to the operating system, indicating an error occurred
}

// Function to disable raw mode
void disableRawMode(void) {
	// tcsetattr sets the parameters associated with the terminal (referred to by the file descriptor STDIN_FILENO) from the termios structure referred to by &E.orig_termios
	// TCSAFLUSH constant is used to change the attributes after all the output written to the object referred by STDIN_FILENO has been transmitted, and all input that has been received but not read will be discarded  before the change is made
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tscetattr");	// If there's an error, call the die function with the message "tcsetattr"
}

// Function to enable raw mode
void enableRawMode(void) {
	// tcgetattr gets the parameters associated with the terminal (referred to by the file discriptor STDIN_FILENO) and stores them in the termios structure pointed to by &E.orig_termios
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");	// If there's an error, call the die function with the message "tcgetattr"
	
	// Register the disableRawMode function to be called automatically when the program exits
	atexit(disableRawMode);	
	
	// Create a copy of the original termios structure (terminal settings)
	struct termios raw = E.orig_termios;
	
	// Modify the copy to disable various flags:
	// Input flags: BRKINT, ICRNL, INPCK, ISTRIP, IXON
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	// Output flags: OPOST
	raw.c_oflag &= ~(OPOST);
	// Control flags: CS8
	raw.c_cflag |= (CS8);
	// Local flags: ECHO, ICANON, IEXTEN, ISIG
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	// Control characters: VMIN, VTIME
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	// tcsetattr sets the parameters associated with the terminal (referred to by the file discriptor STDIN_FILENO) from the termios structure referred to by &raw
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcgetattr");	// If there's an error, call the  die function with the message "tcsetattr"
}

// Function to read key from the terminal
int editorReadKey(void) {
	int nread;
	char c;
	
	// Using a loop to keep trying to read a character from the standard input
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		// If read() returns -1, there was an error. If errno is EAGAIN, we try again
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	
	// If the character read was an escape character, we have to read two or more characters
	if (c == '\x1b') {
		char seq[3];
		
		// Try to read the next two characters
		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
		
		// If the first character is '[', it could be an arrow key or a home/end key
		if (seq[0] == '[') {
			// If the second character is a digit, it could be a home/end key or a page up/down key
			if (seq[1] >= '0' && seq[1] <= '9') {
				// Try to read the next character
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				// If the third character is '~', it's a home/end key or a page up/down key
				if (seq[2] == '~') {
					switch (seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}
			// If the second character is not a digit, it's an arrow key or a home/end key		
			else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
		}
		// If the first character is '0', it's a home/end key
		else if (seq[0] == '0') {
			switch (seq[1]) {
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}
		return '\z1b';
	}
	// If it's not an arrow key, home/end key, or page up/down key, just return the escape character
	else {
		// if it's not an escape character, just return the character
		return c;
	}
}

// Function to get cursor position
int getCursorPosition(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
	
	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) !=1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';
	
	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
	
	return 0;
}

// Function to get window size
int getWindowSize(int *rows, int *cols) {
	struct winsize ws;
	
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return getCursorPosition(rows, cols);
	}
	
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

// Define a structure for a dynamic string (append buffer)
struct abuf {
	char *b;	// Buffer
	int len;	// Length
};

// Initialize an append buffer to NULL with length 0
#define ABUF_INIT {NULL, 0}

// Function to append to buffer
void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);
	
	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

// Function to free buffer
void abFree(struct abuf *ab) {
	free(ab->b);
}

// Function to draw rows of the editor
void editorDrawRows(struct abuf *ab) {
	int y;
	for (y = 0; y < E.screenrows; y++) {
		if (y == E.screenrows / 3) {
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Text Editor -- Version %s", TEXTEDIT_VERSION);
			if (welcomelen > E.screencols) welcomelen = E.screencols;
			int padding = (E.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		}
		
		else {
			abAppend(ab, "~", 1);
		}
		
		abAppend(ab, "\x1b[K", 3);
		if (y < E.screenrows - 1) {
			abAppend(ab, "\r\n", 2);
		}
	}
}

// Function to refresh the screen
void editorRefreshScreen(void) {
	struct abuf ab = ABUF_INIT;
	
	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);
	
	editorDrawRows(&ab);
	
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));
	
	abAppend(&ab, "\x1b[?25h", 6);
	
	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

// Function to move the cursor
void editorMoveCursor(int key) {
	switch (key) {
		case ARROW_LEFT:
			if (E.cx != 0) {
				E.cx--;
			}
			break;
		case ARROW_RIGHT:
			if (E.cx != E.screencols - 1) {
				E.cx++;
			}
			break;
		case ARROW_UP:
			if (E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if (E.cy != E.screenrows - 1) {
			E.cy++;
			}
			break;
	}
}

// Function to process keypress
void editorProcessKeypress(void) {
	int c = editorReadKey();
	
	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
			
		case HOME_KEY:
			E.cx = 0;
			break;
			
		case END_KEY:
			E.cx = E.screencols - 1;
			break;
		
		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screenrows;
				while (times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;
		
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}

// Function to initialize the editor
void initEditor(void) {
	E.cx = 0;
	E.cy = 0;
	
	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

// Main Function
int main(void) {
	enableRawMode();	// Enable raw mode in the terminal
	initEditor();		// Initialize the editor
	
	while(1) {		// Main program loop
		editorRefreshScreen();		// Refresh the screen
		editorProcessKeypress();	// Process a keypress
	}
	
	return 0;	// Exit the program
}



/***
BREAKDOWN OF EACH FUNCTION:

[ die(const char *s) ]
This function is called when a fatal error occurs. It clears the screen, 
moves the curser to the top-left corner, prints the error message 
pointed to by s, and terminates the program with an error code



[ disableRawMode(void) ]
This function is used to disable raw mode for the terminal, which is a mode
allowing you to process input and output character by character rather than
line by line. It does this by restoring the original terminal attributes that
were saved when raw mode was enabled. If there's an error while trying to
set the terminal attributes, it called the die function with an error message



[ enableRawMode(void) ]
This function is used to enable raw mode for the terminal, which is a mode
allowing you to process input and output character by character rather than
line by line. It does this by getting the current terminal attributes,
modifying them to disable various flags, and then setting the terminal
attributes. If there's an error message. It also registers the disableRawMode
function to be called automatically when the program exits, to ensure that
raw mode is disabled even if the program terminates unexpectedly



[ editorReadKey(void) ]
This function reads a key from the terminal. If the key is an escape character,
it reads two more characters to check if it's an arrow key, home/end key,
or page up/down key escape sequence. If it is, it returns a corresponding
key code. If it's not once of these escape sequences, it just returns
the escape character. If the key is not an escape character, it just returns
the key



[ getCursorPosition(int *rows, int *cols) ]
This function queries the terminal for the current position of the cursor. It
stores the row and column in the variables pointed to by rows & cols



[ getWindowSize(int *rows, int *cols) ]
This function queries the terminal for its size. It stores the number of rows
and columns in the variables pointed to by rows and cols



[ abAppend(struct abuf *ab, const char *s, int len) ]
This function appends the string s of length len to the append buffer ab



[ abFree(struct abuf *ab) ]
This function frees the memory used by the append buffer ab



[ editorDrawRows(struct abuf *ab) ]
This function appends a row of tildes (~) to the append buffer ab. This gives
the editor a visual appearance similar to vim



[ editorRefreshScreen(void) ]
This function refreshes the screen by drawing each row of the text buffer



[ editorMoveCursor(int key) ]
This function moves the cursor based on the key that was pressed. It
handles the arrow keys and home/end keys



[ editorProcessKeypress(void) ]
This function waits for a keypress and then handles it. It's responsible for
dispatching the keypress to the appropriate handler function


[ initEditor(void) ]
This function initializes the editor. It sets the cursor position to the
top-left corner and gets the size of the terminal



[ main(void) ]
Main program loop. It enables raw mode, initializes the editor, and then
enters a loop where it continually refreshes the screen and processes keypresses





BREAKDOWN OF NON-FUNCTIONS:

[ Defines ]
These are the constants defined for the program. TEXTEDIT_VERSION is the
version of the text editor, and CTRL_KEY(k) is a macro that masks the upper 3
bits of a character, effectively mapping it to its control key equivalent



[ Enums ]
editorKey is an enumeration of special keys that the editor will recognize.
These are used to handle arrow keys and other special keys



[ struct editorConfig ]
This is a structure that holes the state of the editor, including cursor
position, screen size, and original terminal settings. E is a global instance
of this structure



[ struct abuf ]
Append Buffer: This is a dynamic string implementation. It's used to build up
a string incrementally before writing it to the output all at once



These parts of the code are essential for setting up the environment, defining 
constants, and managing the stat of the editor