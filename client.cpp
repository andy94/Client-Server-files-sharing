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
#include <cstdlib>
#include <string>
#include <map>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "aux.hpp"

using namespace std;

/*
 *    USAGE: ./client <nume_client> <nume_dir> <port_client> <IP_server> <port_server>
 *               0          1            2           3            4            5
 */

#define MAX_CLIENTS    20
#define BUFLEN 1024
#define forever while(1)

/* Functie prin care se printeaza in log o eroare */
void printError(int type, FILE* log) {
    Error_Type e(type);
    printf("%d : %s\n", e.getErrorType(), e.getErrorMessage().c_str());
    fprintf(log, "%d : %s\n", e.getErrorType(), e.getErrorMessage().c_str());
}

/* Funcite prin care se primesc un set de fisiere (parti de fisier) de la toti
 * clientii care trimit*/
void processFileToRecv(ToProcessFiles &toRcvFiles, ProcessFile *toRcv,
        fd_set &read_fds, int i, char file_name[], FILE* log) {

    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);

    /* Primire dimensiune bufferului care va fi trimis */
    int n = recv(toRcv->sock_id, buffer, sizeof(buffer), 0);
    if (n < 0) {
        printError(-2, log);
        return;
    }

    int sz = atoi(buffer);

    /* Primire continut */
    memset(buffer, 0, BUFLEN);
    n = recv(toRcv->sock_id, buffer, sizeof(buffer), 0);
    if (n < 0) {
        printError(-2, log);
        return;
    }

    /* Scrie in fisier */
    write(toRcv->file_fd, buffer, sz);
    memset(buffer, 0, BUFLEN);

    /* Creste dimensiunea procesata */
    toRcv->processed_dim += sz;

    /* Daca s-a primit complet fisierul */
    if (toRcv->isDone()) {
        close(toRcv->file_fd);
        FD_CLR(i, &read_fds);
        close(i);

        toRcvFiles.removeBySockId(i);
        vector<string> rs = split(toRcv->file->file_name, '/');
        string fn = rs[rs.size() - 1];

        printf("\nSucces %s\n", fn.c_str());
        printf("\033[22;34m%s> \033[0m", file_name);
        fprintf(log, "Succes %s\n", fn.c_str());
        fflush(stdout);
    }
}

/* Functie prin care se trimite un set de continut pentru fiecare client care
 * asteapta sa primeasca un fisier */
void processFileToSend(ToProcessFiles &toSndFiles,
        vector<pair<string, string> > &history) {

    /* Pentru fiecare "abonat" */
    for (int index = 0; index < toSndFiles.getSize(); ++index) {

        char buffer[BUFLEN];
        ProcessFile* pf = toSndFiles.getFilesByIndex(index);

        memset(buffer, 0, BUFLEN);
        int n = read(pf->file_fd, buffer, BUFLEN);
        pf->processed_dim += n;

        string sz = int_to_string(n);
        char sizeChar[BUFLEN];
        memset(sizeChar, 0, BUFLEN);
        memcpy(sizeChar, sz.c_str(), sz.size());

        /* Trimite dimensiunea continutului ce va urma */
        n = send(pf->sock_id, sizeChar, sizeof(sizeChar), 0);

        /* Daca e o iesie fortata a clientului */
        if (n <= 0) {
            toSndFiles.removeBySockIndex(index);
            close(pf->file_fd);
            close(pf->sock_id);
            return;
        }
        memset(sizeChar, 0, BUFLEN);

        /* Trimite continut */
        n = send(pf->sock_id, buffer, sizeof(buffer), 0);
        memset(buffer, 0, BUFLEN);

        /* Daca s-a terminat de transmis */
        if (pf->isDone()) {
            close(pf->file_fd);
            close(pf->sock_id);

            vector<string> rs = split(pf->file->file_name, '/');
            string fn = rs[rs.size() - 1];
            history.push_back(pair<string, string>(pf->cl_name, fn));
            toSndFiles.removeBySockIndex(index);
            index--;
        }
    }
}

