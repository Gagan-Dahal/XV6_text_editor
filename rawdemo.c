#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(void)
{
  setconsole(1); // enable raw mode

  char c;
  while(read(0, &c, 1) == 1){
    if(c == 24) // Ctrl-X to exit
      break;
    else if(c == 8 || c == 127){ // backspace
      write(1, "\b \b", 3);
    } else {
      write(1, &c, 1);
    }
  }

  setconsole(0); // restore cooked mode
  exit();
}
