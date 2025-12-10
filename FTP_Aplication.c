#include <stdio.h>
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
#define DEFAULT_PASS "user@example.com"

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

    // primeiro slash que divide iformaçao host e url-path
    char *first_slash = strstr(ptr, "/");
    if (first_slash == NULL) return -1;
    *first_slash = '\0';
    strncpy(info->host, ptr, sizeof(info->host) - 1);

    strncpy(info->path, first_slash + 1, sizeof(info->path) - 1);

    //o filename esta depois do ultimo slash
    char *last_slash = strrchr(info->path, '/');
    if (last_slash == NULL) {
        strncpy(info->filename, info->path, sizeof(info->filename));
    } else {
        strncpy(info->filename, last_slash + 1, sizeof(info->filename));
    }

    // para segura null termination
    info->user[sizeof(info->user) - 1] = '\0';
    info->host[sizeof(info->host) - 1] = '\0';
    
    return 0;

}

int write_command(int sockfd, const char *command) {
    char cmd_buf[256];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", command);
    
    printf(">> %s", cmd_buf);

    size_t len = strlen(cmd_buf);
    if (write(sockfd, cmd_buf, len) != len) {
        perror("write_command()");
        return -1;
    }
    return 0;
}

int read_response(int sockfd, char *full_response) {
    char *ptr = full_response;
    int bytes_read;
    int code = 0;
    
    full_response[0] = '\0';

    do {
        // Tenta ler um bloco de dados
        bytes_read = read(sockfd, ptr, MAX_RESPONSE_SIZE - (ptr - full_response) - 1);
        if (bytes_read <= 0) {
            if (bytes_read == 0) fprintf(stderr, "Server closed control connection unexpectedly.\n");
            else perror("read()");
            return -1; // Sai se falhar
        }   
        ptr[bytes_read] = '\0';
        printf("<< %s", ptr);

        // Verifica o código ftp

        if (sscanf(full_response, "%d", &code) == 1) {
            char *last_line = strrchr(full_response, '\n'); 
            if (last_line && (last_line - full_response) >= 4 && last_line[-4] == ' ') {
                 return code; 
            }
        }
        ptr += bytes_read; 
    } while ((ptr - full_response) < MAX_RESPONSE_SIZE - 1);
    
    fprintf(stderr, "Error\n");
    return -1;
}

int ftp_login(int sockfd, const UrlInfo *info) {
    char response[MAX_RESPONSE_SIZE];
    int code;

    // 220 pronto para novo usuario
    code = read_response(sockfd, response);
    if (code != 220) {
        fprintf(stderr, "eroo: nao está pronto para login (Code %d)\n", code);
        return -1;
    }
    
    char user_cmd[100];
    snprintf(user_cmd, 100, "USER %s", info->user);
    if (write_command(sockfd, user_cmd) == -1) return -1;
    
    // 3. Ler resposta
    code = read_response(sockfd, response);
    
    if (code == 230) {
        printf("Login anonimo.\n");
        return 0;
    } else if (code != 331) {
        fprintf(stderr, "erro: login (Code %d)\n", code);
        return -1;
    }

    // 331 - user ok pass falta
    char pass_cmd[100];
    snprintf(pass_cmd, 100, "PASS %s", info->password);
    if (write_command(sockfd, pass_cmd) == -1) return -1;

    // 230 login com sucesso
    code = read_response(sockfd, response);
    if (code == 230) {
        printf("login feito.\n");
        return 0;
    } else {
        fprintf(stderr, "erro: pass (Code %d)\n", code);
        return -1;
    }
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
        fprintf(stderr,"erro");
        return -1;
    }

    //printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

    int sockfd = -1;
    struct sockaddr_in server_addr;

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FTP_CONTROL_PORT);

    // ip resolvido pelo dns h->h_adress
    bcopy((char *)h->h_addr, (char *)&server_addr.sin_addr.s_addr, h->h_length);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    if (connect(sockfd,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0) {
        perror("connect()");
        close(sockfd);
        return -1;
    }


    if (ftp_login(sockfd, &url_data) != 0) {
        close(sockfd);
        return 1;
    }
    printf("Fase 2 (Login) concluída com sucesso.\n");
    write_command(sockfd, "QUIT");
    close(sockfd);


    return 0;
}

