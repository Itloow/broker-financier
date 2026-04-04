#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PORTFOLIO 20

/* structure pour stocker un actif dans le portefeuille du client */
typedef struct {
    char nom[50];
    int quantite;
} portfolio_t;

/* portefeuille local du client, on garde tout en memoire */
portfolio_t portefeuille[MAX_PORTFOLIO];
int nb_actifs = 0;

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    /* on cree le socket TCP */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    /* on convertit l'IP en binaire */
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Adresse invalide");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* connexion au serveur */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Echec de la connexion");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connecte au broker.\n");

    /* on recoit le message de bienvenue */
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
        printf("Commande > ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        /* on ignore si l'utilisateur appuie juste sur entree */
        if (strlen(buffer) == 0) {
            continue;
        }

        /* commande WALLET : on affiche le portefeuille local sans passer par le serveur */
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

        /* on sauvegarde la commande avant de l'envoyer, on en aura besoin apres */
        char cmd_save[BUFFER_SIZE] = {0};
        strncpy(cmd_save, buffer, BUFFER_SIZE - 1);

        /* --- extraction de la commande et des arguments pour les verifications locales --- */
        char cmd[BUFFER_SIZE] = {0};
        char arg_produit[50] = {0};
        char arg_qte_str[20] = {0};
        int i = 0;

        /* premier mot = la commande (BUY, SELL, LIST, etc.) */
        while (cmd_save[i] != '\0' && cmd_save[i] != ' ') {
            cmd[i] = cmd_save[i];
            i++;
        }
        cmd[i] = '\0';

        /* deuxieme mot = le nom du produit */
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

        /* troisieme mot = la quantite */
        if (cmd_save[i] == ' ') {
            i++;
            strncpy(arg_qte_str, cmd_save + i, 19);
        }

        /* ============================================================ */
        /* VERIFICATIONS COTE CLIENT AVANT ENVOI (tache 14)             */
        /* on verifie localement pour eviter d'envoyer des commandes    */
        /* qu'on sait deja invalides, ca evite de surcharger le serveur */
        /* ============================================================ */

        /* pour BUY et SELL : on verifie que la quantite est valide */
        if (strcmp(cmd, "BUY") == 0 || strcmp(cmd, "SELL") == 0) {

            /* verif : il faut un nom de produit */
            if (strlen(arg_produit) == 0) {
                printf("Erreur : il manque le nom du produit.\n");
                printf("Usage : %s <produit> <quantite>\n", cmd);
                continue;
            }

            /* verif : il faut une quantite */
            if (strlen(arg_qte_str) == 0) {
                printf("Erreur : il manque la quantite.\n");
                printf("Usage : %s <produit> <quantite>\n", cmd);
                continue;
            }

            /* verif : la quantite doit etre un nombre > 0 */
            int qte_check = atoi(arg_qte_str);
            if (qte_check <= 0) {
                printf("Erreur : la quantite doit etre superieure a 0.\n");
                continue;
            }

            /* verif specifique au SELL : on check si on possede assez d'actions */
            if (strcmp(cmd, "SELL") == 0) {
                int possede = 0;
                int j;
                for (j = 0; j < nb_actifs; j++) {
                    if (strcmp(portefeuille[j].nom, arg_produit) == 0) {
                        possede = portefeuille[j].quantite;
                        break;
                    }
                }
                if (possede < qte_check) {
                    printf("Impossible de vendre %d x %s : vous n'en possedez que %d.\n",
                           qte_check, arg_produit, possede);
                    continue;
                }
            }
        }

        /* verif pour INFO : il faut un nom de produit */
        if (strcmp(cmd, "INFO") == 0 && strlen(arg_produit) == 0) {
            printf("Erreur : il manque le nom du produit.\n");
            printf("Usage : INFO <produit>\n");
            continue;
        }

        /* ============================================================ */
        /* FIN DES VERIFICATIONS LOCALES                                */
        /* si on arrive ici, la commande est valide, on l'envoie       */
        /* ============================================================ */

        /* envoi de la commande au serveur */
        send(sock, buffer, strlen(buffer), 0);

        /* si c'est exit, on attend la confirmation et on quitte */
        if (strcmp(buffer, "exit") == 0) {
            memset(buffer, 0, BUFFER_SIZE);
            valread = read(sock, buffer, BUFFER_SIZE);
            if (valread > 0) {
                printf("%s\n", buffer);
            }
            printf("Deconnexion du broker.\n");
            break;
        }

        /* on lit la reponse du serveur */
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(sock, buffer, BUFFER_SIZE);

        if (valread == 0) {
            printf("Le serveur a ferme la connexion.\n");
            break;
        }

        if (valread < 0) {
            perror("Erreur de lecture");
            printf("Deconnexion inattendue du serveur.\n");
            break;
        }

        printf("%s\n", buffer);

        /* === mise a jour du portefeuille local apres reponse du serveur === */

        /* si c'est un BUY reussi (la reponse commence par "Achat") on ajoute au portefeuille */
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

        /* si c'est un SELL reussi (la reponse commence par "Vente") on retire du portefeuille */
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

    /* on ferme le socket proprement */
    close(sock);

    return 0;
}
