/*** includes ***/
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

/*** #defines ***/
// Now we map the 'cntrl + q' keys press to be quit rather than just q
// The CTRL_KEY macro bitwise-ANDs the charater provided in its argument with
// the value 00011111. This mirrors the action of the 'control key' on the keyboard
#define CNTRL_KEY(c) (0x1F & c) //mimicks the function fo the control key


/*** data ***/
// global variable to store the original state of the terminal before we edit it's 
// variable values. This allows us to restore the terminal 
// state to it's original value at program inception

struct editorCfg{
    int cur_x; // the horizontal coordinate of the cursor (the columns)
    int cur_y; // the vertical coordinate of the cursor (the rows)
    int screenRows;
    int screenColumns;
    struct termios orig_termios;
};

struct editorCfg EdiCfg;

/********************************************TERMINAL CONFG************************************/
/* the die function prints an error message and exits the function
    Most C library functions that fail will set the global errno variable 
    to indicate what the error was. perror() looks at the global errno variable and 
    prints a descriptive error message for it. It also prints the string given to it 
    before it prints the error message, which is meant to provide context about what 
    part of your code caused the error.
*/
void die(const char* s){
    //we would like to clear screen when 'CTRL Q' is pressed
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // write(STDOUT_FILENO, "\x1b[1J", 4);
    // write(STDOUT_FILENO, "\x1b[0J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s); // comes from the stdio.h library
    exit(1); // comes from the stdlib.h library
}

// calls the tcseattr() function to set the values of the terminal to the values
// of the default terminal before exit
void disbleRawMode(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &EdiCfg.orig_termios) == -1)
        die("tcsetattr");
}

