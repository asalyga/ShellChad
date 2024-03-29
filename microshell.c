#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"

#define MICROSHELL_ERROR(format, ...) {\
    fflush(stdout);\
    fprintf(stderr, KRED "-microshell: " KNRM);\
    fprintf(stderr, format, ##__VA_ARGS__);\
    fprintf(stderr, "\n");\
}\

#define CHECK_ALLOCATION(buff) {\
    if(!buff) {\
        MICROSHELL_ERROR("allocation error")\
        exit(1);\
}\
}\

typedef struct CONTEXT  CONTEXT;
typedef struct PROCESS  PROCESS;

struct PROCESS
{
    pid_t           pid;
    int             status;
    int             completed;
    int             stopped;
    int             argc;
    const char**    argv;
    char*           path;
};

struct CONTEXT
{
    const char*     home_dir;
    char*           cwd;
    char*           prev_cwd;
    char*           paths;
    size_t          num_paths;
    pid_t           shell_pid;
    pid_t           shell_pgid;
    char*           history;
    size_t          history_index;
    size_t          max_history_len;
    char*           argv_buff;
    int             argc;
    char**          argv;
};

typedef int(*COMMAND_HANDLER)(int, const char**);

char*   copy_string(const char* string);
char*   expand_path(const char* path);
void    swap_ptr(char** a, char** b);
void    list_directory_files(const char* path);
int     is_directory(const char *path);
int     parse();
int     path_exists(const char* path);
int     process_shell_command();
void    extract_paths_from_PATH();
void    set_home_dir();
void    wait_for_process(PROCESS* process);
char*   path_concat(const char* a, const char* b);
void    launch_foreground_process(PROCESS* process);

//              Komendy shell-a
int     microshell_cd(int argc, const char** argv);
int     microshell_exit(int argc, const char** argv);
int     microshell_help(int argc, const char** argv);
int     microshell_ls(int argc, const char** argv);
int     microshell_mkdir(int argc, const char** argv);
int     microshell_history(int argc, const char** argv);



void* command_handlers[] = 
{
        // format: Nazwa komendy, funkcja komendy
        "cd",       microshell_cd,
        "exit",     microshell_exit,
        "help",     microshell_help,
        "ls",       microshell_ls,
        "mkdir",    microshell_mkdir,
        "history",  microshell_history
};

CONTEXT* context()
{
    static CONTEXT context;
    return &context;
}

void wait_for_process(PROCESS* process)
{
    int     status;
    pid_t   pid;
    do {
        pid = waitpid(WAIT_ANY, &status, WUNTRACED);
        process->status = status;
        if(WIFSTOPPED(status))
            process->stopped = 1;
        else
            process->completed = 1;
    } while (process->completed == 0 && process->stopped == 0);
}

