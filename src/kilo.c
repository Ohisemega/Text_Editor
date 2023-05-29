#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// global variable to store the original state of the terminal before we edit it's 
// variable values. This allows us to restore the terminal 
// state to it's original value at program inception
struct termios orig_termios;

void die(const char* s){
    perror(s); // comes from the stdio.h library
    exit(1); // comes from the stdlib.h library
}

// calls the tcseattr() function to set the values of the terminal to the values
// of the default terminal before exit
void disbleRawMode(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

//this function turns off terminal echo. 
//On default the terminal is in 'cooked mode' or 'canonical mode'
//meaning that characters are passed into the stream buffer
//and enter must be pressed before the characters are processed
//now, we intend toset it to 'raw mode' 
//to do this, we use the tcgetattr() function to read the current 
//attributes into a struct and write the terminal attributes back out.
void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcsetattr");
    struct termios raw = orig_termios;
    
    // atexit() function is called whenever the program exits
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
    // which are various bits for setting terminal control
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
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

// the die function prints an error message and exits the function
/*Most C library functions that fail will set the global errno variable 
    to indicate what the error was. perror() looks at the global errno variable and 
    prints a descriptive error message for it. It also prints the string given to it 
    before it prints the error message, which is meant to provide context about what 
    part of your code caused the error.
*/

int main(){
    enableRawMode();
    while (1){
        char c = '\0';
        read(STDIN_FILENO, &c, 1);
        if(iscntrl(c)){
            printf("%d\r\n", c);//carriage return added to out printf statements rather than allowing the terminal implicitly implement it
        }else{
            printf("%d ('%c')\r\n", c, c);
        }
        if(c == 'q') break;
    }
    return 0;
}

