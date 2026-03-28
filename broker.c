#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define NB_PRODUITS 5

/* Structure pour un produit financier (pattern typedef struct du TP2) */
typedef struct {
    char nom[50];
    float prix;
    int quantite;
} produit_t;

/* Structure pour passer les infos du client au thread
 * (meme pattern que client_data_t dans serveurc.c du TP2) */
typedef struct {
    int socket_fd;
    struct sockaddr_in client_addr;
} client_info_t;

/* Liste des produits disponibles chez le broker */
produit_t produits[NB_PRODUITS] = {
    {"Apple",     180.50, 100},
    {"Google",    140.25, 80},
    {"Tesla",     250.75, 50},
    {"Amazon",    185.00, 120},
    {"Microsoft", 420.30, 90}
};

/*
 * handle_client - Gere la communication avec un client dans un thread dedie.
 * Recoit un pointeur vers client_info_t contenant le socket ET l'adresse du client.
 */
void *handle_client(void *arg) {
    client_info_t *info = (client_info_t *)arg;
    int client_socket = info->socket_fd;
    char *client_ip = inet_ntoa(info->client_addr.sin_addr);
    int client_port = ntohs(info->client_addr.sin_port);
    char buffer[BUFFER_SIZE] = {0};

    printf("[LOG] Client connecte : %s:%d\n", client_ip, client_port);

    /* Envoyer un message de bienvenue au client */
    char *welcome = "Bienvenue sur le Broker Financier.";
    send(client_socket, welcome, strlen(welcome), 0);

    while (1) {
        /* Effacer le buffer avant chaque lecture */
        memset(buffer, 0, BUFFER_SIZE);

        /* Lire le message du client */
        int valread = read(client_socket, buffer, BUFFER_SIZE);

        /* Cas 1 : le client a ferme la connexion proprement (read retourne 0) */
        if (valread == 0) {
            printf("[LOG] Client %s:%d a ferme la connexion.\n", client_ip, client_port);
            break;
        }

        /* Cas 2 : erreur de lecture sur le socket */
        if (valread < 0) {
            perror("[ERREUR] Erreur de lecture");
            printf("[LOG] Deconnexion inattendue du client %s:%d\n", client_ip, client_port);
            break;
        }

        printf("[LOG] Commande recue de %s:%d : %s\n", client_ip, client_port, buffer);

        /* Cas 3 : le client envoie "exit" pour quitter */
        if (strcmp(buffer, "exit") == 0) {
            printf("[LOG] Client %s:%d a demande a quitter.\n", client_ip, client_port);
            char *bye = "Deconnexion confirmee. A bientot.";
            send(client_socket, bye, strlen(bye), 0);
            break;
        }

        /* Extraire la commande (premier mot) et l'argument */
        char commande[BUFFER_SIZE] = {0};
        char argument[BUFFER_SIZE] = {0};
        int i = 0;

        while (buffer[i] != '\0' && buffer[i] != ' ') {
            commande[i] = buffer[i];
            i++;
        }
        commande[i] = '\0';

        if (buffer[i] == ' ') {
            i++;
            strncpy(argument, buffer + i, BUFFER_SIZE - 1);
        }

        /* Traitement des commandes */
        char response[BUFFER_SIZE * 2] = {0};

        if (strcmp(commande, "LIST") == 0) {
            /* Commande LIST : afficher tous les produits */
            sprintf(response, "--- Produits disponibles ---\n");
            for (int j = 0; j < NB_PRODUITS; j++) {
                char ligne[100] = {0};
                sprintf(ligne, "%s  |  Prix : %.2f  |  Qte : %d\n",
                        produits[j].nom, produits[j].prix, produits[j].quantite);
                strcat(response, ligne);
            }
            strcat(response, "----------------------------");

        } else if (strcmp(commande, "INFO") == 0) {
            /* Commande INFO <produit> : afficher les details d'un produit */
            if (strlen(argument) == 0) {
                sprintf(response, "Usage : INFO <nom_produit>");
            } else {
                int trouve = 0;
                for (int j = 0; j < NB_PRODUITS; j++) {
                    if (strcmp(argument, produits[j].nom) == 0) {
                        sprintf(response, "Produit : %s\nPrix : %.2f\nQuantite disponible : %d",
                                produits[j].nom, produits[j].prix, produits[j].quantite);
                        trouve = 1;
                        break;
                    }
                }
                if (trouve == 0) {
                    sprintf(response, "Produit '%s' introuvable.", argument);
                }
            }

        } else {
            /* Commande inconnue */
            sprintf(response, "Commande inconnue : %s\nCommandes disponibles : LIST, INFO <produit>", commande);
        }

        send(client_socket, response, strlen(response), 0);
    }

    /* Nettoyage : fermer le socket puis liberer la memoire (pattern TP pthread) */
    close(client_socket);
    printf("[LOG] Socket du client %s:%d ferme. Thread termine.\n", client_ip, client_port);
    free(info);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    /* Creation du socket */
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

    /* Ecoute des connexions entrantes */
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

        /* Allouer la structure client_info_t pour passer au thread */
        client_info_t *info = malloc(sizeof(client_info_t));
        if (info == NULL) {
            perror("Erreur malloc");
            close(new_socket);
            continue;
        }
        info->socket_fd = new_socket;
        info->client_addr = address;

        /* Creer un thread pour gerer le client */
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)info) != 0) {
            perror("Erreur pthread_create");
            free(info);
            close(new_socket);
            continue;
        }

        /* Detacher le thread pour qu'il se libere automatiquement */
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