// this function turns off terminal echo. 
// On default the terminal is in 'cooked mode' or 'canonical mode' meaning that characters are passed into the stream buffer
// and enter must be pressed before the characters are processed now, we intend toset it to 'raw mode' 
// to do this, we use the tcgetattr() function to read the current attributes into a struct and write the terminal attributes back out.
void enableRawMode(void){
    if (tcgetattr(STDIN_FILENO, &EdiCfg.orig_termios) == -1) die("tcsetattr");
    struct termios raw = EdiCfg.orig_termios;
    
    // atexit() function is automatically called whenever the program exits
    atexit(disbleRawMode);

    // the ICANON flag (2 in decimal or 0b 0000 0010) allows us to turn off the canonical flag and 
    // read input from STDIN byte by byte the ECHO flag is (decimal 8, or byte 0b 0000 1000)
    // when we bit OR the ECHO and ICANON flags, the resulting binary is 
    // a number with both the 2nd and 4th bit set to 1. --> 0b 0000 1010
    // the '~' sign would allow us to switch off the fourth bit and second bit,
    // while keeping the other bits on. Bit AND with any number would preserve the bit value i the 
    // other operand except for the 4th and 2nd digit Bit AND works like ===> 1111 & 1011 == 1011, 
    // 1011001 & 1111111 ===> 1011001 and then we can use that to mask the variables in the c_lfag variable

    // ICRNL flag controls the 'cntrl + m' key presses
    // IXON flag disables the 'cntrl + S' and 'cntrl +'
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL| IXON); // IFLAGS are input flags

    // VMIN and VTIME are terminal flags that come from termios library.
    // they are indexes into the c_cc[] array which are called control characters
    // which are various bits for setting terminal controls -
    // an array of bytes that control various terminal settings
    // The VMIN value sets the minimum number of bytes of input needed before read() can return
    //
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10;

    // the terminal translates every newline character we type '\n' into  '\r\n'
    // hence we use the 'OPOST' flag to switch that off
    /*The VMIN value sets the minimum number of bytes of input needed before read() can return. 
      We set it to 0 so that read() returns as soon as there is any input to be read. 
      The VTIME value sets the maximum amount of time to wait before read() returns. 
      It is in TENTHS of a SECOND, so we set it to 1/10 of a second, or 100 milliseconds. 
      If read() times out, it will return 0, which makes sense because its usual return value is 
      the number of bytes read.
    */
    raw.c_oflag &= ~(OPOST); // OFLAGS are output flags
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // LFLAGS are local flags
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey(void){
    int nread;
    char c;
    /*  In Cygwin, when read() times out it returns -1 with an errno of EAGAIN, 
     *  instead of just returning 0 like it’s supposed to. To make it work in Cygwin,
     *  we won’t treat EAGAIN as an error.
     *  this function keeps reading from the STDIN file i.e. standard input or keyboard
     *  until it can no longer get any characters
     */
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){ // polls the STDIN device untill a character is found
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    if(c == '\x1b'){
        char charSequence[3];
        if(read(STDIN_FILENO, &charSequence[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &charSequence[1], 1) != 1) return '\x1b';

        if(charSequence[0] == '['){
            switch (charSequence[1]){
                case 'A': return 'w';
                case 'B': return 's';
                case 'C': return 'd';
                case 'D': return 'a';
            }
        }
        return '\x1b';
    }else{
        return c;
    }
    return c;
}

/******************************************************** APPEND BUFFER********************************************/
// This module helps us to avoid multiple calls to the "write" function
#define INIT_BUFF {NULL, 0}
typedef struct appendBuf {
  char *buferPtr;
  int64_t len;
}appendBuf;

void bufferAppendFunc(appendBuf* objPtr, const char*s, int64_t len){
    char* new = realloc(objPtr->buferPtr, objPtr->len + len);

    if(new == NULL) die("realloc");
    
    memcpy(&new[objPtr->len], s, len);
    objPtr->buferPtr = new;
    objPtr->len += len;
}

void bufferReleaseFunc(appendBuf* bufPtr){
    free(bufPtr->buferPtr);
}

/******************************************************** OUTPUT **************************************************/
#define KILO_VERSION "0.0.1"
void editorDrawRows(appendBuf* ab){
    int64_t y;
    int64_t third_of_screen = EdiCfg.screenRows / 3; 
    for(y = 0; y < EdiCfg.screenRows; ++y){
        if(y == third_of_screen){
            char welcomeMessg[80];
            int welcomeLen = snprintf(welcomeMessg, sizeof(welcomeMessg), "Kilo editor -- version %s", KILO_VERSION);
            if (welcomeLen > EdiCfg.screenColumns) welcomeLen = EdiCfg.screenColumns;
            int padding = (EdiCfg.screenColumns - welcomeLen) / 2;
            if (padding) {
                bufferAppendFunc(ab, "~", 1);
                --padding;
            }
            while (--padding) bufferAppendFunc(ab, " ", 1);
            bufferAppendFunc(ab, welcomeMessg, welcomeLen);
        }else{
            bufferAppendFunc(ab, "~", 1);
        }
        bufferAppendFunc(ab, "\x1b[K", 3);
        if (y < EdiCfg.screenRows - 1) {
            bufferAppendFunc(ab, "\r\n", 2);
        }
    }//
}

void editorClearScreen(void){
    appendBuf buf = INIT_BUFF;

    // The h and l modes help us tun hide and show the cursor 
    bufferAppendFunc(&buf, "\x1b[?25l]", 6);

    // The 4 in our write() call means we are writing 4 bytes out to the terminal. 
    // The first byte is \x1b, which is the escape character, or 27 in decimal.
    // folowed by the '[' character. This is known as the ESCAPE SEQUENCE.
    /* Escape sequences instruct the terminal to do various text formatting tasks, 
       such as coloring text, moving the cursor around, and clearing parts of the screen. */
    // escape sequence commands take 2 arguments: an number (N) and a letter (J, H, e.t.c)
    // in the format: \x1bNJ where 'N' is a number. J is the command and J clears the screen.
    // 0 is the default number for the 'J' command, and it clears the screen from the cursor to
    // the end. Both STD_OUT and write(...) come from <unistd.h> library header file
    // bufferAppendFunc(&buf, "\x1b[2J", 4);
    
    // the 'H' - command actually helps to position the cursor. The H command actually takes two arguments:
    // the row number and the column number at which to position the cursor. Multiple arguments are separated 
    // by a ; character. For example "\x1b[12;24H"
    bufferAppendFunc(&buf, "\x1b[H", 3);

    editorDrawRows(&buf);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", EdiCfg.cur_y + 1, EdiCfg.cur_x + 1);
    bufferAppendFunc(&buf, buffer, strlen(buffer));

    bufferAppendFunc(&buf, "\x1b[?25h", 6);
    write(STDOUT_FILENO, buf.buferPtr, buf.len);
    bufferReleaseFunc(&buf);
}

/********************************************** INPUT ***************************************************/
void moveCursor(char key){
    switch (key)
    {
    case 'a':
        --EdiCfg.cur_x;
        break;
    case 'd':
        ++EdiCfg.cur_x;
        break;
    case 'w':
        --EdiCfg.cur_y;
        break;
    case 's':
        ++EdiCfg.cur_y;
        break;
    default:
        break;
    };
}

char editorKeyPressProcess(void){
    char c = editorReadKey();
    switch (c){
        case CNTRL_KEY('q'):
            // clear screen on quit program
            // write(STDOUT_FILENO, "\x1b[2J", 4);
            // write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case 'a':
        case 'd':
        case 'w':
        case 's':
            moveCursor(c);
            break;
    };
    return c;
}

// the n command can be used to query the terminal for device status information
// we want to give the number 6 for cursor position and read the output from standard input
int getCurrentCursorPosition(int* rows, int* cols){
    int ret;
    char buf[32];
    unsigned int i = 0;

    // writing to stdout file is basically writing to the screen an output value of values
    // writing the escape sequence with the 'n' comand sends a query to the terminal to
    // return the cursor's position on the screen via the argument '6' returns the cursor position
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4){ 
        ret = -1;
    }else{
        ret =-1;
        for(i = 0; i < sizeof(buf) - 1; i++){
            // this reads the new position of the cursor on the screen
            // the values are read into a buffer the size of the 3rd argument 
            if(read(STDIN_FILENO, &buf[i], sizeof(char)) != 1) break;
            if iscntrl(buf[i]){
                printf("%d\r\n", buf[i]);
            }else{
                printf("%d (%c)\r\n", buf[i], buf[i]);
            }
            if(buf[i] == 'R') break;
        }
        buf[i] = '\0';
        ret = 0;

        // we want to avoid printing out the first character on the 
        // stdout file because the first character is an escape sequence which 
        // the terminal will detect
        printf("\r\n&buf[1]: '%s'\r\n", &buf[1]); 
        if(buf[0] != '\x1b' || buf[1] != '[') ret = -1;
        if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) ret = -1; // takes input from a variable and parses it into a variable(s) of specified formatting
    }
    editorReadKey();
    return ret;
}

