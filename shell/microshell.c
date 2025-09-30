#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAX_INPUT 16384
#define MAX_PATH 8192

/* Structure for managing local variables */
typedef struct
{
    char **vars;
    int count;
} VarList;

/* Initialize variable list */
void init_vars(VarList *vlist)
{
    vlist->vars = malloc(sizeof(char *));
    vlist->vars[0] = NULL;
    vlist->count = 0;
}

/* Find variable in list */
char *find_var(VarList *vlist, const char *name)
{
    for (int i = 0; vlist->vars[i] != NULL; i++)
    {
        char *eq = strchr(vlist->vars[i], '=');
        if (eq != NULL)
        {
            size_t name_len = eq - vlist->vars[i];
            if (strncmp(vlist->vars[i], name, name_len) == 0 && name[name_len] == '\0')
            {
                return eq + 1;
            }
        }
    }
    return NULL;
}

/* Set or update a variable */
void set_var(VarList *vlist, const char *assignment)
{
    char *eq = strchr(assignment, '=');
    if (!eq)
        return;

    size_t name_len = eq - assignment;

    /* Check if variable already exists */
    for (int i = 0; vlist->vars[i] != NULL; i++)
    {
        if (strncmp(vlist->vars[i], assignment, name_len) == 0 &&
            vlist->vars[i][name_len] == '=')
        {
            free(vlist->vars[i]);
            vlist->vars[i] = strdup(assignment);
            return;
        }
    }

    /* Add new variable */
    vlist->vars = realloc(vlist->vars, sizeof(char *) * (vlist->count + 2));
    vlist->vars[vlist->count] = strdup(assignment);
    vlist->vars[vlist->count + 1] = NULL;
    vlist->count++;
}

/* Expand $variables in input string */
void expand_variables(char *input, VarList *vlist)
{
    char *dollar = strchr(input, '$');

    while (dollar != NULL)
    {
        char *var_start = dollar + 1;
        char *var_end = var_start;

        /* Find end of variable name */
        while (*var_end && *var_end != ' ' && *var_end != '\0')
        {
            var_end++;
        }

        char var_name[256];
        size_t var_len = var_end - var_start;
        strncpy(var_name, var_start, var_len);
        var_name[var_len] = '\0';

        /* Look up variable value */
        char *value = find_var(vlist, var_name);

        if (value != NULL)
        {
            /* Replace $var with its value */
            char temp[MAX_INPUT];
            strcpy(temp, var_end);
            strcpy(dollar, value);
            strcat(dollar, temp);
        }
        else
        {
            /* Remove undefined variable */
            memmove(dollar, var_end, strlen(var_end) + 1);
        }

        dollar = strchr(dollar + (value ? strlen(value) : 0), '$');
    }
}

/* Parse input into argv array */
int parse_args(char *input, char ***argv_out)
{
    int capacity = 64;
    char **argv = malloc(sizeof(char *) * capacity);
    int argc = 0;

    char *token = strtok(input, " \t");
    while (token != NULL)
    {
        if (argc >= capacity - 1)
        {
            capacity *= 2;
            argv = realloc(argv, sizeof(char *) * capacity);
        }
        argv[argc++] = strdup(token);
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;

    *argv_out = argv;
    return argc;
}

/* Free argv array */
void free_args(char **argv, int argc)
{
    for (int i = 0; i < argc; i++)
    {
        free(argv[i]);
    }
    free(argv);
}

/* Handle I/O redirection and remove redirect tokens from argv */
bool handle_redirects(char **argv, int *argc)
{
    int i = 0;
    int j = 0;

    while (i < *argc)
    {
        if (strcmp(argv[i], "<") == 0 && i + 1 < *argc)
        {
            int fd = open(argv[i + 1], O_RDONLY);
            if (fd < 0)
            {
                fprintf(stderr, "cannot access %s: No such file or directory\n", argv[i + 1]);
                return false;
            }
            if (dup2(fd, STDIN_FILENO) < 0)
            {
                close(fd);
                return false;
            }
            close(fd);
            i += 2;
        }
        else if (strcmp(argv[i], ">") == 0 && i + 1 < *argc)
        {
            int fd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                fprintf(stderr, "%s: Permission denied\n", argv[i + 1]);
                return false;
            }
            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                close(fd);
                return false;
            }
            close(fd);
            i += 2;
        }
        else if (strcmp(argv[i], "2>") == 0 && i + 1 < *argc)
        {
            int fd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                fprintf(stderr, "%s: Permission denied\n", argv[i + 1]);
                return false;
            }
            if (dup2(fd, STDERR_FILENO) < 0)
            {
                close(fd);
                return false;
            }
            close(fd);
            i += 2;
        }
        else
        {
            argv[j++] = argv[i++];
        }
    }

    argv[j] = NULL;
    *argc = j;
    return true;
}

/* Check if command is a variable assignment */
bool is_assignment(const char *cmd)
{
    char *eq = strchr(cmd, '=');
    if (!eq)
        return false;

    /* Check for spaces around '=' */
    if (*(eq + 1) == ' ' || *(eq - 1) == ' ')
        return false;

    return true;
}

/* Built-in: echo */
int builtin_echo(char **argv, int argc)
{
    for (int i = 1; i < argc; i++)
    {
        printf("%s", argv[i]);
        if (i < argc - 1)
            printf(" ");
    }
    printf("\n");
    return 0;
}

