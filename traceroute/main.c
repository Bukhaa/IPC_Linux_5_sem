#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <time.h>
#include <netdb.h>

#define ARP_FILE "/proc/net/arp"
#define LOG_FILE "network_log.txt"
#define SCAN_INTERVAL 60
#define MAX_LINE_LENGTH 256

// Функция для определения MAC-адреса из строки (как и раньше)
void get_mac_from_line(char *line, char *mac) {
    char *ptr = strtok(line, " ");
    int i = 0;
    while (ptr != NULL && i < 4) {
        if (i == 3) {
            strcpy(mac, ptr);
            return;
        }
        ptr = strtok(NULL, " ");
        i++;
    }
    strcpy(mac, "N/A");
}

void scan_network() {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char ip_addr[INET_ADDRSTRLEN];
    char mac_addr[18];
    char hostname[NI_MAXHOST];
    time_t now;
    time(&now);
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));


    fp = fopen(ARP_FILE, "r");
    if (fp == NULL) {
        perror("Error opening arp table");
        return;
    }

    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp == NULL) {
        perror("Error opening log file");
        fclose(fp);
        return;
    }
    fprintf(log_fp, "==== %s ====\n", time_str);


    fgets(line, MAX_LINE_LENGTH, fp); // skip header
    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        if (sscanf(line, "%s %*s %*s %*s %s %*s\n", ip_addr, mac_addr) == 2) {
             // Resolve hostname
            struct sockaddr_in addr;
            inet_pton(AF_INET, ip_addr, &(addr.sin_addr));
             socklen_t addrlen = sizeof(addr);

            int result = getnameinfo((struct sockaddr *)&addr, addrlen, hostname, sizeof(hostname), NULL, 0, 0);
             if (result != 0) {
                strcpy(hostname, "N/A");
             }

            fprintf(log_fp, "IP: %s, MAC: %s, Hostname: %s\n", ip_addr, mac_addr, hostname);
            printf("IP: %s, MAC: %s, Hostname: %s\n", ip_addr, mac_addr, hostname);
        }
    }
    fclose(log_fp);
    fclose(fp);
}

int main() {
    pid_t pid, sid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }


    while (1) {
        scan_network();
        sleep(SCAN_INTERVAL);
    }

    return EXIT_SUCCESS;
}
