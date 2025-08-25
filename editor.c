#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"


/*TO-DO
-Bind ctrl+o as save and ctrl+x as exit

*/

#define MAX_lines 512
#define MAX_COLS 128

char buffer[MAX_lines][MAX_COLS];
int num_lines = 0;

void load_file(const char* filename) {
    int file_descriptor = open(filename, O_RDONLY);
    if (file_descriptor<0){
        printf(1, "File not found\n");
        num_lines = 1;
        buffer[0][0] = '\0';
        return;
    }
    char c;
    int row = 0, column = 0;
    while (read(file_descriptor, &c, 1)==1){
        if(c == '\r') continue;
        if(c == '\n'){
            buffer[row][column] = '\0';
            column = 0;
            row++;
        }
        else{
            if(column>=MAX_COLS-1){
                buffer[row][column] = '\0';
                row++;
                column = 0;
            }
            buffer[row][column++] = c;
        }
        
    }
    buffer[row][column] = '\0';

    num_lines = row+1;
    close(file_descriptor);
}

void display_buffer(){
    clear();
    for(int i = 0; i<num_lines; i++){
        printf(1, "%s\n", buffer[i]);
    }
    return;
}
void save_file(const char* filename, int line_start, int line_end){
    int file_descriptor = open(filename, O_CREATE | O_APPEND);
    if(file_descriptor < 0){
        printf(1, "Error: could not open %s for writing\n", filename);
        return;
    }
    for (int i = line_start; i<=line_end; i++){
        write(file_descriptor, buffer[i], strlen(buffer[i]));
        write(file_descriptor, "\n", 1);
    }
    close(file_descriptor);

}

void write_file(const char* filename){
    char entered;
    int j = 0;
    int line_start = num_lines;
    while(1){
        int n = read(0, &entered, 1);
        if(n < 1) continue;
        if (entered == 15){  //ctrl+o
            buffer[num_lines][j] = '\0';
            save_file(filename, line_start, num_lines);
            continue;
        }
        if(entered == 24){ //ctrl+x
            exit();
        }
        if(entered == '\n'){
            buffer[num_lines][j] = '\0';
            num_lines++;
            j = 0;
            continue;
        }
        buffer[num_lines][j++] = entered;
        if(j>=MAX_COLS-1){
            buffer[num_lines][j] = '\0';
            num_lines++;
            j = 0;
        }
    }
}

int main(int argc, char*argv[]){
    if(argc != 2){
        printf(1, "The correct syntax is: editor <filename>\n");
        exit();
    }

    char* filename = argv[1];
    printf(1, "%s\n", filename);
    load_file(filename);
    display_buffer();

    char c;
    read(0, &c, 1);

    write_file(filename);

    read(0, &c, 1);

    exit();
}