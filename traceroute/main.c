#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define LOG_FILE "network_monitor.log"
#define INTERVAL 60  // Интервал мониторинга в секундах

// Функция для демонизации
void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    // Закрываем стандартные файловые дескрипторы
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

// Функция для выполнения команды и получения вывода
char* execute_command(const char* command) {
    FILE* fp = popen(command, "r");
    if (!fp) {
        perror("Failed to run command");
        return NULL;
    }

    char* output = malloc(4096);
    if (!output) {
        perror("Failed to allocate memory");
        pclose(fp);
        return NULL;
    }

    size_t len = 0;
    while (fgets(output + len, 4096 - len, fp) != NULL) {
        len = strlen(output);
    }

    pclose(fp);
    return output;
}

// Функция для записи информации о сети в файл
void log_network_info() {
    FILE* log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Failed to open log file");
        return;
    }

    // Получаем текущее время
    time_t now = time(NULL);
    fprintf(log_file, "Monitoring at: %s", ctime(&now));

    // Определяем шлюз с помощью traceroute
    char* gateway = execute_command("traceroute -m 1 8.8.8.8 | grep -oP '(?<=^\\s1\\s)[^\\s]+'");
    if (gateway) {
        fprintf(log_file, "Gateway: %s", gateway);
        free(gateway);
    } else {
        fprintf(log_file, "Failed to determine gateway.\n");
    }

    // Получаем информацию об узлах с помощью arp
    char* arp_output = execute_command("arp -a");
    if (arp_output) {
        fprintf(log_file, "Network nodes:\n%s", arp_output);
        free(arp_output);
    } else {
        fprintf(log_file, "Failed to retrieve ARP table.\n");
    }

    fclose(log_file);
}

int main() {
    daemonize();

    while (1) {
        log_network_info();
        sleep(INTERVAL);
    }

    return 0;
}
