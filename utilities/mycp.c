#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#define COUNT 500

int main(int argc, char *argv[])
{
    char buf[COUNT];

    if (argc != 3)
    {
        printf("Invalid argument count");
        exit(-1);
    }

    int srcFile = open(argv[1], O_RDONLY);
    if (srcFile < 0)
    {
        printf("could not open the source file\n");
        exit(-2);
    }

    int destFile = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destFile < 0)
    {
        printf("could not open the destination file\n");
        exit(-3);
    }

    int num_read;
    while ((num_read = read(srcFile, buf, COUNT)) > 0)
    {
        if (write(destFile, buf, num_read) < 0)
        {
            printf("could not write the destination file\n");
            exit(-3);
        }
    }
    close(srcFile);
    close(destFile);
    return 0;
}
