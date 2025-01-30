#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#define LOG_FILE "network_monitor.log"
#define DOT_FILE "network_graph.dot"  // Файл для записи графа в формате DOT
#define INTERVAL 60  // Интервал мониторинга в секундах
#define MAX_HOPS 30  // Максимальное количество хопов для traceroute
#define PACKET_SIZE 64
#define TIMEOUT 1  // Таймаут в секундах

// Структура для ICMP-пакета
struct icmp_packet {
    struct icmphdr hdr;
    char msg[PACKET_SIZE - sizeof(struct icmphdr)];
};

// Функция для вычисления контрольной суммы
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Упрощённый traceroute
char* traceroute(const char* target) {
    int sockfd;
    struct sockaddr_in addr;
    struct icmp_packet packet;
    char *gateway = NULL;

    // Создаем RAW-сокет
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("socket");
        return NULL;
    }

    // Устанавливаем таймаут
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Заполняем адрес назначения
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, target, &addr.sin_addr);

    // Отправляем ICMP-пакеты с увеличивающимся TTL
    for (int ttl = 1; ttl <= MAX_HOPS; ttl++) {
        // Устанавливаем TTL
        setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

        // Заполняем ICMP-пакет
        memset(&packet, 0, sizeof(packet));
        packet.hdr.type = ICMP_ECHO;
        packet.hdr.code = 0;
        packet.hdr.un.echo.id = getpid();
        packet.hdr.un.echo.sequence = ttl;
        packet.hdr.checksum = checksum(&packet, sizeof(packet));

        // Отправляем пакет
        if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr)) <= 0) {
            perror("sendto");
            break;
        }

        // Получаем ответ
        char buffer[PACKET_SIZE];
        struct sockaddr_in recv_addr;
        socklen_t addr_len = sizeof(recv_addr);
        if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&recv_addr, &addr_len) > 0) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &recv_addr.sin_addr, ip, sizeof(ip));
            if (ttl == 1) {
                gateway = strdup(ip);
            }
            break;
        }
    }

    close(sockfd);
    return gateway;
}

// Функция для записи информации о сети в файл и генерации .DOT
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
    char* gateway = traceroute("8.8.8.8");  // Используем Google DNS для трассировки
    if (gateway) {
        fprintf(log_file, "Gateway: %s\n", gateway);
    } else {
        fprintf(log_file, "Failed to determine gateway.\n");
    }

    // Получаем информацию об узлах с помощью arp
    FILE* arp_output = popen("arp -a", "r");
    if (arp_output) {
        char buffer[256];
        fprintf(log_file, "Network nodes:\n");
        while (fgets(buffer, sizeof(buffer), arp_output) != NULL) {
            fprintf(log_file, "%s", buffer);
        }
        pclose(arp_output);
    } else {
        fprintf(log_file, "Failed to retrieve ARP table.\n");
    }

    fclose(log_file);

    // Генерация .DOT файла
    FILE* dot_file = fopen(DOT_FILE, "w");
    if (!dot_file) {
        perror("Failed to open DOT file");
        return;
    }

    // Заголовок DOT-файла
    fprintf(dot_file, "digraph G {\n");
    fprintf(dot_file, "  node [shape=box];\n");

    // Добавляем шлюз
    if (gateway) {
        fprintf(dot_file, "  \"%s\" [color=red];\n", gateway);
    }

    // Добавляем узлы и связи
    arp_output = popen("arp -a", "r");
    if (arp_output) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), arp_output) != NULL) {
            char ip[16], mac[18], name[64];
            if (sscanf(buffer, "%s %s %s", name, ip, mac) == 3) {
                // Добавляем узел
                fprintf(dot_file, "  \"%s\" [label=\"%s\\n%s\"];\n", ip, ip, mac);
                // Добавляем связь с шлюзом (если шлюз определён)
                if (gateway) {
                    fprintf(dot_file, "  \"%s\" -> \"%s\";\n", ip, gateway);
                }
            }
        }
        pclose(arp_output);
    }

    // Завершение DOT-файла
    fprintf(dot_file, "}\n");
    fclose(dot_file);

    // Освобождаем память, выделенную для gateway
    if (gateway) {
        free(gateway);
    }
}

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

int main() {
    daemonize();

    while (1) {
        log_network_info();
        sleep(INTERVAL);
    }

    return 0;
}
