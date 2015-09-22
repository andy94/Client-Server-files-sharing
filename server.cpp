/******************************************************************************
 * Tema        : 3 - PC                                                       *
 * Autor       : Andrei Ursache                                               *
 * Grupa       : 322 CA                                                       *
 * Data        : 1.05.2015                                                    *
 ******************************************************************************/

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "aux.hpp"

/*
 *  USAGE: ./server  <port_server>
 *                  0           1
 */

#define MAX_CLIENTS    20
#define BUFLEN 1024
#define forever while(1)

using namespace std;

int main(int argc, char *argv[]) {

    /* Declarare socketi, adrese si buffere */
    int initial_sock_fd, new_sock_fd, port_no;
    socklen_t clilen;
    char buffer[BUFLEN];
    struct sockaddr_in server_addr, client_addr;
    int n, i;

    fd_set read_fds;
    fd_set temp_fds;

    int fdmax;

    /* Apelare gresita */
    if (argc != 2) {
        error("Usage: ./server  <port_server>");
    }

    /* Multimile de descriptori pentru select */
    FD_ZERO(&read_fds);
    FD_ZERO(&temp_fds);

    /* Creearea socketului initilal ******************************************/
    initial_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (initial_sock_fd < 0) {
        error("ERROR opening socket");
    }

    /* Portul pe care va asculta socketul */
    port_no = atoi(argv[1]);

    /* Initializare server_addr */
    memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_no);

    /* Bind pe initial_sock_fd */
    if (bind(initial_sock_fd, (struct sockaddr *) &server_addr,
            sizeof(struct sockaddr)) < 0) {
        error("ERROR on binding");
    }

    /* initial_sock_fd va fi pe listen pentru noile conexiuni */
    listen(initial_sock_fd, MAX_CLIENTS);

    /* Setam socketul initial_sock_fd in multimea de citire */
    FD_SET(initial_sock_fd, &read_fds);
    /* si de la tastatura */
    FD_SET(0, &read_fds);
    /* maximul curent */
    fdmax = initial_sock_fd;

    /* Instantieri */
    vector<Client*> clients; // lista de clienti

    Error_Type::instantiateErrors();
    Command_Arguments::instantiateCommands();

    printf("Server open. Type [quit] for close.\n");

    /*Bucla infinita *********************************************************/
    forever {
        temp_fds = read_fds;
        if (select(fdmax + 1, &temp_fds, NULL, NULL, NULL) == -1) {
            error("ERROR in select");
        }

        /* Pentru fiecare descriptor in parte */
        for (i = 0; i <= fdmax; i++) {

            /* Daca e setat */
            if (FD_ISSET(i, &temp_fds)) {

                /* Daca e cel initial -> nou client */
                if (i == initial_sock_fd) {
                    clilen = sizeof(client_addr);

                    if ((new_sock_fd = accept(initial_sock_fd,
                            (struct sockaddr *) &client_addr, &clilen)) == -1) {
                        printf("\x1b[32mserver> \x1b[0m");
                        printf("Error in accept client\n");

                    } else {
                        /* Daca a decurs bine acceptul*/

                        /* Primire nume */
                        memset(buffer, 0, BUFLEN);
                        n = recv(new_sock_fd, buffer, sizeof(buffer), 0);

                        /* Daca este client existent */
                        if (exist(clients, buffer)) {
                            n = send(new_sock_fd, "reject: client name exists",
                                    strlen("reject: client name exists"), 0);

                            printf("\x1b[32mserver> \x1b[0m");
                            printf(
                                    "New connection [%s] rejected: client name exists\n",
                                    buffer);

                        } else {
                            /*Daca este client nou */

                            /* Trimite accept */
                            n = send(new_sock_fd, "accept", strlen("accept"),
                                    0);
                            char clientName[strlen(buffer) + 1];
                            memcpy(clientName, buffer, strlen(buffer) + 1);

                            /* Primire port */
                            memset(buffer, 0, BUFLEN);
                            n = recv(new_sock_fd, buffer, sizeof(buffer), 0);

                            /* Seteaza descriptorul */
                            FD_SET(new_sock_fd, &read_fds);
                            if (new_sock_fd > fdmax) {
                                fdmax = new_sock_fd;
                            }

                            /* Creeaza si adauga un nou client */
                            Client *newClient = new Client(new_sock_fd,
                                    clientName, inet_ntoa(client_addr.sin_addr),
                                    atoi(buffer));
                            clients.push_back(newClient);

                            printf("\x1b[32mserver> \x1b[0m");
                            printf(
                                    "New connection from [%s] [%s] [%d] has fd [%d]\n",
                                    newClient->getName().c_str(),
                                    newClient->getIpChar().c_str(),
                                    newClient->getPortNoInt(),
                                    newClient->getFd());
                            memset(buffer, 0, BUFLEN);
                        }
                    }

                } else {
                    /* Daca e un descriptor de la ceilalti clienti conectati */
                    memset(buffer, 0, BUFLEN);

                    /* Daca primeste de la tastatura -> QUIT */
                    if (i == 0) {

                        fgets(buffer, BUFLEN - 1, stdin);
                        /* Daca e quit se va iesi */
                        if (strcmp(buffer, "quit\n") == 0) {
                            /* Trimite mesaj tuturor clientilor */
                            for (unsigned int index = 0; index < clients.size();
                                    ++index) {
                                n = send(clients[index]->getFd(), "quit",
                                        strlen("quit"), 0);
                            }

                            FD_ZERO(&read_fds);
                            FD_ZERO(&temp_fds);
                            close(initial_sock_fd);
                            return 0;
                        }

                    } else {
                        /* Daca e de la un client */

                        /* Daca a iesit brusc */
                        if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {

                            /* Sterge client */
                            Client* client_gone = getClientByFd(clients, i);
                            printf("\x1b[32mserver> \x1b[0m");
                            printf(
                                    "Client with name [%s] fd [%d] exit without announcing\n",
                                    client_gone->getName().c_str(), i);

                            deleteClient(clients, client_gone);

                            close(i);
                            FD_CLR(i, &read_fds);

                        } else {
                            /* Daca vrea o comanda */

                            Command_Arguments command(buffer);

                            switch (command.getCommandCode()) {

                            /* Infolcients ***********************************/
                            case 0: {
                                printf("\x1b[32mserver> \x1b[0m");
                                cout << "Send infoclients to ["
                                        << getClientByFd(clients, i)->getName()
                                        << "]\n";

                                string info = "";

                                /* Se creeaza o singura lista cu toate informatiile */
                                for (unsigned int index = 0;
                                        index < clients.size(); ++index) {
                                    info +=
                                            (clients[index]->getName() + " "
                                                    + clients[index]->getIpChar()
                                                    + " "
                                                    + int_to_string(
                                                            clients[index]->getPortNoInt())
                                                    + " ");
                                }

                                /* Trimite lista */
                                n = send(i, info.c_str(), info.size() + 1, 0); // trimitere lista
                                memset(buffer, 0, BUFLEN);

                                break;
                            }

                                /* Getshare **********************************/
                            case 1: {

                                n = recv(i, buffer, sizeof(buffer), 0);
                                Client* who = getClientByName(clients, buffer);

                                /* Daca nu a fost gasit */
                                if (who == NULL) {
                                    printf("\x1b[32mserver> \x1b[0m");
                                    printf("Client with name [%s] is unknown\n",
                                            buffer);
                                    memset(buffer, 0, BUFLEN);
                                    memcpy(buffer, "-3", sizeof("-3"));

                                } else {
                                    /* Daca exista */
                                    printf("\x1b[32mserver> \x1b[0m");
                                    printf(
                                            "Client [%s] getshare for client [%s]\n",
                                            getClientByFd(clients, i)->getName().c_str(),
                                            buffer);
                                    vector<Shared_File*>* fs =
                                            who->getSharedFiles();

                                    string mes = int_to_string(fs->size());
                                    mes += " ";
                                    for (unsigned int index = 0;
                                            index < fs->size(); ++index) {
                                        mes += (*fs)[index]->file_name;
                                        mes += " ";
                                        mes += int_to_string((*fs)[index]->dim);
                                        mes += " ";
                                    }
                                    memset(buffer, 0, BUFLEN);
                                    memcpy(buffer, mes.c_str(), mes.size());
                                }

                                n = send(i, buffer, sizeof(buffer), 0);
                                memset(buffer, 0, BUFLEN);

                                break;
                            }

                                /* Share ***********************************/
                            case 2: {
                                n = recv(i, buffer, sizeof(buffer), 0);
                                vector<string> shared_file = split(buffer, ' ');
                                Client* who = getClientByFd(clients, i);

                                who->addSharedFile(
                                        new Shared_File(shared_file[0],
                                                atoi(shared_file[1].c_str())));
                                printf("\x1b[32mserver> \x1b[0m");
                                printf("Client [%s] fd [%d] share [%s]\n",
                                        who->getName().c_str(), i,
                                        shared_file[0].c_str());
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                                /* Unshare ***********************************/
                            case 3: {
                                n = recv(i, buffer, sizeof(buffer), 0);
                                Client* who = getClientByFd(clients, i);

                                bool deleted = who->delSharedFile(buffer);

                                if (deleted) {
                                    /* Ok */
                                    printf("\x1b[32mserver> \x1b[0m");
                                    printf("Client [%s] fd [%d] unshare [%s]\n",
                                            who->getName().c_str(), i, buffer);
                                    memset(buffer, 0, BUFLEN);
                                    memcpy(buffer, "0", sizeof("0"));

                                } else {
                                    /* Nu exista */
                                    printf("\x1b[32mserver> \x1b[0m");
                                    printf(
                                            "Client [%s] fd [%d] tried to unshare [%s] -> unknown\n",
                                            who->getName().c_str(), i, buffer);
                                    memset(buffer, 0, BUFLEN);
                                    memcpy(buffer, "-5", sizeof("-5"));

                                }

                                n = send(i, buffer, sizeof(buffer), 0);
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                                /* Quit ***********************************/
                            case 6: // quit-----------------------------------------------------

                                Client* client_gone = getClientByFd(clients, i);
                                printf("\x1b[32mserver> \x1b[0m");
                                printf("Client [%s] fd [%d] quit\n",
                                        client_gone->getName().c_str(), i);

                                /* Stergere client */
                                deleteClient(clients, client_gone);

                                close(i);
                                FD_CLR(i, &read_fds);

                                break;
                            }

                            memset(buffer, 0, BUFLEN);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
