#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PORTFOLIO 20
#define SOLDE_INITIAL 10000.00

/* un actif dans le portefeuille */
typedef struct {
    char nom[50];
    int quantite;
} portfolio_t;

/* le portefeuille du client, stocke en local */
portfolio_t portefeuille[MAX_PORTFOLIO];
int nb_actifs = 0;

/* solde du client pour acheter des actions */
float solde = SOLDE_INITIAL;

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    /* creation du socket tcp */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    /* conversion de l'ip */
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Adresse invalide");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* on se connecte */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Echec de la connexion");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connecte au broker.\n");

    /* message de bienvenue */
    memset(buffer, 0, BUFFER_SIZE);
    int valread = read(sock, buffer, BUFFER_SIZE - 1);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("%s\n", buffer);
    } else {
        printf("Impossible de recevoir le message de bienvenue.\n");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("Commandes disponibles : LIST, INFO <produit>, BUY <produit> <qte>, SELL <produit> <qte>, WALLET, exit\n");
    printf("Solde initial : %.2f\n\n", solde);

    while (1) {
        printf("Commande > ");

        /* si fgets retourne NULL = ctrl+d ou erreur, on quitte */
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("\nFin de saisie, deconnexion.\n");
            break;
        }
        buffer[strcspn(buffer, "\n")] = 0;

        /* rien tape, on skip */
        if (strlen(buffer) == 0) {
            continue;
        }

        /* wallet = affichage local, pas besoin du serveur */
        if (strcmp(buffer, "WALLET") == 0) {
            printf("--- Portefeuille client ---\n");
            printf("Solde disponible : %.2f\n", solde);
            if (nb_actifs == 0) {
                printf("Aucun actif en portefeuille.\n");
            } else {
                int i;
                for (i = 0; i < nb_actifs; i++) {
                    printf("%s  |  Qte : %d\n",
                           portefeuille[i].nom, portefeuille[i].quantite);
                }
            }
            printf("---------------------------\n");
            continue;
        }

        /* on garde la commande de cote pour apres */
        char cmd_save[BUFFER_SIZE] = {0};
        strncpy(cmd_save, buffer, BUFFER_SIZE - 1);

        /* on decoupe en 3 : commande, produit, quantite */
        char cmd[BUFFER_SIZE] = {0};
        char arg_produit[50] = {0};
        char arg_qte_str[20] = {0};
        int i = 0;

        /* 1er mot */
        while (cmd_save[i] != '\0' && cmd_save[i] != ' ') {
            cmd[i] = cmd_save[i];
            i++;
        }
        cmd[i] = '\0';

        /* 2eme mot */
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

        /* 3eme mot */
        if (cmd_save[i] == ' ') {
            i++;
            strncpy(arg_qte_str, cmd_save + i, 19);
        }

        /* --- verifs avant d'envoyer --- */

        if (strcmp(cmd, "BUY") == 0 || strcmp(cmd, "SELL") == 0) {

            /* pas de produit */
            if (strlen(arg_produit) == 0) {
                printf("Erreur : il manque le nom du produit.\n");
                printf("Usage : %s <produit> <quantite>\n", cmd);
                continue;
            }

            /* pas de quantite */
            if (strlen(arg_qte_str) == 0) {
                printf("Erreur : il manque la quantite.\n");
                printf("Usage : %s <produit> <quantite>\n", cmd);
                continue;
            }

            /* quantite negative ou nulle */
            int qte_check = atoi(arg_qte_str);
            if (qte_check <= 0) {
                printf("Erreur : la quantite doit etre superieure a 0.\n");
                continue;
            }

            /* pour sell on check le portefeuille */
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

        /* info sans produit */
        if (strcmp(cmd, "INFO") == 0 && strlen(arg_produit) == 0) {
            printf("Erreur : il manque le nom du produit.\n");
            printf("Usage : INFO <produit>\n");
            continue;
        }

        /* c'est bon, on envoie */
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Erreur envoi");
            printf("Impossible d'envoyer la commande, deconnexion.\n");
            break;
        }

        /* exit */
        if (strcmp(buffer, "exit") == 0) {
            memset(buffer, 0, BUFFER_SIZE);
            valread = read(sock, buffer, BUFFER_SIZE - 1);
            if (valread > 0) {
                buffer[valread] = '\0';
                printf("%s\n", buffer);
            }
            printf("Deconnexion du broker.\n");
            break;
        }

        /* reponse du serveur */
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(sock, buffer, BUFFER_SIZE - 1);

        if (valread == 0) {
            printf("Le serveur a ferme la connexion.\n");
            break;
        }

        if (valread < 0) {
            perror("Erreur de lecture");
            printf("Deconnexion inattendue du serveur.\n");
            break;
        }

        /* on termine la chaine proprement */
        buffer[valread] = '\0';

        printf("%s\n", buffer);

        /* buy reussi -> on ajoute au portefeuille et on sync le solde */
        if (strcmp(cmd, "BUY") == 0 && strlen(arg_produit) > 0
            && strlen(arg_qte_str) > 0
            && buffer[0] == 'A' && buffer[1] == 'c' && buffer[2] == 'h'
            && buffer[3] == 'a' && buffer[4] == 't') {
            int qte = atoi(arg_qte_str);

            /* on cherche "Solde restant : " dans la reponse pour sync */
            char marqueur[] = "Solde restant : ";
            int m_len = strlen(marqueur);
            int b = 0;
            while (buffer[b] != '\0') {
                int match = 1;
                int c;
                for (c = 0; c < m_len; c++) {
                    if (buffer[b + c] != marqueur[c]) {
                        match = 0;
                        break;
                    }
                }
                if (match == 1) {
                    /* on lit le nombre apres le marqueur */
                    char solde_str[20] = {0};
                    int s = 0;
                    int pos = b + m_len;
                    while (buffer[pos] != '\0' && buffer[pos] != '\n' && s < 19) {
                        solde_str[s] = buffer[pos];
                        pos++;
                        s++;
                    }
                    /* conversion manuelle du float */
                    int partie_ent = atoi(solde_str);
                    float nouveau_solde = partie_ent;
                    int d = 0;
                    while (solde_str[d] != '\0' && solde_str[d] != '.') {
                        d++;
                    }
                    if (solde_str[d] == '.') {
                        d++;
                        int partie_dec = atoi(solde_str + d);
                        float diviseur = 1.0;
                        while (solde_str[d] >= '0' && solde_str[d] <= '9') {
                            diviseur = diviseur * 10;
                            d++;
                        }
                        nouveau_solde = partie_ent + (partie_dec / diviseur);
                    }
                    solde = nouveau_solde;
                    break;
                }
                b++;
            }

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

        /* sell reussi -> on retire du portefeuille et on sync le solde */
        if (strcmp(cmd, "SELL") == 0 && strlen(arg_produit) > 0
            && strlen(arg_qte_str) > 0
            && buffer[0] == 'V' && buffer[1] == 'e' && buffer[2] == 'n'
            && buffer[3] == 't' && buffer[4] == 'e') {
            int qte = atoi(arg_qte_str);

            /* on cherche "Solde restant : " dans la reponse pour sync */
            char marqueur[] = "Solde restant : ";
            int m_len = strlen(marqueur);
            int b = 0;
            while (buffer[b] != '\0') {
                int match = 1;
                int c;
                for (c = 0; c < m_len; c++) {
                    if (buffer[b + c] != marqueur[c]) {
                        match = 0;
                        break;
                    }
                }
                if (match == 1) {
                    char solde_str[20] = {0};
                    int s = 0;
                    int pos = b + m_len;
                    while (buffer[pos] != '\0' && buffer[pos] != '\n' && s < 19) {
                        solde_str[s] = buffer[pos];
                        pos++;
                        s++;
                    }
                    int partie_ent = atoi(solde_str);
                    float nouveau_solde = partie_ent;
                    int d = 0;
                    while (solde_str[d] != '\0' && solde_str[d] != '.') {
                        d++;
                    }
                    if (solde_str[d] == '.') {
                        d++;
                        int partie_dec = atoi(solde_str + d);
                        float diviseur = 1.0;
                        while (solde_str[d] >= '0' && solde_str[d] <= '9') {
                            diviseur = diviseur * 10;
                            d++;
                        }
                        nouveau_solde = partie_ent + (partie_dec / diviseur);
                    }
                    solde = nouveau_solde;
                    break;
                }
                b++;
            }

            int j;
            for (j = 0; j < nb_actifs; j++) {
                if (strcmp(portefeuille[j].nom, arg_produit) == 0) {
                    portefeuille[j].quantite = portefeuille[j].quantite - qte;
                    break;
                }
            }
        }
    }

    close(sock);

    return 0;
}
