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
