/**
 * 端口扫描器
 * 支持TCP Connect, TCP SYN, UDP扫描
 * 多线程，服务识别，横幅抓取
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include "framework/plugin_interface.h"

#define MAX_THREADS 200
#define MAX_PORTS 65535
#define SCAN_TIMEOUT 2
#define MAX_BANNER_SIZE 1024
#define MAX_SERVICES 1000

// 伪头部用于计算TCP校验和
struct pseudo_header {
    uint32_t source_address;
    uint32_t dest_address;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t tcp_length;
};

// 扫描类型枚举
typedef enum {
    SCAN_TCP_CONNECT = 0,
    SCAN_TCP_SYN,
    SCAN_TCP_ACK,
    SCAN_TCP_FIN,
    SCAN_TCP_XMAS,
    SCAN_TCP_NULL,
    SCAN_UDP,
    SCAN_UDP_CONNECT
} ScanType;

// 端口状态枚举
typedef enum {
    PORT_OPEN = 0,
    PORT_CLOSED,
    PORT_FILTERED,
    PORT_OPEN_FILTERED,
    PORT_UNFILTERED
} PortState;

// 服务信息结构
typedef struct {
    int port;
    char name[32];
    char protocol[8];
    char description[128];
} ServiceInfo;

// 扫描结果结构
typedef struct {
    int port;
    char protocol[8];
    char state[16];
    char service[32];
    char banner[256];
    long response_time; // 响应时间(ms)
    struct timeval timestamp;
} ScanResult;

// 线程参数结构
typedef struct {
    char *target;
    struct in_addr target_addr;
    int start_port;
    int end_port;
    int *ports_to_scan;
    int port_count;
    int timeout_ms;
    ScanType scan_type;
    int thread_id;
    int *current_index;
    pthread_mutex_t *index_mutex;
    ScanResult *results;
    int *result_count;
    pthread_mutex_t *result_mutex;
    int *total_scanned;
    int *open_ports;
    int *closed_ports;
    int *filtered_ports;
    int banner_grab;
    int verbose;
} ThreadParams;

// 全局变量
static ServiceInfo *service_db = NULL;
static int service_count = 0;
static volatile int scan_running = 0;
static pthread_mutex_t scan_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timeval scan_start_time;

// 插件信息
void get_plugin_info(PluginInfo *info) {
    strcpy(info->name, "port-scanner");
    strcpy(info->version, "2.0.0");
    strcpy(info->author, "PenTest Team");
    strcpy(info->description, "端口扫描器，支持TCP/UDP多种扫描类型");
    strcpy(info->category, "scanner");
}

// 初始化服务数据库
void init_service_database(void) {
    // 常见服务端口映射
    ServiceInfo common_services[] = {
        {20, "ftp-data", "tcp", "FTP Data Transfer"},
        {21, "ftp", "tcp", "File Transfer Protocol"},
        {22, "ssh", "tcp", "Secure Shell"},
        {23, "telnet", "tcp", "Telnet"},
        {25, "smtp", "tcp", "Simple Mail Transfer Protocol"},
        {53, "dns", "tcp/udp", "Domain Name System"},
        {67, "dhcp", "udp", "DHCP Server"},
        {68, "dhcp", "udp", "DHCP Client"},
        {69, "tftp", "udp", "Trivial File Transfer Protocol"},
        {80, "http", "tcp", "Hypertext Transfer Protocol"},
        {110, "pop3", "tcp", "Post Office Protocol v3"},
        {111, "rpcbind", "tcp/udp", "RPC Portmapper"},
        {123, "ntp", "udp", "Network Time Protocol"},
        {135, "msrpc", "tcp", "Microsoft RPC"},
        {137, "netbios-ns", "udp", "NetBIOS Name Service"},
        {138, "netbios-dgm", "udp", "NetBIOS Datagram Service"},
        {139, "netbios-ssn", "tcp", "NetBIOS Session Service"},
        {143, "imap", "tcp", "Internet Message Access Protocol"},
        {161, "snmp", "udp", "Simple Network Management Protocol"},
        {162, "snmptrap", "udp", "SNMP Trap"},
        {389, "ldap", "tcp", "Lightweight Directory Access Protocol"},
        {443, "https", "tcp", "HTTP over SSL/TLS"},
        {445, "microsoft-ds", "tcp", "Microsoft Directory Services"},
        {465, "smtps", "tcp", "SMTP over SSL"},
        {514, "syslog", "udp", "System Logging Protocol"},
        {587, "smtp", "tcp", "SMTP Submission"},
        {636, "ldaps", "tcp", "LDAP over SSL"},
        {993, "imaps", "tcp", "IMAP over SSL"},
        {995, "pop3s", "tcp", "POP3 over SSL"},
        {1080, "socks", "tcp", "SOCKS Proxy"},
        {1433, "ms-sql-s", "tcp", "Microsoft SQL Server"},
        {1521, "oracle", "tcp", "Oracle Database"},
        {1723, "pptp", "tcp", "Point-to-Point Tunneling Protocol"},
        {1883, "mqtt", "tcp", "MQ Telemetry Transport"},
        {1900, "upnp", "udp", "Universal Plug and Play"},
        {2049, "nfs", "tcp/udp", "Network File System"},
        {2082, "cpanel", "tcp", "cPanel"},
        {2083, "cpanel", "tcp", "cPanel SSL"},
        {2086, "whm", "tcp", "WebHost Manager"},
        {2087, "whm", "tcp", "WebHost Manager SSL"},
        {2095, "webmail", "tcp", "cPanel WebMail"},
        {2096, "webmail", "tcp", "cPanel WebMail SSL"},
        {2181, "zookeeper", "tcp", "Apache ZooKeeper"},
        {2375, "docker", "tcp", "Docker REST API"},
        {2376, "docker", "tcp", "Docker REST API SSL"},
        {3000, "nodejs", "tcp", "Node.js Application"},
        {3306, "mysql", "tcp", "MySQL Database"},
        {3389, "ms-wbt-server", "tcp", "Remote Desktop Protocol"},
        {3690, "svn", "tcp", "Subversion"},
        {4000, "remoteanything", "tcp", "Remote Anything"},
        {4040, "yo", "tcp", "Yarn Application Manager"},
        {4200, "angular", "tcp", "Angular Development Server"},
        {4369, "epmd", "tcp", "Erlang Port Mapper Daemon"},
        {5000, "upnp", "tcp", "Universal Plug and Play"},
        {5432, "postgresql", "tcp", "PostgreSQL Database"},
        {5601, "kibana", "tcp", "Kibana"},
        {5672, "amqp", "tcp", "Advanced Message Queuing Protocol"},
        {5900, "vnc", "tcp", "Virtual Network Computing"},
        {5984, "couchdb", "tcp", "Apache CouchDB"},
        {6379, "redis", "tcp", "Redis Key-Value Store"},
        {7001, "weblogic", "tcp", "Oracle WebLogic Server"},
        {7002, "weblogic", "tcp", "Oracle WebLogic Server SSL"},
        {8000, "http-alt", "tcp", "HTTP Alternate"},
        {8008, "http-alt", "tcp", "HTTP Alternate"},
        {8080, "http-proxy", "tcp", "HTTP Proxy"},
        {8081, "http-proxy", "tcp", "HTTP Proxy"},
        {8088, "http-alt", "tcp", "HTTP Alternate"},
        {8089, "splunk", "tcp", "Splunk"},
        {8443, "https-alt", "tcp", "HTTPS Alternate"},
        {8888, "http-alt", "tcp", "HTTP Alternate"},
        {9000, "sonar", "tcp", "SonarQube"},
        {9001, "tor", "tcp", "Tor"},
        {9042, "cassandra", "tcp", "Apache Cassandra"},
        {9092, "kafka", "tcp", "Apache Kafka"},
        {9200, "elasticsearch", "tcp", "Elasticsearch"},
        {9300, "elasticsearch", "tcp", "Elasticsearch Transport"},
        {9418, "git", "tcp", "Git"},
        {11211, "memcache", "tcp", "Memcached"},
        {15672, "rabbitmq", "tcp", "RabbitMQ Management"},
        {27017, "mongodb", "tcp", "MongoDB"},
        {27018, "mongodb", "tcp", "MongoDB Sharding"},
        {28017, "mongodb", "tcp", "MongoDB Web Interface"},
        {50000, "db2", "tcp", "IBM DB2"},
        {50070, "hadoop", "tcp", "Hadoop HDFS NameNode"},
        {61616, "activemq", "tcp", "Apache ActiveMQ"}
    };

    service_count = sizeof(common_services) / sizeof(ServiceInfo);
    service_db = malloc(sizeof(ServiceInfo) * service_count);
    memcpy(service_db, common_services, sizeof(common_services));
}

// 根据端口号获取服务信息
const char* get_service_by_port(int port, const char* protocol) {
    for (int i = 0; i < service_count; i++) {
        if (service_db[i].port == port) {
            // 检查协议是否匹配
            if (strstr(service_db[i].protocol, protocol) != NULL) {
                return service_db[i].name;
            }
        }
    }
    return "unknown";
}

// 计算TCP校验和
unsigned short tcp_checksum(unsigned short *ptr, int nbytes) {
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }

    if (nbytes == 1) {
        oddbyte = 0;
        *((unsigned char*)&oddbyte) = *(unsigned char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;

    return answer;
}

// 创建原始套接字
int create_raw_socket(void) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        perror("创建原始套接字失败");
        return -1;
    }

    // 设置IP_HDRINCL选项，自己构造IP头部
    int one = 1;
    const int *val = &one;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("设置IP_HDRINCL失败");
        close(sock);
        return -1;
    }

    return sock;
}

// TCP Connect扫描
int tcp_connect_scan(const char *target, int port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    // 设置超时
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // 解析目标地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, target, &addr.sin_addr) <= 0) {
        // DNS解析
        struct hostent *host = gethostbyname(target);
        if (!host) {
            close(sock);
            return -1;
        }
        memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    }

    // 尝试连接
    struct timeval start, end;
    gettimeofday(&start, NULL);

    int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    gettimeofday(&end, NULL);
    long response_time = (end.tv_sec - start.tv_sec) * 1000 +
    (end.tv_usec - start.tv_usec) / 1000;

    close(sock);

    if (result == 0) {
        return response_time; // 返回响应时间
    } else {
        return -1;
    }
}

// TCP SYN扫描（半开放扫描）
int tcp_syn_scan(const char *target, int port, int timeout_ms) {
    int raw_sock = create_raw_socket();
    if (raw_sock < 0) {
        return -1;
    }

    // 需要root权限
    if (geteuid() != 0) {
        printf("警告: TCP SYN扫描需要root权限\n");
        close(raw_sock);
        return -2;
    }

    // 构造IP头部
    char packet[4096];
    struct iphdr *iph = (struct iphdr *)packet;
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));
    struct sockaddr_in sin;

    // 目标地址
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (inet_pton(AF_INET, target, &sin.sin_addr) <= 0) {
        struct hostent *host = gethostbyname(target);
        if (!host) {
            close(raw_sock);
            return -1;
        }
        memcpy(&sin.sin_addr, host->h_addr_list[0], host->h_length);
    }

    // 填充IP头部
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iph->id = htonl(54321); // 随机ID
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;
    iph->saddr = inet_addr("192.168.1.100"); // 伪造源地址
    iph->daddr = sin.sin_addr.s_addr;

    // 填充TCP头部
    tcph->source = htons(12345); // 随机源端口
    tcph->dest = htons(port);
    tcph->seq = htonl(1105024978);
    tcph->ack_seq = 0;
    tcph->doff = 5;
    tcph->fin = 0;
    tcph->syn = 1;
    tcph->rst = 0;
    tcph->psh = 0;
    tcph->ack = 0;
    tcph->urg = 0;
    tcph->window = htons(5840);
    tcph->check = 0;
    tcph->urg_ptr = 0;

    // 计算TCP校验和
    struct pseudo_header psh;
    psh.source_address = iph->saddr;
    psh.dest_address = sin.sin_addr.s_addr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr));

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
    char *pseudogram = malloc(psize);
    memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));

    tcph->check = tcp_checksum((unsigned short*)pseudogram, psize);
    free(pseudogram);

    // 发送SYN包
    int sent = sendto(raw_sock, packet, iph->tot_len, 0,
                      (struct sockaddr *)&sin, sizeof(sin));

    if (sent < 0) {
        perror("发送SYN包失败");
        close(raw_sock);
        return -1;
    }

    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(raw_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // 接收响应
    char buffer[4096];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(raw_sock, &readfds);

    struct timeval select_timeout = tv;
    int ret = select(raw_sock + 1, &readfds, NULL, NULL, &select_timeout);

    if (ret > 0) {
        int received = recvfrom(raw_sock, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&from, &fromlen);
        if (received > 0) {
            struct iphdr *recv_iph = (struct iphdr *)buffer;
            struct tcphdr *recv_tcph = (struct tcphdr *)(buffer + (recv_iph->ihl * 4));

            // 检查是否是SYN-ACK响应
            if (recv_tcph->syn && recv_tcph->ack) {
                close(raw_sock);
                return 1; // 端口开放
            } else if (recv_tcph->rst) {
                close(raw_sock);
                return 0; // 端口关闭
            }
        }
    }

    close(raw_sock);
    return -1; // 端口被过滤或无响应
}

// UDP扫描
int udp_scan(const char *target, int port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    // 设置超时
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, target, &addr.sin_addr) <= 0) {
        struct hostent *host = gethostbyname(target);
        if (!host) {
            close(sock);
            return -1;
        }
        memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    }

    // 发送空数据包
    char buffer[1] = {0};
    int sent = sendto(sock, buffer, 1, 0, (struct sockaddr *)&addr, sizeof(addr));

    if (sent < 0) {
        close(sock);
        return -1;
    }

    // 尝试接收ICMP端口不可达消息
    char recv_buffer[1024];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    int received = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0,
                            (struct sockaddr *)&from, &fromlen);

    close(sock);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1; // 端口可能开放（UDP无响应）
        }
        return -1;
    }

    // 检查是否是ICMP端口不可达消息
    // 简化处理：如果有响应，可能是ICMP错误
    return 0; // 端口关闭
}

// 横幅抓取
char* grab_banner(const char *target, int port, int timeout_ms, const char *protocol) {
    if (strcmp(protocol, "tcp") != 0) {
        return NULL; // 只支持TCP横幅抓取
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return NULL;
    }

    // 设置超时
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, target, &addr.sin_addr) <= 0) {
        struct hostent *host = gethostbyname(target);
        if (!host) {
            close(sock);
            return NULL;
        }
        memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    }

    // 连接
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }

    // 根据端口发送不同的探针
    char *banner = malloc(MAX_BANNER_SIZE);
    memset(banner, 0, MAX_BANNER_SIZE);
    char probe[256];
    int probe_len = 0;

    if (port == 80 || port == 8080 || port == 8000 || port == 8888) {
        // HTTP
        strcpy(probe, "GET / HTTP/1.0\r\n\r\n");
        probe_len = strlen(probe);
    } else if (port == 443 || port == 8443) {
        // HTTPS (尝试SSL握手)
        strcpy(probe, "\x16\x03\x01\x00\x75\x01\x00\x00\x71\x03\x03"); // TLS ClientHello开始
        probe_len = 11;
    } else if (port == 21 || port == 2121) {
        // FTP
        strcpy(probe, "USER anonymous\r\n");
        probe_len = strlen(probe);
    } else if (port == 22) {
        // SSH (等待banner)
        probe_len = 0;
    } else if (port == 25 || port == 587) {
        // SMTP
        strcpy(probe, "HELO example.com\r\n");
        probe_len = strlen(probe);
    } else if (port == 110) {
        // POP3
        strcpy(probe, "USER test\r\n");
        probe_len = strlen(probe);
    } else if (port == 143) {
        // IMAP
        strcpy(probe, "a001 LOGIN user pass\r\n");
        probe_len = strlen(probe);
    } else if (port == 3306) {
        // MySQL
        strcpy(probe, "\x0a"); // Protocol version 10
        probe_len = 1;
    } else {
        // 通用探针：发送换行符
        strcpy(probe, "\r\n");
        probe_len = 2;
    }

    // 发送探针
    if (probe_len > 0) {
        send(sock, probe, probe_len, 0);
    }

    // 接收响应
    char buffer[1024];
    int total_received = 0;
    int received;

    while (total_received < MAX_BANNER_SIZE - 1) {
        received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            break;
        }

        buffer[received] = '\0';

        // 过滤不可打印字符
        for (int i = 0; i < received; i++) {
            if (buffer[i] < 32 && buffer[i] != '\n' && buffer[i] != '\r' && buffer[i] != '\t') {
                buffer[i] = '.';
            }
        }

        // 添加到banner
        if (total_received + received < MAX_BANNER_SIZE) {
            strcat(banner + total_received, buffer);
            total_received += received;
        } else {
            strncat(banner + total_received, buffer, MAX_BANNER_SIZE - total_received - 1);
            break;
        }
    }

    close(sock);

    // 清理banner：去除多余空白字符
    char *clean_banner = malloc(strlen(banner) + 1);
    char *src = banner;
    char *dst = clean_banner;
    int in_space = 0;

    while (*src) {
        if (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r') {
            if (!in_space && dst > clean_banner) {
                *dst++ = ' ';
                in_space = 1;
            }
        } else {
            *dst++ = *src;
            in_space = 0;
        }
        src++;
    }
    *dst = '\0';

    // 去除首尾空格
    char *start = clean_banner;
    while (*start == ' ') start++;

    char *end = start + strlen(start) - 1;
    while (end > start && *end == ' ') end--;
    *(end + 1) = '\0';

    free(banner);

    if (strlen(start) == 0) {
        free(clean_banner);
        return NULL;
    }

    return start;
}

// 扫描线程函数
void* scan_thread_func(void *arg) {
    ThreadParams *params = (ThreadParams *)arg;

    while (scan_running) {
        int port_index = -1;

        // 获取下一个要扫描的端口
        pthread_mutex_lock(params->index_mutex);
        if (*params->current_index < params->port_count) {
            port_index = (*params->current_index)++;
        }
        pthread_mutex_unlock(params->index_mutex);

        if (port_index == -1) {
            break; // 所有端口都已扫描
        }

        int port = params->ports_to_scan[port_index];

        // 执行扫描
        int result = -1;
        long response_time = -1;
        const char *protocol = "tcp";

        switch (params->scan_type) {
            case SCAN_TCP_CONNECT:
                response_time = tcp_connect_scan(params->target, port, params->timeout_ms);
                if (response_time >= 0) {
                    result = 1; // 开放
                } else {
                    result = 0; // 关闭
                }
                break;

            case SCAN_TCP_SYN:
                result = tcp_syn_scan(params->target, port, params->timeout_ms);
                protocol = "tcp";
                break;

            case SCAN_UDP:
                result = udp_scan(params->target, port, params->timeout_ms);
                protocol = "udp";
                break;

            default:
                result = -1;
        }

        // 更新统计
        pthread_mutex_lock(params->result_mutex);
        (*params->total_scanned)++;

        if (result > 0) {
            // 端口开放
            (*params->open_ports)++;

            // 添加结果
            if (*params->result_count < MAX_PORTS) {
                ScanResult *scan_result = &params->results[*params->result_count];
                scan_result->port = port;
                strcpy(scan_result->protocol, protocol);
                strcpy(scan_result->state, "open");

                const char *service = get_service_by_port(port, protocol);
                strncpy(scan_result->service, service, sizeof(scan_result->service) - 1);

                // 抓取横幅
                if (params->banner_grab && result > 0 && strcmp(protocol, "tcp") == 0) {
                    char *banner = grab_banner(params->target, port, params->timeout_ms, protocol);
                    if (banner) {
                        strncpy(scan_result->banner, banner, sizeof(scan_result->banner) - 1);
                        free(banner);
                    } else {
                        scan_result->banner[0] = '\0';
                    }
                } else {
                    scan_result->banner[0] = '\0';
                }

                scan_result->response_time = (response_time > 0) ? response_time : 0;
                gettimeofday(&scan_result->timestamp, NULL);

                (*params->result_count)++;
            }
        } else if (result == 0) {
            (*params->closed_ports)++;
        } else {
            (*params->filtered_ports)++;
        }
        pthread_mutex_unlock(params->result_mutex);

        // 显示进度（如果启用详细模式）
        if (params->verbose) {
            pthread_mutex_lock(&scan_mutex);
            printf("线程 %d: 扫描端口 %d - %s\n",
                   params->thread_id, port,
                   (result > 0) ? "开放" : (result == 0) ? "关闭" : "过滤");
            pthread_mutex_unlock(&scan_mutex);
        }
    }

    return NULL;
}

// 解析端口范围字符串
int* parse_port_range(const char *range_str, int *count) {
    int *ports = NULL;
    *count = 0;

    if (!range_str || strlen(range_str) == 0) {
        return NULL;
    }

    // 分配初始内存
    ports = malloc(MAX_PORTS * sizeof(int));
    if (!ports) {
        return NULL;
    }

    char *token, *str, *tofree;
    tofree = str = strdup(range_str);

    while ((token = strsep(&str, ",")) != NULL) {
        char *dash = strchr(token, '-');
        if (dash) {
            // 端口范围
            *dash = '\0';
            int start = atoi(token);
            int end = atoi(dash + 1);

            if (start < 1) start = 1;
            if (end > MAX_PORTS) end = MAX_PORTS;
            if (start > end) {
                int temp = start;
                start = end;
                end = temp;
            }

            for (int i = start; i <= end && *count < MAX_PORTS; i++) {
                ports[(*count)++] = i;
            }
        } else {
            // 单个端口
            int port = atoi(token);
            if (port >= 1 && port <= MAX_PORTS && *count < MAX_PORTS) {
                ports[(*count)++] = port;
            }
        }
    }

    free(tofree);

    // 重新分配内存到实际大小
    if (*count > 0) {
        int *temp = realloc(ports, *count * sizeof(int));
        if (temp) {
            ports = temp;
        }
    } else {
        free(ports);
        ports = NULL;
    }

    return ports;
}

// 执行扫描
int perform_scan(const char *target, const char *port_range,
                 int thread_count, int timeout_ms, ScanType scan_type,
                 int banner_grab, int verbose,
                 ScanResult **results_ptr, int *result_count) {

    if (thread_count < 1) thread_count = 1;
    if (thread_count > MAX_THREADS) thread_count = MAX_THREADS;
    if (timeout_ms < 100) timeout_ms = 100;
    if (timeout_ms > 10000) timeout_ms = 10000;

    // 解析端口范围
    int port_count = 0;
    int *ports = parse_port_range(port_range, &port_count);

    if (!ports || port_count == 0) {
        printf("错误: 无效的端口范围\n");
        return -1;
    }

    // 解析目标地址
    struct in_addr target_addr;
    if (inet_pton(AF_INET, target, &target_addr) <= 0) {
        struct hostent *host = gethostbyname(target);
        if (!host) {
            printf("错误: 无法解析目标地址 %s\n", target);
            free(ports);
            return -1;
        }
        memcpy(&target_addr, host->h_addr_list[0], sizeof(target_addr));
    }

    printf("开始扫描 %s (%s)\n", target, inet_ntoa(target_addr));
    printf("端口范围: %s (%d个端口)\n", port_range, port_count);
    printf("线程数: %d, 超时: %dms, 扫描类型: ", thread_count, timeout_ms);

    switch (scan_type) {
        case SCAN_TCP_CONNECT: printf("TCP Connect\n"); break;
        case SCAN_TCP_SYN: printf("TCP SYN\n"); break;
        case SCAN_UDP: printf("UDP\n"); break;
        default: printf("Unknown\n"); break;
    }

    printf("横幅抓取: %s\n", banner_grab ? "启用" : "禁用");
    printf("========================================\n");

    // 初始化结果数组
    ScanResult *results = malloc(MAX_PORTS * sizeof(ScanResult));
    int total_results = 0;

    // 统计信息
    int total_scanned = 0;
    int open_ports = 0;
    int closed_ports = 0;
    int filtered_ports = 0;

    // 线程管理
    pthread_t threads[thread_count];
    ThreadParams thread_params[thread_count];
    int current_index = 0;

    pthread_mutex_t index_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

    // 设置扫描状态
    scan_running = 1;
    gettimeofday(&scan_start_time, NULL);

    // 创建线程
    for (int i = 0; i < thread_count; i++) {
        thread_params[i].target = strdup(target);
        thread_params[i].target_addr = target_addr;
        thread_params[i].ports_to_scan = ports;
        thread_params[i].port_count = port_count;
        thread_params[i].start_port = ports[0];
        thread_params[i].end_port = ports[port_count - 1];
        thread_params[i].timeout_ms = timeout_ms;
        thread_params[i].scan_type = scan_type;
        thread_params[i].thread_id = i;
        thread_params[i].current_index = &current_index;
        thread_params[i].index_mutex = &index_mutex;
        thread_params[i].results = results;
        thread_params[i].result_count = &total_results;
        thread_params[i].result_mutex = &result_mutex;
        thread_params[i].total_scanned = &total_scanned;
        thread_params[i].open_ports = &open_ports;
        thread_params[i].closed_ports = &closed_ports;
        thread_params[i].filtered_ports = &filtered_ports;
        thread_params[i].banner_grab = banner_grab;
        thread_params[i].verbose = verbose;

        pthread_create(&threads[i], NULL, scan_thread_func, &thread_params[i]);
    }

    // 显示进度
    struct timeval last_update;
    gettimeofday(&last_update, NULL);

    while (scan_running) {
        sleep(1);

        struct timeval now;
        gettimeofday(&now, NULL);

        // 每2秒更新一次进度
        if ((now.tv_sec - last_update.tv_sec) >= 2) {
            pthread_mutex_lock(&result_mutex);
            int scanned = total_scanned;
            int open = open_ports;
            pthread_mutex_unlock(&result_mutex);

            float progress = (float)scanned / port_count * 100;
            printf("进度: %d/%d (%.1f%%) - 开放端口: %d\r",
                   scanned, port_count, progress, open);
            fflush(stdout);

            last_update = now;
        }

        // 检查是否完成
        pthread_mutex_lock(&index_mutex);
        if (current_index >= port_count) {
            scan_running = 0;
        }
        pthread_mutex_unlock(&index_mutex);
    }

    // 等待所有线程完成
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
        free(thread_params[i].target);
    }

    // 清理互斥锁
    pthread_mutex_destroy(&index_mutex);
    pthread_mutex_destroy(&result_mutex);

    // 计算扫描时间
    struct timeval scan_end_time;
    gettimeofday(&scan_end_time, NULL);
    long scan_time = (scan_end_time.tv_sec - scan_start_time.tv_sec) * 1000 +
    (scan_end_time.tv_usec - scan_start_time.tv_usec) / 1000;

    printf("\n\n扫描完成!\n");
    printf("扫描时间: %.2f秒\n", scan_time / 1000.0);
    printf("统计: 开放=%d, 关闭=%d, 过滤=%d\n",
           open_ports, closed_ports, filtered_ports);

    // 返回结果
    *results_ptr = results;
    *result_count = total_results;

    free(ports);
    return 0;
                 }

                 // 显示扫描结果
                 void display_results(ScanResult *results, int count, int show_banner) {
                     if (count == 0) {
                         printf("未发现开放端口\n");
                         return;
                     }

                     printf("\n扫描结果 (%d个开放端口):\n", count);
                     printf("================================================================================\n");
                     if (show_banner) {
                         printf("%-8s %-8s %-10s %-20s %-8s %s\n",
                                "端口", "协议", "状态", "服务", "响应时间", "横幅");
                         printf("%-8s %-8s %-10s %-20s %-8s %s\n",
                                "----", "----", "----", "----", "--------", "------");

                         for (int i = 0; i < count; i++) {
                             printf("%-8d %-8s %-10s %-20s %-8ldms %s\n",
                                    results[i].port,
                                    results[i].protocol,
                                    results[i].state,
                                    results[i].service,
                                    results[i].response_time,
                                    results[i].banner[0] ? results[i].banner : "");
                         }
                     } else {
                         printf("%-8s %-8s %-10s %-20s %-8s\n",
                                "端口", "协议", "状态", "服务", "响应时间");
                         printf("%-8s %-8s %-10s %-20s %-8s\n",
                                "----", "----", "----", "----", "--------");

                         for (int i = 0; i < count; i++) {
                             printf("%-8d %-8s %-10s %-20s %-8ldms\n",
                                    results[i].port,
                                    results[i].protocol,
                                    results[i].state,
                                    results[i].service,
                                    results[i].response_time);
                         }
                     }
                 }

                 // 保存结果到文件
                 void save_results(const char *filename, const char *format,
                                   ScanResult *results, int count, const char *target) {

                     if (count == 0) {
                         printf("没有结果可保存\n");
                         return;
                     }

                     FILE *fp = fopen(filename, "w");
                     if (!fp) {
                         printf("错误: 无法创建文件 %s\n", filename);
                         return;
                     }

                     time_t now = time(NULL);
                     struct tm *tm_info = localtime(&now);
                     char time_str[64];
                     strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

                     if (strcmp(format, "json") == 0) {
                         fprintf(fp, "{\n");
                         fprintf(fp, "  \"scan_info\": {\n");
                         fprintf(fp, "    \"target\": \"%s\",\n", target);
                         fprintf(fp, "    \"scan_time\": \"%s\",\n", time_str);
                         fprintf(fp, "    \"open_ports\": %d\n", count);
                         fprintf(fp, "  },\n");
                         fprintf(fp, "  \"results\": [\n");

                         for (int i = 0; i < count; i++) {
                             fprintf(fp, "    {\n");
                             fprintf(fp, "      \"port\": %d,\n", results[i].port);
                             fprintf(fp, "      \"protocol\": \"%s\",\n", results[i].protocol);
                             fprintf(fp, "      \"state\": \"%s\",\n", results[i].state);
                             fprintf(fp, "      \"service\": \"%s\",\n", results[i].service);
                             fprintf(fp, "      \"response_time\": %ld,\n", results[i].response_time);
                             fprintf(fp, "      \"banner\": \"%s\"\n", results[i].banner);

                             if (i < count - 1) {
                                 fprintf(fp, "    },\n");
                             } else {
                                 fprintf(fp, "    }\n");
                             }
                         }

                         fprintf(fp, "  ]\n");
                         fprintf(fp, "}\n");
                     } else if (strcmp(format, "csv") == 0) {
                         fprintf(fp, "Port,Protocol,State,Service,Response_Time,Banner\n");
                         for (int i = 0; i < count; i++) {
                             // 转义引号
                             char escaped_banner[512];
                             char *src = results[i].banner;
                             char *dst = escaped_banner;
                             while (*src && (dst - escaped_banner) < 510) {
                                 if (*src == '"') {
                                     *dst++ = '"';
                                     *dst++ = '"';
                                 } else {
                                     *dst++ = *src;
                                 }
                                 src++;
                             }
                             *dst = '\0';

                             fprintf(fp, "%d,%s,%s,%s,%ld,\"%s\"\n",
                                     results[i].port,
                                     results[i].protocol,
                                     results[i].state,
                                     results[i].service,
                                     results[i].response_time,
                                     escaped_banner);
                         }
                     } else {
                         // 纯文本格式
                         fprintf(fp, "端口扫描结果\n");
                         fprintf(fp, "目标: %s\n", target);
                         fprintf(fp, "扫描时间: %s\n", time_str);
                         fprintf(fp, "开放端口: %d\n\n", count);

                         for (int i = 0; i < count; i++) {
                             fprintf(fp, "端口 %d (%s):\n", results[i].port, results[i].protocol);
                             fprintf(fp, "  状态: %s\n", results[i].state);
                             fprintf(fp, "  服务: %s\n", results[i].service);
                             fprintf(fp, "  响应时间: %ldms\n", results[i].response_time);
                             if (results[i].banner[0]) {
                                 fprintf(fp, "  横幅: %s\n", results[i].banner);
                             }
                             fprintf(fp, "\n");
                         }
                     }

                     fclose(fp);
                     printf("结果已保存到: %s (格式: %s)\n", filename, format);
                                   }

                                   // 插件初始化
                                   int port_scanner_init(void) {
                                       printf("端口扫描器初始化...\n");
                                       init_service_database();
                                       return 0;
                                   }

                                   // 插件清理
                                   void port_scanner_cleanup(void) {
                                       if (service_db) {
                                           free(service_db);
                                           service_db = NULL;
                                       }
                                       printf("端口扫描器清理完成\n");
                                   }

                                   // 执行命令
                                   int port_scanner_execute(int argc, char **argv) {
                                       if (argc < 2) {
                                           printf("用法: port-scanner <命令> [参数]\n");
                                           printf("命令:\n");
                                           printf("  scan <目标> [选项]         执行端口扫描\n");
                                           printf("  help                       显示详细帮助\n");
                                           printf("\n扫描选项:\n");
                                           printf("  -p, --ports <范围>        端口范围 (默认: 1-1024)\n");
                                           printf("  -t, --threads <数量>      线程数量 (默认: 50)\n");
                                           printf("  -T, --timeout <毫秒>      超时时间 (默认: 2000)\n");
                                           printf("  -s, --scan-type <类型>    扫描类型: connect, syn, udp (默认: connect)\n");
                                           printf("  -b, --banner              启用横幅抓取\n");
                                           printf("  -v, --verbose             显示详细输出\n");
                                           printf("  -o, --output <文件>       输出文件\n");
                                           printf("  -f, --format <格式>       输出格式: txt, csv, json (默认: txt)\n");
                                           printf("  --no-banner               不显示横幅信息\n");
                                           return 0;
                                       }

                                       char *command = argv[0];

                                       if (strcmp(command, "scan") == 0) {
                                           if (argc < 2) {
                                               fprintf(stderr, "错误: 需要指定目标\n");
                                               return 1;
                                           }

                                           char *target = argv[1];
                                           char *port_range = "1-1024";
                                           int thread_count = 50;
                                           int timeout_ms = 2000;
                                           ScanType scan_type = SCAN_TCP_CONNECT;
                                           int banner_grab = 0;
                                           int verbose = 0;
                                           char *output_file = NULL;
                                           char *format = "txt";
                                           int show_banner = 1;

                                           // 解析选项
                                           for (int i = 2; i < argc; i++) {
                                               if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--ports") == 0) && i + 1 < argc) {
                                                   port_range = argv[++i];
                                               } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threads") == 0) && i + 1 < argc) {
                                                   thread_count = atoi(argv[++i]);
                                               } else if ((strcmp(argv[i], "-T") == 0 || strcmp(argv[i], "--timeout") == 0) && i + 1 < argc) {
                                                   timeout_ms = atoi(argv[++i]);
                                               } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--scan-type") == 0) && i + 1 < argc) {
                                                   char *type = argv[++i];
                                                   if (strcmp(type, "connect") == 0) {
                                                       scan_type = SCAN_TCP_CONNECT;
                                                   } else if (strcmp(type, "syn") == 0) {
                                                       scan_type = SCAN_TCP_SYN;
                                                   } else if (strcmp(type, "udp") == 0) {
                                                       scan_type = SCAN_UDP;
                                                   } else {
                                                       fprintf(stderr, "警告: 未知扫描类型 '%s'，使用默认connect\n", type);
                                                   }
                                               } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--banner") == 0) {
                                                   banner_grab = 1;
                                               } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                                                   verbose = 1;
                                               } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
                                                   output_file = argv[++i];
                                               } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) && i + 1 < argc) {
                                                   format = argv[++i];
                                               } else if (strcmp(argv[i], "--no-banner") == 0) {
                                                   show_banner = 0;
                                               }
                                           }

                                           // 执行扫描
                                           ScanResult *results = NULL;
                                           int result_count = 0;

                                           int ret = perform_scan(target, port_range, thread_count, timeout_ms,
                                                                  scan_type, banner_grab, verbose,
                                                                  &results, &result_count);

                                           if (ret == 0 && results) {
                                               // 显示结果
                                               display_results(results, result_count, show_banner);

                                               // 保存结果
                                               if (output_file) {
                                                   save_results(output_file, format, results, result_count, target);
                                               }

                                               // 释放结果内存
                                               free(results);
                                           }

                                           return (ret == 0) ? 0 : 1;

                                       } else if (strcmp(command, "help") == 0) {
                                           printf("端口扫描器帮助\n");
                                           printf("==============\n");
                                           printf("这是一个完全用的高性能端口扫描器。\n\n");
                                           printf("支持的扫描类型:\n");
                                           printf("  connect  - TCP连接扫描（最常用）\n");
                                           printf("  syn      - TCP SYN扫描（半开放扫描，需要root权限）\n");
                                           printf("  udp      - UDP扫描（速度较慢）\n\n");
                                           printf("端口范围格式:\n");
                                           printf("  单个端口: 80\n");
                                           printf("  端口范围: 1-1000\n");
                                           printf("  多个端口: 80,443,8080\n");
                                           printf("  混合格式: 1-100,443,8080-8088\n\n");
                                           printf("示例:\n");
                                           printf("  pentk port-scanner scan 192.168.1.1\n");
                                           printf("  pentk port-scanner scan example.com -p 1-65535 -t 100 -s syn\n");
                                           printf("  pentk port-scanner scan 10.0.0.1 -p 80,443,8080 -b -o result.json -f json\n");
                                           return 0;

                                       } else {
                                           fprintf(stderr, "错误: 未知命令 '%s'\n", command);
                                           return 1;
                                       }
                                   }

                                   // 获取帮助信息
                                   const char* port_scanner_get_help(void) {
                                       return
                                       "端口扫描器\n"
                                       "====================\n"
                                       "命令: scan <目标> [选项]\n\n"
                                       "选项:\n"
                                       "  -p, --ports <范围>    端口范围 (默认: 1-1024)\n"
                                       "  -t, --threads <数>    线程数 (默认: 50，最大: 200)\n"
                                       "  -T, --timeout <毫秒>  超时时间 (默认: 2000)\n"
                                       "  -s, --scan-type <类型> 扫描类型: connect, syn, udp\n"
                                       "  -b, --banner          启用横幅抓取\n"
                                       "  -v, --verbose         显示详细输出\n"
                                       "  -o, --output <文件>   输出到文件\n"
                                       "  -f, --format <格式>   输出格式: txt, csv, json\n"
                                       "  --no-banner           输出时不显示横幅信息\n\n"
                                       "注意: SYN扫描需要root权限\n";
                                   }

                                   // 获取插件函数
                                   void get_plugin_functions(PluginFunctions *funcs) {
                                       funcs->init = port_scanner_init;
                                       funcs->execute = port_scanner_execute;
                                       funcs->cleanup = port_scanner_cleanup;
                                       funcs->run_command = NULL;
                                       funcs->free_result = NULL;
                                       funcs->get_help = port_scanner_get_help;
                                   }
