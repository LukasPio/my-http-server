#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 10000
#define MAX_RESPONSE_LENGTH 4096
#define MAX_REQUEST_LENGTH 8192
#define MAX_EMAIL_LENGTH 30
#define MAX_PASSWORD_LENGTH 100
#define SECRET "MJHv9HoJHjA3xuMf"
#define SALT "0TpBBufz8fXCimwXZ9ZuGt7Jfcv5zxO4"

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

enum ATTRIBUTE_TO_GET
{
    EMAIL,
    PASSWORD
};

int server, client;
char request[MAX_REQUEST_LENGTH];

char *togle_encrypt_decrypt(char *password, char *key)
{
    int pass_size = strlen(password);
    int key_size = strlen(key);

    char *result = malloc(pass_size * 2 + 1);
    if (!result)
        return NULL;

    for (int i = 0; i < pass_size; i++)
    {
        unsigned char xored = (unsigned char)password[i] ^ (unsigned char)key[i % key_size];
        sprintf(result + i * 2, "%02x", xored);
    }
    result[pass_size * 2] = '\0';
    return result;
}

char *encrypt_with_salt(char *password)
{
    int key_len = strlen(SECRET) + strlen(SALT);

    char *key = malloc(key_len + 1);
    if (!key)
        return NULL;

    strcpy(key, SECRET);
    strcat(key, SALT);

    char *result = togle_encrypt_decrypt(password, key);

    free(key);
    return result;
}

char *users_to_json(User *users, int count)
{
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
        return NULL;

    buffer[0] = '\0';
    int used = 0;

    for (int i = 0; i < count; i++)
    {
        int written = snprintf(
            buffer + used,
            BUFFER_SIZE - used,
            "{\"email\":\"%s\",\"password\":\"%s\"}",
            users[i].email,
            users[i].password);

        if (written < 0 || written >= BUFFER_SIZE - used)
        {
            break;
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

User *create_user(char *email, char *password)
{
    User *user = (User *)malloc(sizeof(User));

    if (user == NULL)
    {
        printf("Occurred error initializing an user");
        exit(1);
    }

    strncpy(user->email, email, MAX_EMAIL_LENGTH - 1);
    user->email[MAX_EMAIL_LENGTH - 1] = '\0';

    char *enc = encrypt_with_salt(password);
    strncpy(user->password, enc, MAX_PASSWORD_LENGTH - 1);
    user->password[MAX_PASSWORD_LENGTH - 1] = '\0';
    free(enc);

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
        break;
    case 401:
        return "Unauthorized";
    default:
        printf("Invalid code was provided");
        exit(1);
        break;
    }
}

char *build_response(int code, char *body, char *content_type)
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

void write_response(int code, char *body, char *content_type)
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

    if (!fp)
    {
        *count = 0;
        return NULL;
    }

    fseek(fp, 0, SEEK_END);

    long file_size = ftell(fp);
    rewind(fp);

    *count = file_size / sizeof(User);

    User *users = (User *)malloc(file_size);

    if (!users)
    {
        perror("malloc");
        fclose(fp);
        exit(1);
    }

    fread(users, sizeof(User), *count, fp);
    fclose(fp);

    return users;
}

int login(char *email, char *password)
{
    int count;
    User *all_users = recovery_saved_users(&count);
    User *current;
    for (int i = 0; i < count; i++)
    {
        current = &all_users[i];
        if (strcmp(current->email, email) == 0 && strcmp(current->password, encrypt_with_salt(password)) == 0)
        {
            return 1;
        }
    }
    return 0;
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

void default_get_handler()
{
    write_response(200, "default endpoint", "text/plain");
}

void user_get_handler()
{
    int count;
    User *users = recovery_saved_users(&count);
    char *response = users_to_json(users, count);

    if (users == NULL) {
        write_response(200, "There are no saved users", "plain/text");
    }

    else {
        write_response(200, response, "application/json");
    }

    free(response);
    free(users);
}

char *get_attribute_from_string(char *string, enum ATTRIBUTE_TO_GET attribute)
{

    char *to_find = attribute == PASSWORD ? "\"password\":" : "\"email\":";

    char *body = strstr(string, "\r\n\r\n");
    if (!body)
        return NULL;
    body += 4;

    char *content = strstr(body, to_find);
    if (!content)
        return NULL;
    content += strlen(to_find);
    while (*content == ' ')
        content++;

    if (*content != '"')
        return NULL;
    char *start = content + 1;

    int length = 0;
    while (start[length] != '\0' && start[length] != '"')
    {
        length++;
    }

    char *result = malloc(length + 1);
    if (!result)
        return NULL;

    for (int i = 0; i < length; i++)
    {
        result[i] = start[i];
    }
    result[length] = '\0';

    return result;
}

void user_login_post_handler()
{
    char *email = get_attribute_from_string(request, EMAIL);
    char *password = get_attribute_from_string(request, PASSWORD);
    char *encrypted_password = encrypt_with_salt(password);

    int count;
    User *all_users = recovery_saved_users(&count);

    for (int i = 0; i < count; i++)
    {
        User current = all_users[i];
        if (strcmp(email, current.email) == 0 && strcmp(encrypted_password, current.password) == 0)
        {
            write_response(200, "login feito com sucesso", "text/plain");
            return;
        }
    }
    write_response(401, "Unauthorized", "text/plain");
}

Route *find_route(char *path, char *method)
{

    Route routes[] = {
        {"/", "GET", default_get_handler},
        {"/user", "GET", user_get_handler},
        {"/user/login", "POST", user_login_post_handler},
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

int main()
{

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
            close(client);
        }
    }

    return 0;
}