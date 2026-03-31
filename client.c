#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PORTFOLIO 20

/* Structure pour un actif dans le portefeuille du client */
typedef struct {
    char nom[50];
    int quantite;
} portfolio_t;

/* Portefeuille local du client */
portfolio_t portefeuille[MAX_PORTFOLIO];
int nb_actifs = 0;

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

    printf("Commandes disponibles : LIST, INFO <produit>, BUY <produit> <qte>, SELL <produit> <qte>, WALLET, exit\n\n");

    while (1) {
        /* Saisir une commande a envoyer au serveur */
        printf("Commande > ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        /* Ignorer les commandes vides */
        if (strlen(buffer) == 0) {
            continue;
        }

        /* Commande WALLET : afficher le portefeuille localement */
        if (strcmp(buffer, "WALLET") == 0) {
            if (nb_actifs == 0) {
                printf("Portefeuille vide.\n");
            } else {
                printf("--- Portefeuille client ---\n");
                int i;
                for (i = 0; i < nb_actifs; i++) {
                    printf("%s  |  Qte : %d\n",
                           portefeuille[i].nom, portefeuille[i].quantite);
                }
                printf("---------------------------\n");
            }
            continue;
        }

        /* Sauvegarder la commande envoyee pour mettre a jour le portefeuille */
        char cmd_save[BUFFER_SIZE] = {0};
        strncpy(cmd_save, buffer, BUFFER_SIZE - 1);

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

        /* Mise a jour du portefeuille apres une reponse du serveur */

        /* Extraire la commande et les arguments depuis cmd_save */
        char cmd[BUFFER_SIZE] = {0};
        char arg_produit[50] = {0};
        char arg_qte_str[20] = {0};
        int i = 0;

        /* Lire la commande (premier mot) */
        while (cmd_save[i] != '\0' && cmd_save[i] != ' ') {
            cmd[i] = cmd_save[i];
            i++;
        }
        cmd[i] = '\0';

        /* Lire le nom du produit (deuxieme mot) */
        if (cmd_save[i] == ' ') {
            i++;
            int p = 0;
            while (cmd_save[i] != '\0' && cmd_save[i] != ' ') {
                arg_produit[p] = cmd_save[i];
                i++;
                p++;
            }
            arg_produit[p] = '\0';
        }

        /* Lire la quantite (troisieme mot) */
        if (cmd_save[i] == ' ') {
            i++;
            strncpy(arg_qte_str, cmd_save + i, 19);
        }

        /* Si c'etait un BUY reussi : ajouter au portefeuille */
        if (strcmp(cmd, "BUY") == 0 && strlen(arg_produit) > 0
            && strlen(arg_qte_str) > 0
            && buffer[0] == 'A' && buffer[1] == 'c' && buffer[2] == 'h'
            && buffer[3] == 'a' && buffer[4] == 't') {
            int qte = atoi(arg_qte_str);
            int trouve = 0;
            int j;
            for (j = 0; j < nb_actifs; j++) {
                if (strcmp(portefeuille[j].nom, arg_produit) == 0) {
                    portefeuille[j].quantite = portefeuille[j].quantite + qte;
                    trouve = 1;
                    break;
                }
            }
            if (trouve == 0 && nb_actifs < MAX_PORTFOLIO) {
                strncpy(portefeuille[nb_actifs].nom, arg_produit, 49);
                portefeuille[nb_actifs].quantite = qte;
                nb_actifs++;
            }
        }

        /* Si c'etait un SELL reussi : retirer du portefeuille */
        if (strcmp(cmd, "SELL") == 0 && strlen(arg_produit) > 0
            && strlen(arg_qte_str) > 0
            && buffer[0] == 'V' && buffer[1] == 'e' && buffer[2] == 'n'
            && buffer[3] == 't' && buffer[4] == 'e') {
            int qte = atoi(arg_qte_str);
            int j;
            for (j = 0; j < nb_actifs; j++) {
                if (strcmp(portefeuille[j].nom, arg_produit) == 0) {
                    portefeuille[j].quantite = portefeuille[j].quantite - qte;
                    break;
                }
            }
        }
    }

    /* Fermer le socket */
    close(sock);

    return 0;
}
