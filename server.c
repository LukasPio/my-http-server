#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 10000
#define MAX_RESPONSE_LENGTH 4096
#define MAX_EMAIL_LENGTH 30
#define MAX_PASSWORD_LENGTH 100

typedef struct Route
{
    char *path;
    char *method;
    void (*handler)(void);
} Route;

typedef struct User
{
    char email[MAX_EMAIL_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
} User;

int server, client;

char* users_to_string(User* users, int count) {
    char* buffer = malloc(BUFFER_SIZE);
    if (!buffer) return NULL;

    buffer[0] = '\0';
    int used = 0;

    for (int i = 0; i < count; i++) {
        int written = snprintf(
            buffer + used,
            BUFFER_SIZE - used,
            "{'email':'%s','password':'%s'}",
            users[i].email,
            users[i].password
        );

        if (written < 0 || written >= BUFFER_SIZE - used) {
            break; // prevent overflow
        }

        used += written;
    }

    return buffer;
}

Route *create_route(char *path, char *method, void (*handler)(void))
{
    Route *route = (Route *)malloc(sizeof(Route));
    if (route == NULL)
    {
        printf("Occurred error initializing a route");
        exit(1);
    }

    route->handler = handler;
    route->method = method;
    route->path = path;

    return route;
}

User *create_user(char* email, char* password) {
    User *user = (User *)malloc(sizeof(User));
  
    if (user == NULL)
    {
        printf("Occurred error initializing an user");
        exit(1);
    }

    strncpy(user->email, email, MAX_EMAIL_LENGTH - 1);
    user->email[MAX_EMAIL_LENGTH - 1] = '\0';

    strncpy(user->password, password, MAX_PASSWORD_LENGTH - 1);
    user->password[MAX_PASSWORD_LENGTH - 1] = '\0';

    return user;
}

void throw_socket_err(char *responsible_func, int sock_to_close)
{
    close(sock_to_close);
    perror(responsible_func);
    exit(-1);
}

char *get_code_meaning(int code)
{
    switch (code)
    {
    case 200:
        return "OK";
        break;
    case 404:
        return "Not Found";
    default:
        return "No Content";
        break;
    }
}

char *build_response(int code, char *body, char* content_type)
{
    char *response = (char *)malloc(MAX_RESPONSE_LENGTH);
    char *code_meaning = get_code_meaning(code);
    snprintf(
        response,
        MAX_RESPONSE_LENGTH,
        "HTTP/1.1 %i %s\r\nContent-Length: %lu\r\nContent-Type: %s; charset=UTF-8\r\n\r\n%s",
        code,
        code_meaning,
        strlen(body),
        content_type,
        body);

    return response;
}

void write_response(int code, char *body, char* content_type)
{
    char *response = build_response(code, body, content_type);
    if (write(client, response, strlen(response)) < 0)
    {
        free(response);
        close(client);
        throw_socket_err("write", server);
    }
    free(response);
}

User *recovery_saved_users(int *count)
{
    FILE *fp = fopen("users.bin", "rb");

    if (!fp) {
        *count = 0;
        return NULL;
    }

    fseek(fp, 0, SEEK_END);

    long file_size = ftell(fp);
    rewind(fp);

    *count = file_size / sizeof(User);

    User *users = (User *) malloc(file_size);
    
    if (!users) {
        perror("malloc");
        fclose(fp);
        exit(1);
    }

    fread(users, sizeof(User), *count, fp);
    fclose(fp);

    return users;
}

void default_get_handler()
{
    write_response(200, "default endpoint", "text/plain");
}

void user_get_handler()
{
    int count;
    User* users = recovery_saved_users(&count);
    char* response = users_to_string(users, count);

    write_response(200, response, "application/json");

    free(response);
    free(users);
}

void user_post_handler()
{
}

Route *find_route(char *path, char *method)
{

    Route routes[] = {
        {"/", "GET", default_get_handler},
        {"/user", "GET", user_get_handler},
        {"/user", "POST", user_post_handler},
    };

    for (int i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
    {
        if (strcmp(path, routes[i].path) == 0 && strcmp(method, routes[i].method) == 0)
        {
            return create_route(routes[i].path, routes[i].method, routes[i].handler);
        }
    }

    return NULL;
}

void save_user(User *to_save)
{
    FILE *fp;
    fp = fopen("users.bin", "ab");

    if (fp == NULL)
    {
        printf("Occurred an error on user's file");
        exit(1);
    }

    fwrite(to_save, sizeof(User), 1, fp);
    fclose(fp);
}

int main()
{

    User* user = create_user("lucaspio.galvao@gmail.com", "29012008");
    save_user(user);

    int opt = 1;
    server = socket(AF_INET, SOCK_STREAM, 0);

    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));

    sock_addr.sin_port = htons(8080);
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server, (const struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
        throw_socket_err("bind", server);

    // FIX: listen fora do loop (era dentro)
    if (listen(server, 1) < 0)
        throw_socket_err("listen", server);

    while (1)
    {
        socklen_t sock_size = sizeof(sock_addr);
        client = accept(server, (struct sockaddr *)&sock_addr, &sock_size);

        if (client < 0)
        {
            close(client);
            throw_socket_err("accept", server);
        }

        char request[1024];

        if (read(client, request, sizeof(request)) < 0)
        {
            close(client);
            throw_socket_err("read", server);
        }

        char method[10];
        char path[64];

        sscanf(request, "%s %s", method, path);

        Route *route = find_route(path, method);

        if (route == NULL)
        {
            write_response(404, "This endpoint doesn't exists", "text/plain");
        }
        else
        {
            route->handler();
            free(route);
        }
    }

    return 0;
}