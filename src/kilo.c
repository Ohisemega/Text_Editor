#include <stdio.h>
#include <termios.h>
#include <unistd.h>

//this function turns off terminal echo. 
//On default the terminal is in 'cooked mode' or 'canonical mode'
//meaning that characters are passed into the stream buffer
//and enter must be pressed before the characters are processed
//now, we intend toset it to 'raw mode' 
//to do this, we use the tcgetattr() function to read the current 
//attributes into a struct and write the terminal attributes back out.
void enableRawMode(){
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(){
    char c;
    enableRawMode();
    while (read(STDIN_FILENO, &c, 1) ==1 && c != 'q');
    return 0;
}