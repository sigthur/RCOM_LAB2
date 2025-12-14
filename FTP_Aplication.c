#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  
#include <stdlib.h>
#include <unistd.h>
#include <string.h>     
#include <netdb.h>     
#include <errno.h>

#define FTP_CONTROL_PORT 21
#define MAX_RESPONSE_SIZE 1024

#define DEFAULT_USER "anonymous"
#define DEFAULT_PASS "anonymous"

typedef struct {
    char user[50];
    char password[50];
    char host[100];
    char path[200];
    char filename[100];
} UrlInfo;


int parse_url(const char *url, UrlInfo *info) {
    char url_copy[256];
    strncpy(url_copy, url, 256);
    url_copy[255] = '\0';

    char *ptr = url_copy;

    if (strstr(ptr, "ftp://") != ptr) return -1;
    ptr += strlen("ftp://");

    char *arroba = strstr(ptr, "@");
    char *dois_pontos = strstr(ptr, ":");

    //logado
    if (arroba != NULL && dois_pontos != NULL && dois_pontos < arroba) {
        *dois_pontos = '\0';
        strncpy(info->user, ptr, sizeof(info->user) - 1);
        
        *arroba = '\0';
        strncpy(info->password, dois_pontos + 1, sizeof(info->password) - 1);

        ptr = arroba + 1;
    } 
    //nao logado
    else {
        strncpy(info->user, DEFAULT_USER, sizeof(info->user));
        strncpy(info->password, DEFAULT_PASS, sizeof(info->password));
    }

    // primeiro slash que divide informação host e url-path
    char *first_slash = strstr(ptr, "/");
    if (first_slash == NULL) return -1;
    *first_slash = '\0';
    strncpy(info->host, ptr, sizeof(info->host) - 1);

    strncpy(info->path, first_slash + 1, sizeof(info->path) - 1);

    // o filename está depois do último slash
    char *last_slash = strrchr(info->path, '/');
    if (last_slash == NULL) {
        strncpy(info->filename, info->path, sizeof(info->filename));
    } else {
        strncpy(info->filename, last_slash + 1, sizeof(info->filename));
    }

    info->user[sizeof(info->user) - 1] = '\0';
    info->host[sizeof(info->host) - 1] = '\0';


     
    printf("Download Values:\n");
    printf("User:              %s\n", info->user);
    printf("Password:          %s\n", info->password);
    printf("Host:              %s\n", info->host);
    printf("URL Path:          %s\n", info->path);

    
    return 0;
}

int write_command(int sockfd, const char *cmd) {
    int n = write(sockfd, cmd, strlen(cmd));
    if (n < 0) {
        perror("write()");
        exit(1);
    }
    printf("SENT: %s", cmd);  // já inclui \r\n no final
    return n;
}


int read_response(int sockfd, char *buffer) {
    int n = read(sockfd, buffer, MAX_RESPONSE_SIZE );
    if (n < 0) {
        perror("read()");
        exit(1);
    }
    buffer[n] = '\0';
    //printf("ok\n");
    return n;
}

int ftp_login(int sockfd, const UrlInfo *info) {
    char response[MAX_RESPONSE_SIZE];

    // 220 pronto para novo usuario
    read_response(sockfd, response);   
    if (strncmp(response, "220", 3) != 0) {
            printf("Error: expected 220.\n");
        exit(1);
    }
    printf("SERVER: %s", response);


    char cmd[100];
    snprintf(cmd, 100, "USER %s\r\n", info->user);
    write_command(sockfd, cmd);
    read_response(sockfd, response);
    if (strncmp(response, "331", 3) != 0) {
            printf("Erro: esperado 331.\n");
        exit(1);
    }
    printf("SERVER: %s", response);


    snprintf(cmd, 100, "PASS %s\r\n", info->password);
    write_command(sockfd, cmd);
    read_response(sockfd, response);
    if (strncmp(response, "230", 3) != 0) {
            printf("Erro: esperado 230.\n");
        exit(1);
    }
    printf("SERVER: %s", response);

    return 0;
}


