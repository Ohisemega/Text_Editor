// #include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// global variable to store the original state of the terminal before we edit it's 
// variable values. This allows us to restore the terminal 
// state to it's original value at program inception
struct termios org_termios;

//
void disbleRawMode(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

//this function turns off terminal echo. 
//On default the terminal is in 'cooked mode' or 'canonical mode'
//meaning that characters are passed into the stream buffer
//and enter must be pressed before the characters are processed
//now, we intend toset it to 'raw mode' 
//to do this, we use the tcgetattr() function to read the current 
//attributes into a struct and write the terminal attributes back out.
void enableRawMode(){
    tcgetattr(STDIN_FILENO, &org_termios);
    struct termios raw = org_termios;
    atexit(disbleRawMode);

    // the ICANON flag (2 in decimal or 0b 0000 0010) allows us to turn off the canonical flag and 
    // read input from STDIN byte by byte
    // the ECHO flag is (decimal 8, or byte 0b 0000 1000)
    // when we bit OR the ECHO and ICANON flags, the resulting binary is 
    // a number with both the 2nd and 4th bit set to 1. --> 0b 0000 1010
    // the '~' sign would allow us to switch off the fourth bit and second bit,
    // while keeping the other bits on. Bit AND with any number would preserve the bit value i the 
    // other operand except for the 4th and 2nd digit
    // Bit AND works like ===> 1111 & 1011 == 1011, 1011001 & 1111111 ===> 1011001
    // and then we can use that to mask the variables in the c_lfag variable
    raw.c_lflag &= ~(ECHO | ICANON); 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(){
    char c;
    enableRawMode();
    while (read(STDIN_FILENO, &c, 1) ==1 && c != 'q');
    return 0;
}