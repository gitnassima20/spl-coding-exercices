#include <stdio.h>

int main(int argc, char *argv[]) {

    if(argc != 3) {
        printf("Invalid argument count");
        exit(-1);
    }

    if (rename(argv[1], argv[2]) == 0) {
        exit(0);
    } else {
        printf("Could not move file\n");
        exit(-1);
    }
}
