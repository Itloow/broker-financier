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

    /* Creation du socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    /* Convertir l'adresse IP en format binaire */
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Adresse invalide");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* Connexion au serveur */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Echec de la connexion");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connecte au broker.\n");

    /* Recevoir le message de bienvenue du serveur */
    memset(buffer, 0, BUFFER_SIZE);
    int valread = read(sock, buffer, BUFFER_SIZE);
    if (valread > 0) {
        printf("%s\n", buffer);
    } else {
        printf("Impossible de recevoir le message de bienvenue.\n");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("Commandes disponibles : LIST, INFO <produit>, exit\n\n");

    while (1) {
        /* Saisir une commande a envoyer au serveur */
        printf("Commande > ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        /* Ignorer les commandes vides */
        if (strlen(buffer) == 0) {
            continue;
        }

        /* Envoyer la commande au serveur */
        send(sock, buffer, strlen(buffer), 0);

        /* Si l'utilisateur veut quitter : attendre la confirmation du serveur */
        if (strcmp(buffer, "exit") == 0) {
            memset(buffer, 0, BUFFER_SIZE);
            valread = read(sock, buffer, BUFFER_SIZE);
            if (valread > 0) {
                printf("%s\n", buffer);
            }
            printf("Deconnexion du broker.\n");
            break;
        }

        /* Lire la reponse du serveur */
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(sock, buffer, BUFFER_SIZE);

        if (valread == 0) {
            /* Le serveur a ferme la connexion proprement */
            printf("Le serveur a ferme la connexion.\n");
            break;
        }

        if (valread < 0) {
            /* Erreur de lecture : deconnexion inattendue */
            perror("Erreur de lecture");
            printf("Deconnexion inattendue du serveur.\n");
            break;
        }

        printf("%s\n", buffer);
    }

    /* Fermer le socket */
    close(sock);

    return 0;
}