/* Built-in: pwd */
int builtin_pwd(int argc)
{
    if (argc > 1)
    {
        printf("Usage: 'pwd' command doesn't have arguments. Arguments are ignored.\n");
    }

    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        printf("Error: Failed to get the pathname (pathname is too long)\n");
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}

/* Built-in: cd */
int builtin_cd(char **argv, int argc)
{
    if (argc != 2)
    {
        printf("Usage: cd [new_directory_path]\n");
        return 1;
    }

    if (chdir(argv[1]) < 0)
    {
        printf("cd: %s: No such file or directory\n", argv[1]);
        return 1;
    }

    return 0;
}

/* Built-in: export */
int builtin_export(char **argv, int argc, VarList *vlist)
{
    if (argc != 2)
    {
        printf("Usage: export 'key=value' or 'key' (but key has to be defined)\n");
        return 1;
    }

    char *var_str;

    if (strchr(argv[1], '=') == NULL)
    {
        /* Export existing variable */
        char *value = find_var(vlist, argv[1]);
        if (!value)
        {
            printf("export: %s is not defined, please define it first.\n", argv[1]);
            return -1;
        }

        /* Build var=value string */
        var_str = malloc(strlen(argv[1]) + strlen(value) + 2);
        sprintf(var_str, "%s=%s", argv[1], value);
    }
    else
    {
        /* Export new variable */
        var_str = strdup(argv[1]);
    }

    if (putenv(var_str) != 0)
    {
        printf("export: couldn't add the variable to environ because there isn't enough memory space\n");
        free(var_str);
        return -1;
    }

    return 0;
}

/* Execute external command */
int execute_command(char **argv)
{
    pid_t pid = fork();

    if (pid > 0)
    {
        /* Parent process */
        int status;
        wait(&status);
        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        return 1;
    }
    else if (pid == 0)
    {
        /* Child process */
        execvp(argv[0], argv);
        printf("%s: command not found\n", argv[0]);
        exit(-2);
    }
    else
    {
        printf("UNIX CMD: Failed to fork\n");
        return -1;
    }
}

/* Main shell loop */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int exit_code = 0;
    char input[MAX_INPUT];
    VarList vlist;

    /* Save original file descriptors */
    int stdin_backup = dup(STDIN_FILENO);
    int stdout_backup = dup(STDOUT_FILENO);
    int stderr_backup = dup(STDERR_FILENO);

    init_vars(&vlist);

    while (1)
    {
        /* Restore original file descriptors before each command */
        dup2(stdin_backup, STDIN_FILENO);
        dup2(stdout_backup, STDOUT_FILENO);
        dup2(stderr_backup, STDERR_FILENO);

        printf("Usage: microshell <command> [args...] [< input] [> output] [2> error]\n");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }

        /* Remove trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n')
        {
            input[len - 1] = '\0';
        }

        if (strlen(input) == 0)
            continue;

        /* Expand variables */
        expand_variables(input, &vlist);

        /* Parse into arguments */
        char **cmd_argv;
        int cmd_argc = parse_args(input, &cmd_argv);

        if (cmd_argc == 0)
        {
            free_args(cmd_argv, cmd_argc);
            continue;
        }

        /* Handle I/O redirection (modifies cmd_argv and cmd_argc) */
        if (!handle_redirects(cmd_argv, &cmd_argc))
        {
            free_args(cmd_argv, cmd_argc);
            exit_code = -1;
            continue;
        }

        if (cmd_argc == 0)
        {
            free_args(cmd_argv, cmd_argc);
            continue;
        }

        /* Check for variable assignment */
        if (is_assignment(cmd_argv[0]))
        {
            char *eq = strchr(cmd_argv[0], '=');
            if (eq && *(eq + 1) != ' ' && *(eq - 1) != ' ')
            {
                set_var(&vlist, cmd_argv[0]);
                exit_code = 0;

                if (cmd_argc == 1)
                {
                    free_args(cmd_argv, cmd_argc);
                    continue;
                }
            }
            else
            {
                printf("Error: The assignment have to be in this literal format: key=value\n");
                exit_code = -1;
                free_args(cmd_argv, cmd_argc);
                continue;
            }
        }

        /* Execute built-in or external command */
        if (strcmp(cmd_argv[0], "exit") == 0)
        {
            if (cmd_argc > 1)
            {
                printf("Usage: 'exit' command doesn't have arguments. Arguments are ignored.\n");
            }
            printf("Good Bye\n");
            free_args(cmd_argv, cmd_argc);
            break;
        }
        else if (strcmp(cmd_argv[0], "echo") == 0)
        {
            exit_code = builtin_echo(cmd_argv, cmd_argc);
        }
        else if (strcmp(cmd_argv[0], "pwd") == 0)
        {
            exit_code = builtin_pwd(cmd_argc);
        }
        else if (strcmp(cmd_argv[0], "cd") == 0)
        {
            exit_code = builtin_cd(cmd_argv, cmd_argc);
        }
        else if (strcmp(cmd_argv[0], "export") == 0)
        {
            exit_code = builtin_export(cmd_argv, cmd_argc, &vlist);
        }
        else
        {
            exit_code = execute_command(cmd_argv);
        }

        free_args(cmd_argv, cmd_argc);
    }

    /* Cleanup */
    for (int i = 0; vlist.vars[i] != NULL; i++)
    {
        free(vlist.vars[i]);
    }
    free(vlist.vars);

    close(stdin_backup);
    close(stdout_backup);
    close(stderr_backup);

    return exit_code;
}