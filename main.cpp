#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pthread.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include "string"
#include "sstream"
#include "iostream"

// Joan Omley
// CSPC 5042 week4
// Referenced from https://www.codingninjas.com/codestudio/library/learning-socket-programming-in-c
// and https://www.youtube.com/watch?v=Pg_4Jz8ZIH4 with professor Riley's split() function.

using namespace std;

#define TRUE   1
#define PORT 8080
#define SERVER_BACKLOG 1
#define BUFF_SIZE 1025

int pipe_fds[2];

vector<string> split(string& src, char delim);
void *processSocket(void *params);
void *runLogger(void *params);

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

typedef struct {
    string key;
    string value;
} http_header_t;

typedef struct {
    string verb;
    string version;
    string uri;
    vector<http_header_t> headers;
} http_request_t;

int main(int argc , char *argv[])
{
    int server_socket, client_socket, addr_size;
    SA_IN server_addr, client_addr;

    if( (server_socket = socket(AF_INET, SOCK_STREAM,0)) == 0)
    {
        perror("Failed_Socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (SA*)&server_addr, sizeof(server_addr))<0)
    {
        perror("Failed_Bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SERVER_BACKLOG) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    addr_size = sizeof(server_addr);
//    ::printf("Listening on port %i\n",PORT);

    pthread_t logger;
    pthread_create(&logger, 0, runLogger, NULL);

    while (TRUE) {

        if ((client_socket = accept(server_socket, (SA*)&client_addr, (socklen_t*)&addr_size))<0)
        {
            perror("Accept!");
            exit(EXIT_FAILURE);
        }

        printf("Connected!\n");
        pthread_t t;
        int *pclient = &client_socket;
        pthread_create(&t,NULL, processSocket, pclient);
    }
    close(client_socket);
    return 0;
}

vector<string> split(string& src, char delim) {
    istringstream ss(src);
    vector<string> res;

    string piece;
    while(getline(ss, piece, delim)) {
        res.push_back(piece);
    }

    return res;
}

void * processSocket(void* socket) {
    int client_socket = *((int*)socket);
    char buffer[1025];

    int value_read = read( client_socket, buffer, 1024);
    string s_req = "";
    http_request_t req;
    for (int i = 0; i < sizeof(buffer); i++) {
        s_req += buffer[i];
    }

    vector<string> verb = split(s_req, ' ');
    vector<string> vers = split(verb[2], '/');
    vector<string> text = split(s_req, '\n');

    req.verb = verb[0];
    req.uri = verb[1];
    req.version = split(vers[1],'\n')[0];
    int size = text[0].size();
//    printf("Received %i bytes on socket %i: %s %s version: %s \n",size,client_socket, req.verb.c_str(),req.uri.c_str(),req.version.c_str());

    string message = "Received " + to_string(size) + " bytes on socket " + to_string(client_socket) + ": " +
            req.verb + " " + req.uri + " " + req.version + "\n";
    int message_size = message.length();
    write(pipe_fds[1],message.c_str(), message_size + 1);
    if (req.uri == "/quit") {
        cout << "Shutting down\n";
        sleep(1);
        exit(0);
    }

    return NULL;
}

void *runLogger(void *params) {
    ::printf("Listening on port %i\n",PORT);
    char buffer[BUFF_SIZE];
    ssize_t bytes = read(pipe_fds[0],buffer, sizeof(buffer));
    while (TRUE) {
        int i = 0;
        while (i < bytes) {
            ::printf("%c",buffer[i]);
            i++;
        }
    }
}