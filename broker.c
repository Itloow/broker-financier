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
#define MAX_PORTFOLIO 20
#define LOG_FILE "broker.log"

/* les 3 structs du projet */

typedef struct {
    char nom[50];
    float prix;
    int quantite;
} produit_t;

/* portefeuille pour tracker ce que chaque client possede */
typedef struct {
    char nom[50];
    int quantite;
} portfolio_t;

/* infos du client qu'on passe au thread */
typedef struct {
    int socket_fd;
    struct sockaddr_in client_addr;
} client_info_t;

/* les 5 actions dispo sur le broker */
produit_t produits[NB_PRODUITS] = {
    {"Apple",     180.50, 100},
    {"Google",    140.25, 80},
    {"Tesla",     250.75, 50},
    {"Amazon",    185.00, 120},
    {"Microsoft", 420.30, 90}
};

/* log dans la console + dans broker.log en meme temps */
void ecrire_log(const char *message) {
    printf("%s\n", message);

    FILE *f = fopen(LOG_FILE, "a");
    if (f == NULL) {
        perror("[ERREUR] Impossible d'ouvrir le fichier de log");
        return;
    }
    fprintf(f, "%s\n", message);
    fclose(f);
}

/* fonction executee par chaque thread, un thread = un client */
void *handle_client(void *arg) {
    client_info_t *info = (client_info_t *)arg;
    int client_socket = info->socket_fd;
    char *client_ip = inet_ntoa(info->client_addr.sin_addr);
    int client_port = ntohs(info->client_addr.sin_port);
    char buffer[BUFFER_SIZE] = {0};

    /* chaque client a son propre portefeuille cote serveur */
    portfolio_t portefeuille[MAX_PORTFOLIO];
    int nb_actifs = 0;
    memset(portefeuille, 0, sizeof(portefeuille));

    char log_msg[BUFFER_SIZE * 2] = {0};
    sprintf(log_msg, "[LOG] Client connecte : %s:%d", client_ip, client_port);
    ecrire_log(log_msg);

    /* premier truc qu'on envoie au client */
    char *welcome = "Bienvenue sur le Broker Financier.";
    if (send(client_socket, welcome, strlen(welcome), 0) < 0) {
        perror("[ERREUR] Envoi bienvenue echoue");
        close(client_socket);
        free(info);
        pthread_exit(NULL);
    }

    /* boucle principale : on attend les commandes du client */
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int valread = read(client_socket, buffer, BUFFER_SIZE - 1);

        /* le client a ferme la connexion */
        if (valread == 0) {
            sprintf(log_msg, "[LOG] Client %s:%d a ferme la connexion.", client_ip, client_port);
            ecrire_log(log_msg);
            break;
        }

        /* erreur reseau ou crash client */
        if (valread < 0) {
            perror("[ERREUR] Erreur de lecture");
            sprintf(log_msg, "[LOG] Deconnexion inattendue du client %s:%d", client_ip, client_port);
            ecrire_log(log_msg);
            break;
        }

        /* on termine la chaine proprement pour eviter du garbage */
        buffer[valread] = '\0';

        sprintf(log_msg, "[LOG] Commande recue de %s:%d : %s", client_ip, client_port, buffer);
        ecrire_log(log_msg);

        /* le client veut quitter */
        if (strcmp(buffer, "exit") == 0) {
            sprintf(log_msg, "[LOG] Client %s:%d a demande a quitter.", client_ip, client_port);
            ecrire_log(log_msg);
            char *bye = "Deconnexion confirmee. A bientot.";
            send(client_socket, bye, strlen(bye), 0);
            break;
        }

        /* on decoupe le buffer en commande + argument */
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

        char response[BUFFER_SIZE * 2] = {0};

        /* ---- LIST : afficher tous les produits ---- */
        if (strcmp(commande, "LIST") == 0) {
            sprintf(response, "--- Produits disponibles ---\n");
            for (int j = 0; j < NB_PRODUITS; j++) {
                char ligne[100] = {0};
                sprintf(ligne, "%s  |  Prix : %.2f  |  Qte : %d\n",
                        produits[j].nom, produits[j].prix, produits[j].quantite);
                strcat(response, ligne);
            }
            strcat(response, "----------------------------");

        /* ---- INFO : details d'un produit ---- */
        } else if (strcmp(commande, "INFO") == 0) {
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

        /* ---- BUY : acheter des actions ---- */
        } else if (strcmp(commande, "BUY") == 0) {
            if (strlen(argument) == 0) {
                sprintf(response, "Usage : BUY <nom_produit> <quantite>");
            } else {
                /* on separe le nom du produit et la quantite */
                char nom_produit[50] = {0};
                char quantite_str[20] = {0};
                int k = 0;

                while (argument[k] != '\0' && argument[k] != ' ') {
                    nom_produit[k] = argument[k];
                    k++;
                }
                nom_produit[k] = '\0';

                if (argument[k] == ' ') {
                    k++;
                    strncpy(quantite_str, argument + k, 19);
                }

                if (strlen(quantite_str) == 0) {
                    sprintf(response, "Usage : BUY <nom_produit> <quantite>");
                } else {
                    int qte = atoi(quantite_str);

                    if (qte <= 0) {
                        sprintf(response, "Quantite invalide. Veuillez entrer un nombre superieur a 0.");
                    } else {
                        int trouve = 0;
                        for (int j = 0; j < NB_PRODUITS; j++) {
                            if (strcmp(nom_produit, produits[j].nom) == 0) {
                                trouve = 1;

                                /* pas assez en stock */
                                if (qte > produits[j].quantite) {
                                    sprintf(response, "Stock insuffisant pour '%s'.\nDemande : %d | Disponible : %d",
                                            produits[j].nom, qte, produits[j].quantite);

                                    sprintf(log_msg, "[LOG] %s:%d achat refuse : %d x %s (stock insuffisant)",
                                            client_ip, client_port, qte, produits[j].nom);
                                    ecrire_log(log_msg);
                                } else {
                                    /* achat ok : on retire du stock broker */
                                    produits[j].quantite = produits[j].quantite - qte;
                                    float cout_total = produits[j].prix * qte;

                                    sprintf(response, "Achat effectue !\nProduit : %s\nQuantite achetee : %d\nPrix unitaire : %.2f\nCout total : %.2f\nStock restant broker : %d",
                                            produits[j].nom, qte, produits[j].prix, cout_total, produits[j].quantite);

                                    /* on ajoute au portefeuille du client cote serveur */
                                    int found = 0;
                                    int p;
                                    for (p = 0; p < nb_actifs; p++) {
                                        if (strcmp(portefeuille[p].nom, nom_produit) == 0) {
                                            portefeuille[p].quantite = portefeuille[p].quantite + qte;
                                            found = 1;
                                            break;
                                        }
                                    }
                                    /* produit pas encore dans le portefeuille, on le cree */
                                    if (found == 0 && nb_actifs < MAX_PORTFOLIO) {
                                        strncpy(portefeuille[nb_actifs].nom, nom_produit, 49);
                                        portefeuille[nb_actifs].quantite = qte;
                                        nb_actifs++;
                                    }

                                    sprintf(log_msg, "[LOG] %s:%d a achete %d x %s (cout: %.2f)",
                                            client_ip, client_port, qte, produits[j].nom, cout_total);
                                    ecrire_log(log_msg);
                                }
                                break;
                            }
                        }
                        if (trouve == 0) {
                            sprintf(response, "Produit '%s' introuvable.", nom_produit);
                        }
                    }
                }
            }

        /* ---- SELL : vendre des actions au broker ---- */
        } else if (strcmp(commande, "SELL") == 0) {
            if (strlen(argument) == 0) {
                sprintf(response, "Usage : SELL <nom_produit> <quantite>");
            } else {
                char nom_produit[50] = {0};
                char quantite_str[20] = {0};
                int k = 0;

                while (argument[k] != '\0' && argument[k] != ' ') {
                    nom_produit[k] = argument[k];
                    k++;
                }
                nom_produit[k] = '\0';

                if (argument[k] == ' ') {
                    k++;
                    strncpy(quantite_str, argument + k, 19);
                }

                if (strlen(quantite_str) == 0) {
                    sprintf(response, "Usage : SELL <nom_produit> <quantite>");
                } else {
                    int qte = atoi(quantite_str);

                    if (qte <= 0) {
                        sprintf(response, "Quantite invalide. Veuillez entrer un nombre superieur a 0.");
                    } else {
                        int trouve = 0;
                        for (int j = 0; j < NB_PRODUITS; j++) {
                            if (strcmp(nom_produit, produits[j].nom) == 0) {
                                trouve = 1;

                                /* on check combien le client possede */
                                int possede = 0;
                                int idx_portf = -1;
                                int p;
                                for (p = 0; p < nb_actifs; p++) {
                                    if (strcmp(portefeuille[p].nom, nom_produit) == 0) {
                                        possede = portefeuille[p].quantite;
                                        idx_portf = p;
                                        break;
                                    }
                                }

                                /* il a pas assez */
                                if (possede < qte) {
                                    sprintf(response, "Fonds insuffisants pour vendre '%s'.\nDemande : %d | Vous possedez : %d",
                                            nom_produit, qte, possede);

                                    sprintf(log_msg, "[LOG] %s:%d vente refusee : %d x %s (possede: %d)",
                                            client_ip, client_port, qte, nom_produit, possede);
                                    ecrire_log(log_msg);
                                } else {
                                    /* vente ok : on remet dans le stock broker */
                                    produits[j].quantite = produits[j].quantite + qte;
                                    float gain_total = produits[j].prix * qte;

                                    /* on retire du portefeuille du client */
                                    portefeuille[idx_portf].quantite = portefeuille[idx_portf].quantite - qte;

                                    sprintf(response, "Vente effectuee !\nProduit : %s\nQuantite vendue : %d\nPrix unitaire : %.2f\nGain total : %.2f\nStock restant broker : %d",
                                            produits[j].nom, qte, produits[j].prix, gain_total, produits[j].quantite);

                                    sprintf(log_msg, "[LOG] %s:%d a vendu %d x %s (gain: %.2f)",
                                            client_ip, client_port, qte, produits[j].nom, gain_total);
                                    ecrire_log(log_msg);
                                }
                                break;
                            }
                        }
                        if (trouve == 0) {
                            sprintf(response, "Produit '%s' introuvable.", nom_produit);
                        }
                    }
                }
            }

        /* ---- commande pas reconnue ---- */
        } else {
            sprintf(response, "Commande inconnue : %s\nCommandes disponibles : LIST, INFO <produit>, BUY <produit> <quantite>, SELL <produit> <quantite>", commande);

            sprintf(log_msg, "[LOG] %s:%d commande inconnue : %s", client_ip, client_port, commande);
            ecrire_log(log_msg);
        }

        /* on envoie la reponse, si ca echoue on degage */
        if (send(client_socket, response, strlen(response), 0) < 0) {
            perror("[ERREUR] Envoi echoue");
            sprintf(log_msg, "[LOG] Erreur envoi vers %s:%d, fermeture.", client_ip, client_port);
            ecrire_log(log_msg);
            break;
        }
    }

    /* nettoyage : on ferme le socket, on free et on quitte le thread */
    close(client_socket);
    sprintf(log_msg, "[LOG] Socket du client %s:%d ferme. Thread termine.", client_ip, client_port);
    ecrire_log(log_msg);
    free(info);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    /* creation du socket tcp */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    /* on ecoute sur toutes les interfaces, port 8080 */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Erreur bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Erreur listen");
        exit(EXIT_FAILURE);
    }

    char log_msg[BUFFER_SIZE * 2] = {0};
    sprintf(log_msg, "[LOG] Serveur broker demarre sur le port %d", PORT);
    ecrire_log(log_msg);

    /* boucle infinie : on accepte les clients un par un */
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                 (socklen_t *)&addrlen)) < 0) {
            perror("Erreur accept");
            continue;
        }

        /* on malloc les infos du client pour les passer au thread */
        client_info_t *info = malloc(sizeof(client_info_t));
        if (info == NULL) {
            perror("Erreur malloc");
            close(new_socket);
            continue;
        }
        info->socket_fd = new_socket;
        info->client_addr = address;

        /* un thread par client */
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)info) != 0) {
            perror("Erreur pthread_create");
            free(info);
            close(new_socket);
            continue;
        }

        /* detach pour qu'il se clean tout seul a la fin */
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
