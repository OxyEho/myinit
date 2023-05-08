#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <stdarg.h>

#define MAXPROC 10

struct task_info {
    int args_count;
    char **args;
    char* in;
    char* out;
};

pid_t pids[MAXPROC];
int pids_count;
struct task_info tasks[MAXPROC];
FILE* log_file;
char conf_name[4096];

int append(char** arr, char* str, int last) {
    arr[last] = malloc(strlen(str));
    strcpy(arr[last], str);
    return last+1;
}

void write_log(char* format, ...) {
    va_list va;
    va_start(va, format);
    int log_len = vsnprintf(NULL, 0, format, va);
    va_end(va);
    char* log_line = malloc(log_len + 1);
    va_start(va, format);
    vsnprintf(log_line, log_len + 1, format, va);
    va_end(va);
    fwrite(log_line, 1, log_len, log_file);
    fflush(log_file);
    free(log_line);
}

void open_log() {
    log_file = fopen("/tmp/myinit.log", "w");
    write_log("myinit stared\n");
}

void close_all_fds() {
    struct rlimit fd_limit;
    getrlimit(RLIMIT_NOFILE, &fd_limit);
    for (int fd = 0; fd < fd_limit.rlim_max; fd++)
        close(fd);
}

void check_absolute_path(char* path) {
    if (path[0] != '/') {
        write_log("you must use only absolute paths\n");
        exit(1);
    }
}

void redirect_io(struct task_info task) {
    freopen(task.in, "r", stdin);
    freopen(task.out, "w", stdout);
}

struct task_info get_task(char* line) {
    char* token = strtok(line, " ");
    check_absolute_path(token);

    struct task_info task;

    task.args = malloc(strlen(token)+1);
    int i = 0;
    while (token != NULL) {
        i = append(task.args, token, i);
        token = strtok(NULL, " ");
    }
    
    if (task.args[i-1][strlen(task.args[i-1]) - 1] == '\n') {
        task.args[i-1][strlen(task.args[i-1]) - 1] = '\0';
    }
    task.in = malloc(strlen(task.args[i-2]));
    task.out = malloc(strlen(task.args[i-1]));
    strcpy(task.in, task.args[i-2]);
    strcpy(task.out, task.args[i-1]);
    check_absolute_path(task.in);
    check_absolute_path(task.out);
    task.args[i-1] = NULL;
    task.args[i-2] = NULL;
    task.args_count = i-2;
    return task;
}

void start_task(int task_num) {
    struct task_info task = tasks[task_num];
    pid_t cpid = fork();
    switch (cpid) {
    case -1:
        write_log("can't start proc: %s\n", task.args[0]);
        exit(1);
        break;
    case 0:
        redirect_io(task);
        execvp(task.args[0], task.args);
    default:
        pids[task_num] = cpid;
        pids_count++;
        write_log("task %d started %s with pid: %d\n", task_num, task.args[0], cpid);
        break;
    }
}

void run() {
    FILE* fp = fopen(conf_name, "r");
    if (fp == NULL) {
        write_log("can not read config file\n");
        exit(1);
    }
    char* line = NULL;
    size_t len = 0;
    int n = 0;
    while ((getline(&line, &len, fp) != -1)) {
        tasks[n] = get_task(line);
        n++;
    }

    pid_t cpid;

    for (int i = 0; i < n; i++) {
        start_task(i);
    }

    while (pids_count) {
        int status = 0;
        cpid = waitpid(-1, &status, 0);
        for (int p = 0; p < n; p++) {
            if (pids[p] == cpid) {
                write_log("task %d finished with status: %d\n", p, status);
                pids[p] = 0;
                pids_count--;
                start_task(p);
            }
        }
    }
    exit(0);
}

void sighup_handler(int signum) {
    for (int p = 0; p < MAXPROC; p++) {
        if (pids[p]) {
            kill(pids[p], SIGKILL);
            write_log("task %d killed\n", p);
        }
    }

    write_log("Handle SIGHUP. Restarting myinit\n");
    run();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("you must specific config file");
        return 1;
    }
    pid_t pid = fork();
    switch (pid) {
    case -1:
        return -1;
        break;
    case 0:
        setsid();
        chdir("/");
        close_all_fds();
        open_log();
        strcpy(conf_name, argv[1]);
        struct sigaction act;
        sigemptyset(&act.sa_mask);
        act.sa_handler = sighup_handler;
        act.sa_flags = SA_NODEFER;
        sigaction(SIGHUP, &act, NULL);
        run();
        return 0;
    default:
        return 0;
    }
}