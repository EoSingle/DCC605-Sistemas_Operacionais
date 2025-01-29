#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>

#define MAX_PROCS 20
#define PROCNAME_WIDTH 10
#define CLEAR_TIME 5 // Controla o tempo de atualização da lista de processos

void list_processes() {
    struct dirent *entry;
    DIR *dp = opendir("/proc");
    int count = 0;

    if (dp == NULL) {
        perror("opendir: /proc");
        return;
    }

    printf("PID    | User    |  PROCNAME  | Estado |\n");
    printf("-------|---------|------------|--------|\n");

    while ((entry = readdir(dp)) != NULL && count < MAX_PROCS) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            char stat_file[256], proc_name[256], state;
            int pid;
            FILE *statf;
            struct stat sb;
            struct passwd *pw;

            snprintf(stat_file, sizeof(stat_file), "/proc/%s/stat", entry->d_name);
            stat(stat_file, &sb);

            if ((pw = getpwuid(sb.st_uid)) != NULL) {
                statf = fopen(stat_file, "r");
                if (statf) {
                    fscanf(statf, "%d (%[^)]) %c", &pid, proc_name, &state); 

                    if (strlen(proc_name) > PROCNAME_WIDTH - 1) {
                        proc_name[PROCNAME_WIDTH - 1] = '\0';  
                    }

                    printf("%-6d | %-7s | %-10s | %-6c |\n", pid, pw->pw_name, proc_name, state);
                    fclose(statf);
                    count++;
                }
            }
        }
    }
    closedir(dp);
}

void *signal_handler(void *arg) {
    int pid, signal;
    while (1) {
        printf("Enter PID and signal: ");
        scanf("%d %d", &pid, &signal);
        if (kill(pid, signal) == -1) {
            perror("Error sending signal");
        } else {
            printf("Signal %d sent to process %d\n", signal, pid);
        }
    }
    return NULL;
}

int main() {
    pthread_t signal_thread;
    
    pthread_create(&signal_thread, NULL, signal_handler, NULL);

    while (1) {
        system("clear");
        list_processes();
        printf("Enter signal:\n");
        sleep(CLEAR_TIME);  
    }

    pthread_join(signal_thread, NULL);
    return 0;
}
