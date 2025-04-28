# 📋 Voici la **feuille de route** structurée que je te propose

---

## 🔵 Phase 0 — Base de projet ESP-IDF sous PlatformIO

**Objectif** :  

- Avoir un projet qui compile/flash proprement sur ton ESP32-C3.

**Actions :**

1. Créer un projet PlatformIO basé sur `framework = espidf`.
2. S'assurer que `/src/main.c` compile même avec juste un `app_main()` vide.
3. Configurer l'upload via USB (UART).

**Critères de validation :**

- La commande `platformio run -t upload` fonctionne sans erreur.
- Ton ESP32C3 boote et tu vois "Hello World" en UART console.

---

## 🔵 Phase 1 — Montage SPIFFS (filesystem interne)

**Objectif** :  

- ESP32 doit pouvoir monter un système de fichier SPIFFS.

**Actions :**

1. Ajouter `spiffs` comme composant dans `CMakeLists.txt`.
2. Initialiser SPIFFS au boot (`esp_vfs_spiffs_register`).

**Critères de validation :**

- À chaque boot, tu vois dans la console : `"SPIFFS mounted successfully"`.
- Aucun crash même si la mémoire SPIFFS est vide.

---

## 🔵 Phase 2 — Gestion fichier SSID/PWD sur SPIFFS

**Objectif** :  
- Lire un fichier `wifi_config.txt` stocké dans SPIFFS.

**Actions :**
1. Si le fichier existe, lire ligne par ligne (`fgets`).
2. Stocker SSID/PWD en mémoire (parsing `;`).

**Critères de validation :**
- Console affiche chaque SSID/PWD trouvé.

Exemple de log attendu :
```
Found WiFi credentials:
SSID: HomeWifi, PASSWORD: secret1
SSID: OfficeNet, PASSWORD: secret2
```

---

## 🔵 Phase 3 — Saisie UART interactive si fichier absent

**Objectif** :  
- Si pas de fichier wifi_config.txt ➔ demander à l'utilisateur via USB/UART.

**Actions :**
1. Attendre entrée utilisateur pour SSID et PASSWORD.
2. Écrire dans SPIFFS un nouveau fichier `wifi_config.txt`.

**Critères de validation :**
- Après saisie, fichier visible dans SPIFFS.
- Redémarrage → fichier est relu correctement.

---

## 🔵 Phase 4 — Connexion WiFi Multi-SSID

**Objectif** :  
- Connecter l'ESP32 à un des SSID listés dans le fichier.

**Actions :**
1. Essayer chaque SSID/PASSWORD.
2. Si succès, arrêter la boucle.

**Critères de validation :**
- Console affiche `"Connected to HomeWifi"` ou `"Connected to OfficeNet"`.
- Récupérer une IP.

---

## 🔵 Phase 5 — Synchro NTP

**Objectif** :  
- Synchroniser l'horloge du module avec un serveur NTP.

**Actions :**
1. Appeler `esp_sntp_init()`.
2. Attendre que `time()` retourne une date valide.

**Critères de validation :**
- La console affiche la date/heure correcte (UTC).

---

## 🔵 Phase 6 — Lecture RFID (RC522 ou PN532)

**Objectif** :  
- Lire un UID de tag RFID.

**Actions :**
1. Choisir en fonction de `#ifdef CONFIG_USE_RC522` ou `CONFIG_USE_PN532`.
2. Init SPI ou I2C selon le module.
3. Lire l'UID.

**Critères de validation :**
- Lorsqu'un tag est scanné, console affiche `"Tag detected: 0xAABBCCDD"`.

---

## 🔵 Phase 7 — Envoi JSON via Webhook

**Objectif** :  
- Poster l'évènement tag (UID, timestamp) sur un serveur Webhook.

**Actions :**
1. Construire payload JSON.
2. Envoyer HTTP POST avec `esp_http_client`.

**Critères de validation :**
- Console : `"Event sent successfully"`.
- Côté serveur : JSON reçu correctement.

---

## 🔵 Phase 8 — Gestion LED Neopixel (feedback)

**Objectif** :  
- Donner un retour visuel (LED) sur état WiFi / RFID détecté.

**Actions :**
1. Piloter LED avec `rmt` + `led_strip.h` officiel de ESP-IDF.
2. Bleu pour "WiFi ok", vert pour "Tag détecté", rouge si erreur.

**Critères de validation :**
- La LED change de couleur selon l'état sans bloquer les autres tâches.

---

# 🔶 À propos de la config (Kconfig vs config.h)

En ESP-IDF :
- **`Kconfig`** sert à générer un `sdkconfig` automatiquement via `menuconfig` ou via PlatformIO directement.
- C'est **plus propre** qu'un `config.h` manuel.

Typiquement tu ajoutes dans ton composant un fichier `Kconfig` du genre :

```kconfig
menu "My Application Configuration"

config WIFI_MAX_RETRY
    int "Maximum WiFi retries"
    default 5

config USE_RC522
    bool "Use RC522 RFID Module"
    default y

config USE_PN532
    bool "Use PN532 RFID Module"
    default n

endmenu
```

Et dans ton code C tu utilises :

```c
#if CONFIG_USE_RC522
// Code pour RC522
#elif CONFIG_USE_PN532
// Code pour PN532
#endif
```

**Dans PlatformIO**, tu peux modifier `sdkconfig.defaults` pour avoir des valeurs par défaut ou utiliser la commande `pio run -t menuconfig`.

---

# 🛠️ Résumé final de ta stratégie de dev

| Phase       | Fonctionnalité         | Dépend de        | Validation clé |
|-------------|-------------------------|------------------|----------------|
| 0           | Projet de base           | Aucun            | Compile, USB OK |
| 1           | SPIFFS                   | Base             | Montage OK |
| 2           | Lecture fichier SSID     | SPIFFS           | Lecture OK |
| 3           | UART entrée manuelle     | SPIFFS           | Fichier créé |
| 4           | Connexion WiFi            | Fichier SSID     | Connexion réussie |
| 5           | NTP Sync                  | WiFi connecté    | Heure valide |
| 6           | RFID Lecture              | Base             | UID détecté |
| 7           | Webhook POST              | WiFi + RFID      | POST OK |
| 8           | LED feedback              | Tout             | LED changement |

---

Veux-tu aussi que je t'aide à rédiger un premier `platformio.ini` propre pour ce projet (avec partition spiffs déjà configuré, sdkconfig.defaults de base et tout clean) ?  
Ça peut te faire gagner du temps et te donner une base solide pour suivre cette feuille de route. 🚀