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
    if ((h = gethostbyname(argv[1])) == NULL) {
        herror("gethostbyname()");
        fprintf(stderr,"erro");
        return -1;
    }

    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

    printf("--- URL Parsed Successfully ---\n");
    printf("User: %s\n", url_data.user);
    printf("Pass: %s\n", url_data.password);
    printf("Host: %s\n", url_data.host);
    printf("Path: %s\n", url_data.path);
    printf("File: %s\n", url_data.filename);

    return 0;
}