int ftp_enter_passive_mode(int sockfd, char *data_ip, int *data_port) {
    char response[MAX_RESPONSE_SIZE];
    char cmd[100];


    snprintf(cmd, 100, "PASV\r\n");
    write_command(sockfd, cmd);
    read_response(sockfd, response);
    if (strncmp(response, "227", 3) != 0) {
            printf("Erro: esperado 227.\n");
        exit(1);
    }
    printf("SERVER: %s", response);


    int h1, h2, h3, h4, p1, p2;
    if (sscanf(response, "%*[^(](%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) < 6) {
        fprintf(stderr, "erro: nao foi possivel interpretar resposta PASV.\n");
        return -1;
    }

    snprintf(data_ip, 64, "%d.%d.%d.%d", h1, h2, h3, h4);
    *data_port = p1 * 256 + p2;

    printf("Modo passivo ativado: %s:%d\n", data_ip, *data_port);
    
    return 0;
}


int ftp_download_file(int sockfd, const UrlInfo *info) {
    char data_ip[64];
    int data_port;
    char response[MAX_RESPONSE_SIZE];
    char cmd[1024];

    if (ftp_enter_passive_mode(sockfd, data_ip, &data_port) != 0) {
        return -1;
    }

    int data_sockfd;
    struct sockaddr_in data_addr;

    if ((data_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() data");
        return -1;
    }


    snprintf(cmd, sizeof(cmd), "RETR /%s\r\n", info->path);
    write_command(sockfd, cmd);
    read_response(sockfd, response);
    bool flag = false;
    if (strncmp(response, "150", 3) == 0) {
        flag = true;
    }
    if (strncmp(response, "125", 3) == 0) {
        flag = true;
    }
    if (flag == false) {
        printf("Erro: esperado 150 ou 125.\n");
        exit(1);
    }
    printf("SERVER: %s", response);


    bzero((char *)&data_addr, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(data_port);
    data_addr.sin_addr.s_addr = inet_addr(data_ip);

    if (connect(data_sockfd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
        perror("connect() data");
        close(data_sockfd);
        return -1;
    }

    //printf("Conexao de dados (modo passivo) estabelecida.\n");

    FILE *fp = fopen(info->filename, "wb");
    if (fp == NULL) {
        perror("fopen()");
        close(data_sockfd);
        return -1;
    }

    char buffer[1024];
    int bytes_read;
    printf("Baixando ficheiro '%s'...\n", info->filename);
    while ((bytes_read = read(data_sockfd, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes_read, fp);
    }

    snprintf(cmd, sizeof(cmd), "QUIT\r\n");
    write_command(sockfd, cmd);

    fclose(fp);
    close(sockfd);
    close(data_sockfd);

    /*if (bytes_read < 0) {
        perror("read() data");
        return -1;
    }

    //read_response(sockfd, response);
    if (code != 226) {
        fprintf(stderr, "erro: transferencia incompleta (Code %d)\n", code);
        return -1;
    }*/

    printf("Download concluido com sucesso: %s\n", info->filename);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }
    
    UrlInfo url_data;
    if (parse_url(argv[1], &url_data) == -1) {
        fprintf(stderr, "Error: Invalid FTP URL format.\n");
        return -1;
    }

    struct hostent *h;
    if ((h = gethostbyname(url_data.host)) == NULL) {
        herror("gethostbyname()");
        return -1;
    }

    int sockfd;
    struct sockaddr_in server_addr;

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FTP_CONTROL_PORT);
    bcopy((char *)h->h_addr, (char *)&server_addr.sin_addr.s_addr, h->h_length);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        close(sockfd);
        return -1;
    }

    if (ftp_login(sockfd, &url_data) != 0) {
        close(sockfd);
        return 1;
    }

    printf("Fase 2 (Login) concluída com sucesso.\n");

    if (ftp_download_file(sockfd, &url_data) != 0) {
        fprintf(stderr, "erro ao baixar o ficheiro.\n");
        return 1;
    }

    printf("Conexao encerrada.\n");
    return 0;
}
