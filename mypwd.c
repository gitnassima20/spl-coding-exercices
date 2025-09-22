#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
int main()
{
        char buffer[1024];
        char *cwd = getcwd(buffer, 1024);
        if (cwd != NULL)
        {
                printf("%s", cwd);
        }
        else
        {
                exit(-1);
        }
}