void launch_foreground_process(PROCESS* process)
{
    switch ((process->pid = fork())) {
        case 0:
            if (setpgid(0, 0) < 0 && tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
                perror("");
                exit(1);
            }
            signal(SIGINT,  SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            execvp(process->path, (char * const *)process->argv);
            perror("execvp");
            exit(1);
        case -1:
            exit(1);
        default:    
            setpgid(process->pid, process->pid);
            tcsetpgrp(STDIN_FILENO, process->pid);
            wait_for_process(process);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            free(process->path);
            break;
    }
}
char* path_concat(const char* a, const char* b)
{


    size_t n = strlen(a);
    size_t m = strlen(b);


    if ((!n || a[n-1] != '/') && (!m || b[0] != '/')) {

        char* buff = malloc(n + m + 2);
        CHECK_ALLOCATION(buff)
        strcpy(buff, a);
        buff[n] = '/';
        strcpy(&buff[n+1], b);
        return buff;
    } else {

        char* buff = malloc(n + m + 1);
        CHECK_ALLOCATION(buff)
        strcpy(buff, a);
        strcpy(&buff[n], b);
        return buff;
    }
}

char* copy_string(const char* string)
{
    char* buff = malloc(strlen(string) + 1);
    CHECK_ALLOCATION(buff)
    strcpy(buff, string);
    return buff;
}


char* expand_path(const char* path)
{
    if (strlen(path) == 1 && *path == '~') {
        return copy_string(context()->home_dir);
    }

    if (strlen(path) > 1 && *path == '~') {
        return path_concat(context()->home_dir, path + 1);
    }

    return copy_string(path);
}
void swap_ptr(char** a, char** b)
{
    char* tmp = (*a);
    (*a) = (*b);
    (*b) = (tmp);
}

int microshell_cd(int argc, const char** argv)
{
    if (!argc)
        return 0;

    char* path = expand_path(argv[0]);


    if (strcmp(path, "-") == 0) {
        free(path);
        if (chdir(context()->prev_cwd) < 0) {
            MICROSHELL_ERROR("FATAL ERROR: cd: %s", strerror(errno))
            exit(1);
        }
        swap_ptr(&context()->prev_cwd, &context()->cwd);
        return 0;
    }

    if (chdir(path) < 0) {
        MICROSHELL_ERROR("cd: %s", strerror(errno))
        free(path);
        return 1;
    }
    free(path);
    free(context()->prev_cwd);
    context()->prev_cwd = context()->cwd;
    context()->cwd = getcwd(NULL, 0);
    CHECK_ALLOCATION(context()->cwd)
    return 0;
}

int microshell_exit(int argc, const char** argv)
{
    free(context()->cwd);
    free(context()->prev_cwd);
    free(context()->paths);
    free(context()->history);
    free(context()->argv_buff);
    free(context()->argv);
    exit(0);
}

int microshell_help(int argc, const char** argv)
{
    const char banner[] = "\n"
                          "\n"
                          "AUTHOR: ALeksander Salyga"
                          "\n"
                          "\n"
                          KGRN "commands: \n" KNRM
                          "  1. cd\n"
                          "  2. help\n"
                          "  3. exit\n"
                          "  4. ls\n"
                          "  5. history\n"
                          "  6. mkdir\n"
                          "\n\n"
                          KGRN "others: \n" KNRM
                          "  1. CTRL Z handling\n"
                          "  2. ~ expands to home directory\n"
                          "  3. basic quote handling\n";

    fprintf(stdout, "%s", banner);
    return 0;
}


int microshell_history(int argc, const char** argv)
{
    fprintf(stdout, "%4d  ", 1);
    for (int i = 0, j = 1; i < context()->history_index; ++i) {
        if (context()->history[i] == '\0' && i+1 < context()->history_index) {
            fprintf(stdout, "%4d  ", ++j);
        } else {
            fprintf(stdout, "%c", context()->history[i]);
        }
    }
    return 0;
}

void list_directory_files(const char* path)
{
    DIR* dir = opendir(path);
    if (!dir) {
        MICROSHELL_ERROR("list files: %s", strerror(errno))
        exit(1);
    }
    struct dirent *entry;
    while((entry = readdir(dir))) {
        char* full_path = path_concat(path, entry->d_name);
        if (is_directory(full_path))
            fprintf(stdout, KGRN "    %s\n" KNRM, entry->d_name);
        else
            fprintf(stdout, "    %s\n", entry->d_name);
        free(full_path);
    }
    closedir(dir);
}


int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

int microshell_ls(int argc, const char** argv) {
   
    if (argc == 0) {
        list_directory_files(&context()->cwd[0]);
        return 0;
    }

    for (int i = 0; i < argc; ++i) {
        char* path = expand_path(argv[i]);

        if (!path_exists(path)) {
            MICROSHELL_ERROR("ls: %s: %s", path, strerror(errno))
            free(path);
            continue;
        }

        if (is_directory(path)) {
            fprintf(stdout, "\n%s: \n", path);
            list_directory_files(path);
        } else {
            fprintf(stdout, "\n%s\n", path);
        }
        free(path);
    }
    return 0;
}
