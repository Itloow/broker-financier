/*
 * client.c - Client du Broker Financier
 * Projet 1 : Broker financier
 * Licence 3 MIASHS - Programmation systèmes et réseaux
 * Université Toulouse Jean Jaurès - 2025/2026
 *
 * Compilation : gcc -Wall -Wextra client.c -o client
 * Exécution :   ./client
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    /* Création du socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    /* Convertir l'adresse IP en format binaire */
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Adresse invalide");
        exit(EXIT_FAILURE);
    }

    /* Connexion au serveur */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Echec de la connexion");
        exit(EXIT_FAILURE);
    }

    printf("Connecte au broker.\n");

    /* Recevoir le message de bienvenue du serveur */
    memset(buffer, 0, BUFFER_SIZE);
    int valread = read(sock, buffer, BUFFER_SIZE);
    if (valread > 0) {
        printf("%s\n", buffer);
    }

    while (1) {
        /* Saisir une commande à envoyer au serveur */
        printf("Commande > ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; /* Supprimer le saut de ligne */

        /* Envoyer la commande au serveur */
        send(sock, buffer, strlen(buffer), 0);

        /* Vérifier si l'utilisateur veut quitter */
        if (strcmp(buffer, "exit") == 0) {
            printf("Deconnexion du broker.\n");
            break;
        }

        /* Lire la réponse du serveur */
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            printf("Serveur deconnecte.\n");
            break;
        }
        printf("%s\n", buffer);
    }

    /* Fermer le socket */
    close(sock);

    return 0;
}
