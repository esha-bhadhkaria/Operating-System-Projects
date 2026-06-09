//wcl - count the total number of lines in files
#include "app.h"
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        INFO("usage: wcl [FILE1] [FILE2] ...");
        return -1;
    }

    uint total_lines = 0;

    //Process each file
    for (int f = 1; f < argc; f++) {
        char* filename = argv[f];

        /* Get the inode number of the file. */
        int file_ino = dir_lookup(workdir_ino, filename);
        if (file_ino < 0) {
            INFO("wcl: file %s not found", filename);
            continue;
        }

        //Read the entire file and count lines
        char block[BLOCK_SIZE];
        uint line_count = 0;
        
        for (uint offset = 0; ; offset += BLOCK_SIZE) {
            if (file_read(file_ino, offset, block) < 0) break;
            
            //Count newlines in the block
            for (int i = 0; i < BLOCK_SIZE; i++) {
                if (block[i] == '\0') break;
                if (block[i] == '\n') line_count++;
            }
        }

        total_lines += line_count;
    }

    printf("%d\n\r", total_lines);
    return 0;
}