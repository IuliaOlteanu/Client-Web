#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include <ctype.h>

// interactiunea cu serverul - date de conectare
#define HOST "34.241.4.235"
#define PORT 8080
#define MAX_LEN 50

typedef struct Elem {
    char username[MAX_LEN];
    char password[MAX_LEN];
}elem;

typedef struct Book {
    char title[MAX_LEN];
    char author[MAX_LEN];
    char genre[MAX_LEN];
    char publisher[MAX_LEN];
    char page_count[MAX_LEN];
}book;

// functie similara cu compute_get_request preluata
// din laborator si extrem de putin modificata astfel incat sa 
//se tina cont de token
char *compute_delete(char *host, char *url, char *query_params,
                        char **cookies, int cookies_count)
{
    char *message = calloc(BUFLEN, sizeof(char));
    char *line = calloc(LINELEN, sizeof(char));

    // Step 1: write the method name, URL, request params (if any) and protocol type
    if (query_params != NULL) {
        sprintf(line, "DELETE %s?%s HTTP/1.1", url, query_params);
    } else {
        sprintf(line, "DELETE %s HTTP/1.1", url);
    }

    compute_message(message, line);

    // Step 2: add the host
    memset(line, 0, LINELEN);
    sprintf(line, "Host: %s", host);
    compute_message(message, line);


    // Step 3 (optional): add headers and/or cookies, according to the protocol format
    if (cookies != NULL) {
       memset(line, 0, LINELEN);
       strcat(line, "Authorization: Bearer ");
       for (int i = 0; i < cookies_count - 1; i++) {
            strcat(line, cookies[i]);
            strcat(line, ";");
        }
        strcat(line, cookies[cookies_count - 1]);
        compute_message(message, line);
    }

    // Step 4: add final new line
    compute_message(message, "");
    return message;
}