int main(int argc, char *argv[]) {

    /* Declarare socketi, adrese si buffere */
    int server_connect_sock, listen_sock, new_sock, n = 0, i;

    struct sockaddr_in server_addr, my_addr, new_addr;
    int my_port;
    socklen_t clilen;

    fd_set read_fds;    //multimea de citire folosita in select()
    fd_set temp_fds;    //multime folosita temporar

    FD_ZERO(&read_fds);
    FD_ZERO(&temp_fds);

    char buffer[BUFLEN];

    /* Apelare gresita */
    if (argc != 6) {
        error(
                "Usage: ./client <nume_client> <nume_dir> <port_client> <IP_server> <port_server>");
    }

    /* Creearea socketului de listen *****************************************/
    my_port = atoi(argv[3]);
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        error("ERROR opening listen socket");
    }

    /* Initializare my_addr */
    memset((char *) &my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(my_port);

    /* Bind pe listen_sock */
    if (bind(listen_sock, (struct sockaddr *) &my_addr, sizeof(struct sockaddr))
            < 0) {
        error("ERROR on binding");
    }
    /* listen_sock va fi pe listen pentru noile conexiuni */
    listen(listen_sock, MAX_CLIENTS);

    /* Creearea socketului conectare la server *******************************/
    server_connect_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_connect_sock < 0) {
        error("ERROR opening socket");
    }
    memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[5]));
    inet_aton(argv[4], &server_addr.sin_addr);

    /* Conectare la server **************************************************/
    if (connect(server_connect_sock, (struct sockaddr*) &server_addr,
            sizeof(server_addr)) < 0) {
        error("ERROR connecting");
    }

    // trimitere nume
    if (send(server_connect_sock, argv[1], strlen(argv[1]), 0) < 0) { 
        error("ERROR contecting");
    }
    // primire accept/reject
    if (recv(server_connect_sock, buffer, sizeof(buffer), 0) <= 0) { 
        error("ERROR recieve accept");
    }

    if (strcmp("accept", buffer) != 0) {
        error("ERROR Name already exists!");
    }

    // trimitere port
    if (send(server_connect_sock, argv[3], strlen(argv[3]), 0) < 0) { 
        error("ERROR sending port");
    }

    /* Initializare "linie de comanda */
    printf("Connected to server\n");
    printf("\033[22;34m%s> \033[0m", argv[1]);
    fflush(stdout);

    /* Creearea fisier de log */
    string log_name = argv[1];
    log_name += ".log";
    FILE *log = fopen(log_name.c_str(), "w");

    /* Instantieri */

    FD_SET(server_connect_sock, &read_fds);
    FD_SET(listen_sock, &read_fds);
    FD_SET(0, &read_fds); // pentru stdin
    int fdmax = max(server_connect_sock, listen_sock);

    vector<Client*> clients; // vector de clienti
    vector<Shared_File*> my_files; // vectorul fisierelor partajate proprii

    Error_Type::instantiateErrors(); // se instantiaza erorile
    Command_Arguments::instantiateCommands();

    /* Directorul */
    string dir_name = "./";
    dir_name += argv[2];
    dir_name += "/";
    char command_name[BUFLEN];

    /* Bufferele de fisiere de trimis / primit */
    ToProcessFiles toSndFiles(3); // max 3
    ToProcessFiles toRcvFiles(3);

    /* Timeout pentru a nu bloca stdin-ul rularea */
    struct timeval tmv;
    tmv.tv_sec = 1;
    tmv.tv_usec = 100;
    n++;

    /* Istoria */
    vector<pair<string, string> > history;

    /*Bucla infinita *********************************************************/
    forever {
        temp_fds = read_fds;
        if (select(fdmax + 1, &temp_fds, NULL, NULL, &tmv) == -1) {
            error("ERROR in select");
        }

        /* Pentru fiecare descriptor in parte */
        for (i = 0; i <= fdmax; i++) {

            /* Daca e setat */
            if (FD_ISSET(i, &temp_fds)) {

                /* Daca e mesaj de la server */
                if (i == server_connect_sock) {

                    memset(buffer, 0, BUFLEN);
                    /* Iesire fortata server */
                    if (recv(server_connect_sock, buffer, sizeof(buffer), 0)
                            <= 0) {
                        printf("\nServer closed - no announcing\n");

                        printf("Exiting\n");
                        close(server_connect_sock);
                        close(listen_sock);
                        FD_ZERO(&read_fds);
                        fclose(log);
                        return 0;

                    } else {
                        /* Iesire normala server */

                        if (strcmp(buffer, "quit") == 0) {

                            printf("\nServer closed\n");

                            printf("Exiting\n");
                            close(server_connect_sock);
                            close(listen_sock);
                            FD_ZERO(&read_fds);
                            fclose(log);
                            return 0;

                        }
                    }

                }

                /* Mesaj din alta parte */
                else {
                    /* Primire comanda stdin *********************************/
                    if (i == 0) {

                        /* Citire comanda: */
                        memset(command_name, 0, BUFLEN);
                        fgets(command_name, BUFLEN, stdin);

                        /* Daca e enter si atat */
                        if (command_name[0] == '\n'
                                || command_name[0] == '\0') {
                            printf("\033[22;34m%s> \033[0m", argv[1]);
                            fflush(stdout);
                            continue;
                        }
                        command_name[strlen(command_name) - 1] = 0;

                        /* Daca e incorecta */
                        if (!Command_Arguments::correctCommand(command_name)) {
                            printf("Incorrect! Try again\n");
                            printf("\033[22;34m%s> \033[0m", argv[1]);
                            fflush(stdout);
                            continue;
                        }

                        Command_Arguments command(command_name);

                        /* Trimite comanda la server daca tebuie */
                        if (Command_Arguments::serverCommand(
                                command.getCommandName())) {

                            memset(buffer, 0, BUFLEN); // trimitere comanda
                            memcpy(buffer, command_name, sizeof(command_name));
                            n = send(server_connect_sock, buffer,
                                    sizeof(buffer), 0);
                        }

                        switch (command.getCommandCode()) {

                        /* Infolcients ***********************************/
                        case 0: {

                            memset(buffer, 0, BUFLEN); // primire raspuns
                            n = recv(server_connect_sock, buffer,
                                    sizeof(buffer), 0);

                            fprintf(log, "%s> infoclients\n", argv[1]);

                            vector<string> info = split(buffer, ' ');
                            for (unsigned int index = 0; index < info.size();
                                    index += 3) {
                                printf("%s %s %s\n", info[index].c_str(),
                                        info[index + 1].c_str(),
                                        info[index + 2].c_str());

                                fprintf(log, "%s %s %s\n", info[index].c_str(),
                                        info[index + 1].c_str(),
                                        info[index + 2].c_str());

                                /* Update clients: */

                                if (!exist(clients, info[index])) {
                                    clients.push_back(
                                            new Client(-1, info[index],
                                                    info[index + 1],
                                                    atoi(
                                                            info[index + 2].c_str())));
                                } else {
                                    /* Daca exista se face update la ip si port */
                                    Client *founded = getClientByName(clients,
                                            info[index]);
                                    founded->setIpChar(info[index + 1]);
                                    founded->setPortNoInt(
                                            atoi(info[index + 2].c_str()));
                                    founded->update();
                                    /* Am procedat astfel pentru o modificare usoara:
                                     * Putem sa nu modificam la doua apeluri succesive de infoclients
                                     * intreaga lista de fisiere partajate. Daca in schimb nu se doreste
                                     * acest lucru, se apeleaza: founded->clearSharedFiles() */
                                }

                            }

                            /* Eliminarea celor iesiti: */
                            for (unsigned int index = 0; index < clients.size();
                                    ++index) {

                                if (!clients[index]->isUpdated()) {
                                    deleteClient(clients, clients[index]);
                                    index--;
                                } else {
                                    clients[index]->make_old();
                                }

                            }
                            memset(buffer, 0, BUFLEN);

                            break;
                        }

                            /* Getshare **********************************/
                        case 1: {
                            string mes = command.getFisrtArg();

                            memset(buffer, 0, BUFLEN); // trimitere comanda
                            memcpy(buffer, mes.c_str(), mes.size());
                            n = send(server_connect_sock, buffer,
                                    sizeof(buffer), 0);

                            /* Astept raspuns: */

                            memset(buffer, 0, BUFLEN);
                            n = recv(server_connect_sock, buffer,
                                    sizeof(buffer), 0);

                            /* Tratez si cazul in care clientul cautat e pe server,
                             * dar la mine nu e pentru ca nu am mai facut infoclients */
                            Client * client = getClientByName(clients,
                                    command.getFisrtArg());

                            fprintf(log, "%s> getshare %s\n", argv[1],
                                    command.getFisrtArg().c_str());

                            if (client == NULL) { // daca nu exista in lista mea e necunoscut
                                printError(-4, log);
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                            // daca nu exista pe server
                            if (strcmp(buffer, "-3") == 0) { 
                                printError(-3, log);
                            } else {

                                vector<string> vals = split(buffer, ' ');
                                int no = atoi(vals[0].c_str());

                                client->clearSharedFiles();
                                for (unsigned int index = 1;
                                        index < vals.size(); index += 2) {

                                    client->addSharedFile(
                                            new Shared_File(vals[index],
                                                    atoi(
                                                            vals[index + 1].c_str())));

                                }

                                /* Afisare */
                                printf("%d\n", no);
                                fprintf(log, "%d\n", no);
                                for (unsigned int index = 0;
                                        index < client->getSharedFiles()->size();
                                        ++index) {
                                    printf("%s %s\n",
                                            (*client->getSharedFiles())[index]->file_name.c_str(),
                                            Shared_File::get_user_fiendly_dim_reprezentation(
                                                    (*client->getSharedFiles())[index]->dim).c_str());
                                    fprintf(log, "%s %s\n",
                                            (*client->getSharedFiles())[index]->file_name.c_str(),
                                            Shared_File::get_user_fiendly_dim_reprezentation(
                                                    (*client->getSharedFiles())[index]->dim).c_str());
                                }
                            }
                            memset(buffer, 0, BUFLEN);
                            break;
                        }

                            /* Share ***********************************/
                        case 2: {

                            string path = dir_name + command.getFisrtArg();

                            struct stat file_info;

                            int res;

                            fprintf(log, "%s> share %s\n", argv[1],
                                    command.getFisrtArg().c_str());

                            // daca nu exista
                            if ((res = stat(path.c_str(), &file_info)) == -1) { 
                                printError(-5, log);
                                break;
                            }

                            /* Se trimite doar daca e nevoie la server */
                            memset(buffer, 0, BUFLEN); // trimitere comanda
                            memcpy(buffer, command_name, sizeof(command_name));
                            n = send(server_connect_sock, buffer,
                                    sizeof(buffer), 0);

                            /* Adaugare in lista proprie */
                            my_files.push_back(
                                    new Shared_File(command.getFisrtArg(),
                                            (int) file_info.st_size));

                            string mes = command.getFisrtArg() + " "
                                    + int_to_string((int) file_info.st_size);

                            memset(buffer, 0, BUFLEN); // trimitere comanda
                            memcpy(buffer, mes.c_str(), mes.size());
                            n = send(server_connect_sock, buffer,
                                    sizeof(buffer), 0);

                            printf("Succes\n");
                            fprintf(log, "Succes\n");
                            memset(buffer, 0, BUFLEN);
                            break;
                        }

                            /* Unshare ***********************************/
                        case 3: {

                            /* stergere din lista proprie */
                            string mes = command.getFisrtArg();

                            fprintf(log, "%s> unshare %s\n", argv[1],
                                    command.getFisrtArg().c_str());

                            for (vector<Shared_File*>::iterator it =
                                    my_files.begin(); it != my_files.end();
                                    ++it) {

                                if ((*it)->file_name == mes) {
                                    my_files.erase(it);
                                    break;
                                }

                            }

                            memset(buffer, 0, BUFLEN); // trimitere comanda
                            memcpy(buffer, mes.c_str(), mes.size());
                            n = send(server_connect_sock, buffer,
                                    sizeof(buffer), 0);

                            /* Astept raspuns: */
                            memset(buffer, 0, BUFLEN);
                            n = recv(server_connect_sock, buffer,
                                    sizeof(buffer), 0);
                            if (strcmp(buffer, "0") == 0) {
                                printf("Succes\n");
                                fprintf(log, "Succes\n");
                            } else {
                                printError(atoi(buffer), log);
                            }
                            memset(buffer, 0, BUFLEN);
                            break;
                        }
                            /* Getfile ***********************************/
                        case 4: {

                            fprintf(log, "%s> getfile %s %s\n", argv[1],
                                    command.getFisrtArg().c_str(),
                                    command.getSecondArg().c_str());

                            /* Daca are deja mai mult de 3 receptionari */
                            if (toRcvFiles.isFull()) {
                                printf("Busy\n");
                                fprintf(log, "Busy\n");
                                break;
                            }

                            string file_name = command.getSecondArg();
                            string client_name = command.getFisrtArg();

                            Client * client = getClientByName(clients,
                                    client_name);

                            /* Daca nu exista in lista mea e necunoscut */
                            if (client == NULL) {
                                printError(-4, log);
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                            Shared_File* file = client->getSharedFileByName(
                                    file_name);

                            /* Daca nu are fisierul asta clientul */
                            if (file == NULL) {
                                printError(-5, log);
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                            bool shared = false;
                            for (vector<Shared_File*>::iterator it =
                                    my_files.begin(); it != my_files.end();
                                    ++it) {
                                if ((*it)->file_name == file_name) {
                                    shared = true;
                                    break;
                                }
                            }

                            /* Daca am eu un fisier partajat cu acelasi nume */
                            if (shared) {
                                printError(-6, log);
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                            /* Conectare la client: */

                            int getter_sock = socket(AF_INET,
                            SOCK_STREAM, 0); // socketul
                            if (getter_sock < 0) {
                                printError(-1, log);
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                            struct sockaddr_in getter_addr; // addr
                            memset((char *) &getter_addr, 0,
                                    sizeof(getter_addr));
                            getter_addr.sin_family = AF_INET;
                            getter_addr.sin_port = htons(
                                    client->getPortNoInt());
                            inet_aton(client->getIpChar().c_str(),
                                    &getter_addr.sin_addr);

                            if (connect(getter_sock,
                                    (struct sockaddr*) &getter_addr,
                                    sizeof(getter_addr)) < 0) { //conectare

                                printError(-1, log);
                                memset(buffer, 0, BUFLEN);
                                break;
                            }

                            /* Daca totul e ok */

                            /* Trimite nume fisier: */
                            memset(buffer, 0, BUFLEN); // trimitere comanda
                            memcpy(buffer, file_name.c_str(), file_name.size());
                            n = send(getter_sock, buffer, sizeof(buffer), 0);

                            /* Primire raspuns daca e ok: */
                            memset(buffer, 0, BUFLEN);
                            n = recv(getter_sock, buffer, sizeof(buffer), 0);
                            if (strcmp(buffer, "0") != 0) { // daca nu e ok
                                close(getter_sock);
                                printError(atoi(buffer), log);
                            } else {

                                FD_SET(getter_sock, &read_fds); // setare descriptor
                                if (getter_sock > fdmax) {
                                    fdmax = getter_sock;
                                }

                                memset(buffer, 0, BUFLEN); // trimitere numele meu
                                memcpy(buffer, argv[1], strlen(argv[1]));
                                n = send(getter_sock, buffer, sizeof(buffer),
                                        0);
                                memset(buffer, 0, BUFLEN);

                                /* Creeare fisier */
                                string path = dir_name + file_name;
                                int fileFD = creat(path.c_str(),
                                O_CREAT);

                                listen(getter_sock, 1); // setare pe listen pentru primire

                                /* Adaugare in bufferul de primit */
                                Shared_File *sf = new Shared_File(path,
                                        file->dim);
                                ProcessFile *toRec = new ProcessFile(
                                        getter_sock, client_name, fileFD, sf);

                                toRcvFiles.addFile(toRec);

                            }

                            memset(buffer, 0, BUFLEN);

                            break;
                        }

                        /* History ***********************************/
                        case 5:

                            fprintf(log, "%s> history\n", argv[1]);
                            printf("%d\n", (int) history.size());
                            fprintf(log, "%d\n", (int) history.size());

                            for (unsigned int index = 0; index < history.size();
                                    ++index) {

                                printf("%s %s\n", history[index].first.c_str(),
                                        history[index].second.c_str());
                                fprintf(log, "%s %s\n",
                                        history[index].first.c_str(),
                                        history[index].second.c_str());

                            }

                            break;

                            /* Quit ***********************************/
                        case 6:
                            printf("Good bye!\n");

                            /* Daca mai sunt fisiere de trimis */
                            if (!toSndFiles.isEmpty()) {
                                printf("Finishing to send files\n");
                                while (!toSndFiles.isEmpty()) {
                                    processFileToSend(toSndFiles, history);
                                }
                            }

                            /* Daca mai am fisiere de primit */
                            if (!toRcvFiles.isEmpty()) {
                                printf(
                                        "Some files are not complete received\n");

                            }

                            printf("Exiting\n");
                            close(server_connect_sock);
                            close(listen_sock);
                            FD_ZERO(&read_fds);
                            fclose(log);
                            return 0;

                        }

                        memset(buffer, 0, BUFLEN);
                        memset(command_name, 0, BUFLEN);
                        printf("\033[22;34m%s> \033[0m", argv[1]);
                        fflush(stdout);

                    } else {
                        /* Primire pe alt socket -> primire cerere de fisier */

                         /* Daca e cel de listen -> o noua conexiune */
                        if (i == listen_sock) {
                            clilen = sizeof(new_addr);

                            if ((new_sock = accept(listen_sock,
                                    (struct sockaddr *) &new_addr, &clilen))
                                    == -1) {
                                printf(
                                        "ERROR in accepting client connection\n");
                            } else {

                                memset(buffer, 0, BUFLEN);
                                n = recv(new_sock, buffer, sizeof(buffer), 0);

                                string file_name = buffer;
                                string cl_name;

                                /* Verific sa nu am deja 3 in buffer de trimis */
                                if (toSndFiles.isFull()) {

                                    memset(buffer, 0, BUFLEN);
                                    memcpy(buffer, "-7", sizeof("-7"));
                                    n = send(new_sock, buffer, sizeof(buffer),
                                            0);
                                    close(new_sock);
                                    continue;

                                }

                                /* Verificare existenta si la mine: */
                                bool shared = false;
                                int dim = 0;
                                for (vector<Shared_File*>::iterator it =
                                        my_files.begin(); it != my_files.end();
                                        ++it) {
                                    if ((*it)->file_name == file_name) {
                                        shared = true;
                                        dim = (*it)->dim;
                                        break;
                                    }
                                }

                                if (!shared) { // daca nu il am:

                                    memset(buffer, 0, BUFLEN);
                                    memcpy(buffer, "-5", sizeof("-5"));
                                    n = send(new_sock, buffer, sizeof(buffer),
                                            0);
                                    close(new_sock);

                                } else { // daca il am:


                                    /* Accept */
                                    memset(buffer, 0, BUFLEN);
                                    memcpy(buffer, "0", sizeof("0"));
                                    n = send(new_sock, buffer, sizeof(buffer),
                                            0);

                                    memset(buffer, 0, BUFLEN);
                                    n = recv(new_sock, buffer, sizeof(buffer),
                                            0);
                                    cl_name = buffer;

                                    /* Deschidere fisier */
                                    string path = dir_name + file_name;
                                    int fileFD = open(path.c_str(),
                                    O_RDONLY);

                                    /* Adauga in bufferul de trimis */
                                    Shared_File *sf = new Shared_File(path,
                                            dim);
                                    ProcessFile *pf = new ProcessFile(new_sock,
                                            cl_name, fileFD, sf);

                                    toSndFiles.addFile(pf);
                                }
                                memset(buffer, 0, BUFLEN);
                            }

                        } else {

                            /* Daca primesc bucati de fisiere */

                            ProcessFile *toRcv = toRcvFiles.getFileBySockId(i);

                            if (toRcv != NULL) {
                                /* Proceseaza primirea */
                                processFileToRecv(toRcvFiles, toRcv, read_fds,
                                        i, argv[1], log);
                            }
                        }
                    }
                    memset(buffer, 0, BUFLEN);
                }
            }
        }
        /* Daca am de trimis, trimit cate o parte la fiecare */
        if (!toSndFiles.isEmpty()) {
            processFileToSend(toSndFiles, history);
        }
    }

    close(server_connect_sock);
    close(listen_sock);
    fclose(log);
    return 0;
}