/********************************************** INITIALIZATION ****************************************/
/*
 *  On most systems, you should be able to get the size of the terminal by 
 *  simply calling ioctl() with the TIOCGWINSZ request. (As far as I can tell, 
 *  it stands for Terminal IOCtl (which itself stands for Input/Output Control) 
 *  Get Window SiZe.
 */

/*  As a fail safe, we can get the window size by moving the cursor 
 *  to the bottom of the screen and using the escape sequence route to 
 *  query the device cursor's position
 */
int getWinSize(int* rows, int* cols){
    struct winsize ws;
    int ret;
    if(1 || (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) || (ws.ws_col == 0)){
        //this condition writes the position of the cursor to the bottom right end of the screen
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){// fall back procedure
            ret = -1;
        }else{
            ret = getCurrentCursorPosition(rows, cols);
        }
    }else{
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        ret = 0;
    }
    return ret;
}

//initTheEditor()'s role is to provide the dimensions of the screen 
void initTheEditor(void){
    EdiCfg.cur_x = 0;
    EdiCfg.cur_y = 0;
    if(getWinSize(&EdiCfg.screenRows, &EdiCfg.screenColumns) == -1) die("getWinSize");
}

int main(){
    enableRawMode();
    initTheEditor();
    while (1){
        char c = '\0';  
        editorClearScreen();
        c = editorKeyPressProcess(); 
        // if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
        //     die("read");
        if(iscntrl(c)){
            printf("%d\r\n", c); // carriage return added to out printf statements rather than allowing the terminal implicitly implement it
        }else{
            printf("%d ('%c')\r\n", c, c);
        }
    }
    return 0;
}

