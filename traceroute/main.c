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
#include <netdb.h> // gethostbyaddr

#define ARP_FILE "/proc/net/arp"
#define LOG_FILE "network_log.txt"
#define DOT_FILE "network_graph.dot"
#define PNG_FILE "network_graph.png"
#define SCAN_INTERVAL 60
#define MAX_LINE_LENGTH 256

// Функция для получения MAC-адреса из строки
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

// Функция для определения шлюза (очень базовый метод)
int get_default_gateway(char *gateway_ip)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char interface[IF_NAMESIZE];
    unsigned long dest_addr, gw_addr;
    int if_idx;

    fp = fopen("/proc/net/route", "r");
    if(fp == NULL) {
        return -1;
    }

    // Пропускаем заголовок
    fgets(line, MAX_LINE_LENGTH, fp);
    while(fgets(line, MAX_LINE_LENGTH, fp)) {
        if (sscanf(line, "%s %lx %lx %*x %*x %*x %*x %*x %*x %d", interface, &dest_addr, &gw_addr, &if_idx) == 4) {
            if(dest_addr == 0) {
               inet_ntop(AF_INET, &gw_addr, gateway_ip, INET_ADDRSTRLEN);
               fclose(fp);
               return 0;
             }

        }
    }

    fclose(fp);
    strcpy(gateway_ip, "N/A");
    return -1;
}

void render_dot_to_png(const char *dot_file, const char *png_file) {
    char command[256];
    snprintf(command, sizeof(command), "dot -Tpng -o %s %s", png_file, dot_file);

    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "Ошибка при рендеринге .dot в PNG\n");
    } else {
        printf("Файл %s успешно создан.\n", png_file);
    }
}

void generate_dot_file(const char *gateway_ip, char nodes[256][INET_ADDRSTRLEN], int node_count) {
    FILE *fp = fopen(DOT_FILE, "w");
    if (fp == NULL) {
        perror("Error opening dot file");
        return;
    }

    fprintf(fp, "digraph Network {\n");
    fprintf(fp, "    rankdir=LR;\n");
      fprintf(fp, "    node [shape=box];\n");
    fprintf(fp, "    \"%s\" [shape=house, color=blue];\n", gateway_ip);


    for (int i = 0; i < node_count; i++) {
      if (strcmp(nodes[i], gateway_ip) != 0){
            fprintf(fp, "    \"%s\" -> \"%s\";\n",gateway_ip, nodes[i]);
      }
    }

    fprintf(fp, "}\n");

    fclose(fp);
}


void scan_network() {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char ip_addr[INET_ADDRSTRLEN];
    char mac_addr[18];
    char hostname[NI_MAXHOST];
    char current_gateway[INET_ADDRSTRLEN];
    time_t now;
    time(&now);
    char time_str[26];
    int node_count = 0;
    char nodes[256][INET_ADDRSTRLEN];


    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    if(get_default_gateway(current_gateway) == 0) {
        printf("Default gateway: %s\n", current_gateway);
    }
    else {
        printf("Could not find default gateway\n");
    }

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
    fprintf(log_fp, "Default gateway: %s\n", current_gateway);

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

            strcpy(nodes[node_count], ip_addr);
            node_count++;
        }

        }
    fclose(log_fp);
    fclose(fp);


    generate_dot_file(current_gateway, nodes, node_count);
    render_dot_to_png(DOT_FILE, PNG_FILE);
}


int main() {
    pid_t pid, sid;
    pid = fork();

    if(pid < 0) {
        exit(EXIT_FAILURE);
    }

    if(pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();

    if(sid < 0) {
        exit(EXIT_FAILURE);
    }

    while (1) {
        scan_network();
        sleep(SCAN_INTERVAL);
    }

    return EXIT_SUCCESS;
}
