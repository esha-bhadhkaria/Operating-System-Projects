//grep: search for pattern in file
#include "app.h"
#include <string.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        INFO("usage: grep [PATTERN] [FILE]");
        return -1;
    }

    char* pattern = argv[1];
    char* filename = argv[2];

    /*Get the inode number of the file. */
    int file_ino = dir_lookup(workdir_ino, filename);
    if (file_ino < 0) {
        INFO("grep: file %s not found", filename);
        return -1;
    }

    //Read the entire file and process it block by block
    char block[BLOCK_SIZE];
    char line[BLOCK_SIZE];
    int index = 0;
    
    for (uint offset = 0; ; offset += BLOCK_SIZE) {
        if (file_read(file_ino, offset, block) < 0) break;     //read till EOF
        
        //Process each character in the block
        for (int i = 0; i < BLOCK_SIZE; i++) {
            if (block[i] == '\0') break;
            
            if (block[i] == '\n') {        //newline found - means we've completed reading a line
                line[index] = '\0';
                if (strstr(line, pattern) != NULL) {    //Check if pattern found in the line
                    printf("%s\n\r", line);
                }
                index = 0;            //reset for next line       
            } else {
                line[index++] = block[i];
                if (index >= BLOCK_SIZE - 1) index = BLOCK_SIZE - 1;       //truncate line if too long
            }
        }
    }
    
    //Handle last line if it doesn't end with newline
    if (index > 0) {
        line[index] = '\0';
        if (strstr(line, pattern) != NULL) {
            printf("%s\n\r", line);
        }
    }

    return 0;
}
