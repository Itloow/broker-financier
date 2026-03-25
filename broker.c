/*
 * broker.c - Serveur Broker Financier
 * Projet 1 : Broker financier
 * Licence 3 MIASHS - Programmation systèmes et réseaux
 * Université Toulouse Jean Jaurès - 2025/2026
 *
 * Compilation : gcc -Wall -Wextra broker.c -o broker -lpthread
 * Exécution :   ./broker
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

/*
 * handle_client - Gère la communication avec un client dans un thread dédié.
 * Pattern identique à la correction TP1 Q4 (version pthread).
 */
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE] = {0};

    /* Envoyer un message de bienvenue au client */
    char *welcome = "Bienvenue sur le Broker Financier.";
    send(client_socket, welcome, strlen(welcome), 0);

    while (1) {
        /* Effacer le buffer avant chaque lecture */
        memset(buffer, 0, BUFFER_SIZE);

        /* Lire le message du client */
        int valread = read(client_socket, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            printf("Client deconnecte.\n");
            break;
        }

        printf("Message recu : %s\n", buffer);

        /* Vérifier si le client veut quitter */
        if (strcmp(buffer, "exit") == 0) {
            printf("Le client a demande a quitter.\n");
            break;
        }

        /* TODO : Traitement des commandes (taches suivantes) */

        /* Réponse temporaire : echo */
        send(client_socket, buffer, strlen(buffer), 0);
    }

    /* Fermer le socket du client */
    close(client_socket);
    free(arg); /* Libérer la mémoire allouée pour le socket */
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    /* Création du socket */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    /* Configuration de l'adresse du serveur */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    /* Attachement du socket au port */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Erreur bind");
        exit(EXIT_FAILURE);
    }

    /* Écoute des connexions entrantes */
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Erreur listen");
        exit(EXIT_FAILURE);
    }

    printf("Serveur broker en attente de connexion sur le port %d...\n", PORT);

    while (1) {
        /* Accepter une nouvelle connexion */
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                 (socklen_t *)&addrlen)) < 0) {
            perror("Erreur accept");
            continue;
        }

        printf("Nouveau client connecte.\n");

        /* Allouer de la mémoire pour le socket du client */
        int *client_socket = malloc(sizeof(int));
        if (client_socket == NULL) {
            perror("Erreur malloc");
            close(new_socket);
            continue;
        }
        *client_socket = new_socket;

        /* Créer un thread pour gérer le client */
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client,
                           (void *)client_socket) != 0) {
            perror("Erreur pthread_create");
            free(client_socket);
            close(new_socket);
            continue;
        }

        /* Détacher le thread pour qu'il se libère automatiquement */
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
