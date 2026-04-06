# Broker Financier

Projet 1 — Licence 3 MIASHS — UE MI0C603T Programmation système et réseau  
Université Toulouse Jean Jaurès — 2025/2026

## Description

Système client-serveur en C simulant un broker financier.  
Le serveur gère plusieurs clients simultanément via des threads POSIX et permet la consultation de produits financiers ainsi que l'achat et la vente d'actifs.  
Chaque client dispose d'un solde initial de 10 000€ et le broker dispose de 50 000€ de fonds propres.

## Auteurs

- Olti
- Hyacinthe
- Karim

## Architecture

Le projet repose sur une architecture TCP client-serveur :

- **broker.c** : le serveur écoute sur le port 8080 et accepte les connexions entrantes. Chaque client est géré dans un thread dédié (`pthread`), ce qui permet plusieurs connexions simultanées. Le serveur maintient un stock de 5 produits financiers, un portefeuille par client, et un fichier de log (`broker.log`) écrit en temps réel.

- **client.c** : le client se connecte au serveur, envoie des commandes saisies par l'utilisateur et affiche les réponses. Il maintient également un portefeuille local et un solde synchronisé avec le serveur.

## Compilation

```bash
make
```

Ou manuellement :

```bash
gcc -Wall -Wextra broker.c -o broker -lpthread
gcc -Wall -Wextra client.c -o client
```

## Exécution

Lancer le serveur :

```bash
./broker
```

Lancer un ou plusieurs clients (dans d'autres terminaux) :

```bash
./client
```

Pour tester sur deux machines différentes, modifier `SERVER_IP` dans `client.c` avec l'adresse IP du serveur (récupérable via `hostname -I` sur la machine serveur).

## Commandes disponibles

| Commande | Description |
|---|---|
| `LIST` | Affiche tous les produits avec leur prix et quantité disponible |
| `INFO <produit>` | Affiche les détails d'un produit spécifique |
| `BUY <produit> <quantité>` | Achète des actions (vérifie le stock et le solde client) |
| `SELL <produit> <quantité>` | Vend des actions au broker (vérifie le portefeuille et les fonds broker) |
| `WALLET` | Affiche le portefeuille et le solde du client (local, sans requête serveur) |
| `exit` | Déconnexion propre du broker |

## Produits disponibles

| Produit | Prix unitaire | Quantité initiale |
|---|---|---|
| Airbus | 312.50€ | 75 |
| TotalEnergies | 47.80€ | 140 |
| BNP | 89.15€ | 60 |
| Renault | 203.00€ | 95 |
| Orange | 15.60€ | 180 |

## Logs

Le serveur écrit chaque événement (connexion, commande, achat, vente, erreur, déconnexion) dans la console et dans le fichier `broker.log` en temps réel. Le fichier est ouvert et fermé à chaque écriture pour garantir la persistance même en cas de crash.

## Nettoyage

```bash
make clean
```