// verificare corectitudine date
int corect(char id[]) {
    int i;
    int len = strlen(id);
    for(i = 0; i < len; i++) {
        if(isdigit(id[i]) == 0) 
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    char *message;
    char *response;
    int sockfd;
    // delimitatori
    const char sep1[2] = ";";
    const char sep2[2] = ":";
    const char sep3[3] = "\"";
    const char tok[6] = "token";

    // sir de caractere pt comenzile
    // primite de la tastatura : register/login/enter_library/
    // get_books/get_book/add_book/delete_book/logout/exit
    char command[10 * MAX_LEN];
    char **cookies;

    char aux[4 * MAX_LEN];
    // alocare spatiu pt cookies
    cookies = (char **)calloc(1, sizeof(char*));
    cookies[0] = (char *)calloc(5 * MAX_LEN, sizeof(char));

    char route[4 * MAX_LEN];
    char route2[4 * MAX_LEN];

    // alocare spatiu pt tokens
    char **tokens = (char **)calloc(1, sizeof(char *));
	tokens[0] = (char *)calloc(5 * MAX_LEN, sizeof(char));
    // indicator pt a se tine cont de login/logout
    // nu se pot da 2 comenzi de login daca nu a fost
    // data o comanda de logout dupa un login
    int ok = 0;

    // indicator pt a se tine cont daca username-ul curent
    // are acces la biblioteca
    int acces_library = 0;

    while(1) {
        memset(command, 0, MAX_LEN);
        // citire comanda de la tastatura
        fgets(command, MAX_LEN - 1, stdin);
        command[strlen(command) - 1] = '\0';
        
        // primire comanda de tipul register
        if(!strncmp(command, "register", strlen("register"))) {
            // deschidere conexiune cu serverul
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
            elem e;
            char payload[4 * MAX_LEN];
            // se ofera prompt pentru username şi password

            // citire username de la tastatura
            printf("username=");
            fgets(e.username, MAX_LEN - 1, stdin);
            e.username[strlen(e.username) - 1] = '\0';

            // citire password de la tastatura
            printf("password=");
            fgets(e.password, MAX_LEN - 1, stdin);
            e.password[strlen(e.password) - 1] = '\0';
            
            sprintf(payload, "{\"username\":\"%s\",\"password\":\"%s\"}", e.username, e.password);
            // ruta de acces : POST /api/v1/tema/auth/register
            // tip payload : application/json
            message = compute_post_request(1, HOST, "/api/v1/tema/auth/register", "application/json", payload, NULL, 0);

            // trimitere mesaj catre server pe socketul deschis mai sus
            send_to_server(sockfd, message);
            
            // verificare raspuns primit, eroare daca username-ul este deja folosit
            response = receive_from_server(sockfd);
            // usename-ul nu este deja folosit
            if(strstr(response, "400") == NULL) {
                // afisare mesaj corespunzator
                printf("201 - OK - Successfully registered user!\n");
            } else {
                // afisare mesaj de eroare
                printf("Username already used!\n");
            }
            // inchidere conexiune
            close_connection(sockfd);
        }

        // primire comanda de tipul login
        else if (!strncmp(command, "login", strlen("login"))) {
            if(ok == 0) {
                // deschidere conexiune cu serverul
                sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
                
                elem e;
                char payload[4 * MAX_LEN];
                // se ofera prompt pentru username şi password

                // citire username de la tastatura
                printf("username=");
                fgets(e.username, MAX_LEN - 1, stdin);
                e.username[strlen(e.username) - 1] = '\0';

                // citire password de la tastatura
                printf("password=");
                fgets(e.password, MAX_LEN - 1, stdin);
                e.password[strlen(e.password) - 1] = '\0';

                sprintf(payload, "{\"username\":\"%s\",\"password\":\"%s\"}", e.username, e.password);
                // ruta de acces : POST /api/v1/tema/auth/login
                // tip payload : application/json
                
                message = compute_post_request(1, HOST, "/api/v1/tema/auth/login", "application/json", payload, NULL, 0);
                
                
                // trimitere mesaj catre server pe socketul deschis mai sus
                send_to_server(sockfd, message);
                // verificare raspuns primit, cookie de sesiune
                response = receive_from_server(sockfd);
                
                // eroare : credentialele nu se potrivesc!
                if(strstr(response, "Credentials") != NULL) {
                    // afisare mesaj de eroare
                    printf("Credentials do not match!\n");
                    ok = 0;
                } else {
                    // credentialele se potrivesc
                    printf("200 - OK - Welcome %s!\n", e.username);
                    ok = 1;
                }
                char *aux1 = strtok(response, sep1);
                aux1 = strtok(NULL, sep1);
                aux1 = strtok(NULL, sep1);
                strcpy(aux, aux1);
                aux1 = strtok(aux, sep2);
                aux1 = strtok(NULL, sep1);
                strcpy(aux, aux1 + 1);
                strcpy(cookies[0], aux);

                // inchidere conexiune
                close_connection(sockfd);
            } else {
                printf("Login already done!\n");
            }
        }
        // primire comanda de tipul enter_library
        else if(!strncmp(command, "enter_library", strlen("enter_library"))) {
            if(ok == 1) {
                // deschidere conexiune cu serverul
                sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
                
                // ruta de acces : GET /api/v1/tema/library/access
                message = compute_get_request(1, HOST, "/api/v1/tema/library/access", NULL, cookies, 1);
                    
                // trimitere mesaj catre server pe socketul deschis mai sus
                send_to_server(sockfd, message);
                    
                // verificare raspuns primit, token JWT
                // eroare daca username-ul nu este autentificat
                response = receive_from_server(sockfd);
                
                if(strstr(response, "error") == NULL) {
                    printf("200 - OK - The library is accessed!\n");
                    acces_library = 1;
                } else {
                    printf("Something bad happened\n");
                    acces_library = 0;
                }
                char *aux2 = strstr(response, tok);
                char *aux3 = strtok(aux2, sep3);
                aux3 = strtok(NULL, sep3);
                aux3 = strtok(NULL, sep3);
                strcpy(tokens[0], aux3);

                // inchidere conexiune
                close_connection(sockfd);
                
            } else {
                printf("You need to log in first\n");
            }
        }
        // primire comanda de tipul get_books
        else if(!strncmp(command, "get_books", strlen("get_books"))) {
            // acesta comanda poate fi realizata cu success
            // daca utilizatorul curent are acces la biblioteca
            if(acces_library == 1) {
                // deschidere conexiune cu serverul
                sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
                
                // ruta de acces : GET /api/v1/tema/library/books
                message = compute_get_request(2, HOST, "/api/v1/tema/library/books", NULL, tokens, 1);
                
                // trimitere mesaj catre server pe socketul deschis mai sus
                send_to_server(sockfd, message);

                // primire raspuns de la server
                response = receive_from_server(sockfd);
                if(strstr(response, "error") != NULL) {
                    printf("Something bad happened!\n");
                } else {
                    printf("%s\n", response);
                }
                continue;

                // inchidere conexiune
                close_connection(sockfd);
            } else {
                printf("Access denied, enter library first!\n");
            }
        }
        // primire comanda de tipul get_book
        else if(!strncmp(command, "get_book", strlen("get_book"))) {
            // acesta comanda poate fi realizata cu success
            // daca utilizatorul curent are acces la biblioteca
            if(acces_library == 1) {
                // deschidere conexiune cu serverul
                sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
                
                // se ofera prompt pentru campurile din structura
                char id[MAX_LEN];
                // citire id de la tastatura
                printf("id=");
                scanf("%s", id);

                // verificare id este numar intreg
                if(corect(id) == 0) {
                    printf("Id needs to be a number!\n");
                } else {
                    // ruta de acces : GET /api/v1/tema/library/books/:bookId
                    sprintf(route, "/api/v1/tema/library/books/%s", id);

                    message = compute_get_request(2, HOST, route, NULL, tokens, 1);
                
                    // trimitere mesaj catre server pe socketul deschis mai sus
                    send_to_server(sockfd, message);

                    // primire raspuns de la server
                    response = receive_from_server(sockfd);

                    if(strstr(response, "error") != NULL) {
                        printf("Error, no book with id %s was found!\n", id);
                    } else {
                        printf("%s\n", response);
                    }
                    continue;
                }
                // inchidere conexiune
                close_connection(sockfd);
            } else {
                printf("Access denied, enter library first!\n");
            }
        }
        // primire comanda de tipul add_book
        else if(!strncmp(command, "add_book", strlen("add_book"))) {
            if(acces_library == 1) {
                // deschidere conexiune cu serverul
                sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
                
                book carte;
                char payload[10 * MAX_LEN];
                // se ofera prompt pentru campurile din structura

                // citire title de la tastatura
                printf("title=");
                fgets(carte.title, MAX_LEN - 1, stdin);

                // citire author de la tastatura
                printf("author=");
                fgets(carte.author, MAX_LEN - 1, stdin);

                // citire genre de la tastatura
                printf("genre=");
                fgets(carte.genre, MAX_LEN - 1, stdin);

                // citire publisher de la tastatura
                printf("publisher=");
                fgets(carte.publisher, MAX_LEN - 1, stdin);

                // citire page_count de la tastatura
                printf("page_count=");
                fgets(carte.page_count, MAX_LEN - 1, stdin);

                carte.title[strlen(carte.title) - 1] = '\0';
                carte.author[strlen(carte.author) - 1] = '\0';
                carte.genre[strlen(carte.genre) - 1] = '\0';
                carte.publisher[strlen(carte.publisher) - 1] = '\0';
                carte.page_count[strlen(carte.page_count) - 1] = '\0';

                // verificare corectitudine date
                // page_count este un numar

                if(corect(carte.page_count) == 0) {
                    printf("Page_count needs to be a number!\n");
                } else {

                    sprintf(payload, "{\"title\":\"%s\",\"author\":\"%s\",\"genre\":\"%s\",\"page_count\":\"%s\",\"publisher\":\"%s\"}", 
                    carte.title, carte.author, carte.genre, carte.page_count, carte.publisher);

                    // ruta de acces : POST /api/v1/tema/library/books
                    // tip payload : application/json
                    message = compute_post_request(2, HOST, "/api/v1/tema/library/books", 
                    "application/json", payload, tokens, 1);

                    // trimitere mesaj catre server pe socketul deschis mai sus
                    send_to_server(sockfd, message);

                    // primire raspuns de la server
                    response = receive_from_server(sockfd);

                    if(strstr(response, "error") == NULL) {
                        printf("A new book is inserted to library!\n");
                    }else {
                        printf("Error trying to add a new book!\n");
                    }
                }
                // inchidere conexiune
                close_connection(sockfd);
                
            } else {
                printf("Access denied, enter library first!\n");
            }
        }
        // primire comanda de tipul delete_book
        else if(!strncmp(command, "delete_book", strlen("delete_book"))) {
            // comanda poate fi realizata numai daca
            // username-ul are acces la biblioteca
            if(acces_library == 1) {
                // deschidere conexiune cu serverul
                sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
                
                // se ofera prompt pentru id-ul unei carti
                char id[MAX_LEN];
                // citire id de la tastatura
                printf("id=");
                fgets(id, MAX_LEN - 1, stdin);
                id[strlen(id) - 1] = '\0';

                // verificare corectitudine date
                if(corect(id) == 0) {
                    printf("Id needs to be a number!\n");
                } else {

                    // ruta de acces : DELETE /api/v1/tema/library/books/:bookId
                    sprintf(route2, "/api/v1/tema/library/books/%s", id);

                    message = compute_delete(HOST, route2, NULL, tokens, 1);
                
                    send_to_server(sockfd, message);
                    response = receive_from_server(sockfd);

                    if(strstr(response, "error") != NULL) {
                        printf("Error trying to delete the book with id %s!\n", id);
                    } else{
                        printf("The book with id %s was removed successfully from library!\n", id);
                    }
                }
                // inchidere conexiune
                close_connection(sockfd);
            } else {
                printf("You need to enter library first!\n");
            }
        }
        // primire comanda de tipul logout
        else if(!strncmp(command, "logout", strlen("logout"))) {
            if(ok == 1) {
                // deschidere conexiune cu serverul
                sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
                
                // ruta de accses : GET /api/v1/tema/auth/logout
                message = compute_get_request(1, HOST, "/api/v1/tema/auth/logout", NULL, cookies, 1);
                
                // trimitere mesaj catre server pe socketul deschis mai sus
                send_to_server(sockfd, message);
                
                // verificare raspuns primit,
                // eroare daca username-ul nu este autentificat
                response = receive_from_server(sockfd);
                if(strstr(response, "error") != NULL) {
                    printf("Something bad happened!\n");
                } else {
                    printf("200 - OK - Logging out!\n");
                    ok = 0;
                }
                
                if(acces_library == 1)
                    acces_library = 0;

                // inchidere conexiune
                close_connection(sockfd);
            } else {
                printf("You need to log in first!\n");
            }
        }
        // primire comanda de tipul exit
        else if(!strncmp(command, "exit", strlen("exit")))
            break;

    }
    return 0;
}
