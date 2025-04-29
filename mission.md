# 📋 Feuille de route Time Tracker ESP32-C3

## Prérequis

- ESP32-C3 "esp32-c3-devkitm-1"
- PlatformIO (VS Code) avec ESP-IDF ≥ v5.x
- Python ≥ 3.7
- Modules matériels : RC522 (SPI), PN532 (I2C/SPI), 1 NeoPixel, etc.
- Librairies externes : cJSON, led_strip, esp_http_client, etc.
- Drivers USB/serial pour ESP32 installés sur votre OS

### Configurer les dépendances ESP-IDF

- Les librairies externes nécessaires (cJSON, led_strip, esp_http_client, etc.) sont soit incluses dans ESP-IDF, soit à ajouter dans `platformio.ini` ou `idf_component.yml` si besoin.
- Pour les modules RFID, voir :
  - [esp-idf-rc522](https://github.com/abobija/esp-idf-rc522)
  - [esp-idf-pn532](https://github.com/garag/esp-idf-pn532)

### Configurer les fichiers de configuration

- Adapter `sdkconfig.defaults` et `Kconfig` selon votre matériel (GPIO, modules actifs).
- Placer vos identifiants WiFi dans `/data/wifi.json` (voir exemple plus bas).

## Architecture du projet

Le projet est organisé pour séparer clairement chaque fonctionnalité dans des composants indépendants, selon les bonnes pratiques ESP-IDF :

- `main/` : point d’entrée du firmware, orchestration générale.
- `components/fs_manager/` : gestion du système de fichiers (LittleFS).
- `components/wifi_manager/` : gestion WiFi, multi-SSID, NTP.
- `components/ap_webserver/` : mode Access Point, serveur HTTP, pages HTML.
- `components/webhook_manager/` : gestion de l’envoi des événements JSON.
- `components/feedback_manager/` : gestion des LEDs NeoPixel et autres feedbacks.
- `components/rfid/rc522/` et `components/rfid/pn532/` : gestion des modules RFID selon le protocole.
- `sdkconfig.defaults`, `Kconfig` : configuration compile-time.
- `platformio.ini` : configuration PlatformIO.

Chaque composant expose une interface claire (header) et peut être activé/désactivé via Kconfig.

```txt
/ (racine du projet)
├── main/
│   ├── main.c
│   └── ... (autres fichiers d’entrée, ex: app_main.c)
├── components/
│   ├── fs_manager/           # Gestion LittleFS, lecture/écriture fichiers
│   │   ├── fs_manager.c
│   │   ├── fs_manager.h
│   │   └── ...
│   ├── wifi_manager/         # Connexion WiFi, multi-SSID, NTP
│   │   ├── wifi_manager.c
│   │   ├── wifi_manager.h
│   │   └── ...
│   ├── ap_webserver/         # Mode AP, serveur HTTP, gestion HTML
│   │   ├── ap_webserver.c
│   │   ├── ap_webserver.h
│   │   └── html/             # Fichiers HTML statiques
│   │       └── index.html
│   ├── webhook_manager/      # Transmission JSON vers serveur distant
│   │   ├── webhook_manager.c
│   │   ├── webhook_manager.h
│   │   └── ...
│   ├── feedback_manager/     # Gestion LED NeoPixel, buzzer, etc.
│   │   ├── feedback_manager.c
│   │   ├── feedback_manager.h
│   │   └── ...
│   ├── rfid/
│   │   ├── rc522/            # Support RC522 (SPI)
│   │   │   ├── rc522.c
│   │   │   ├── rc522.h
│   │   │   └── ...
│   │   ├── pn532/            # Support PN532 (I2C/SPI)
│   │   │   ├── pn532.c
│   │   │   ├── pn532.h
│   │   │   └── ...
│   │   └── rfid_common.h     # Interface commune si besoin
│   └── ...
├── sdkconfig.defaults        # Configurations Kconfig par défaut
├── Kconfig                  # Racine Kconfig pour options globales
├── CMakeLists.txt           # Racine
├── platformio.ini           # Config PlatformIO
└── README.md
```

**Principes :**

- Chaque composant a son dossier dans `/components` (modularité, testabilité, réutilisation).
- Les modules RFID sont séparés par type/protocole, mais peuvent partager une interface commune.
- Les fichiers HTML sont dans un sous-dossier dédié du composant webserver.
- Les fichiers de config (Kconfig, sdkconfig.defaults) sont à la racine.
- `main/` ne contient que le point d’entrée et l’orchestration.

**Bonnes pratiques ESP-IDF :**

- Utiliser les composants pour isoler les fonctionnalités.
- Prévoir des interfaces claires (headers) pour chaque composant.
- Préparer la configuration via Kconfig pour activer/désactiver des modules (ex: RC522/PN532).
- Garder le code métier hors de `main.c` autant que possible.

Exemple de structure de CMakeLists.txt ou de Kconfig pour cette organisation:

- CMakeLists pour `components/fs_manager/`

```txt
idf_component_register(SRCS "fs_manager.c"
                      INCLUDE_DIRS ".")
```

- Kconfig pour `components/rfid/`

```txt
menu "RFID Options"

config USE_RC522
    bool "Activer le support RC522"
    default y

config USE_PN532
    bool "Activer le support PN532"
    default n

endmenu
```

- Kconfig racine

```txt
source "components/rfid/Kconfig"
source "components/fs_manager/Kconfig"
# Ajouter d'autres sources de Kconfig ici
```

---

## Phase 0 — Base de projet ESP-IDF sous PlatformIO ✅

**Objectif :**

- Projet qui compile/flash correctement sur ESP32-C3.

**Actions :**

- `platformio run -t upload` fonctionne sans erreur.
- "Hello World" s'affiche sur la console UART.

**Validation :**

✅ Compilation et upload OK.
✅ Message UART visible.

---

## Phase 1 — Passage à LittleFS et montage au boot ✅

**Objectif :**

- Utiliser LittleFS comme système de fichiers interne par défaut.

**Actions :**

- Ajouter `littlefs` comme composant dans CMakeLists.txt.
- Remplacer `esp_vfs_spiffs_register` par `esp_vfs_littlefs_register`.
- Configurer `board_build.filesystem = littlefs` dans platformio.ini.
- Préparer un fichier `wifi.json` dans /data.
- `pio run --target buildfs` + `uploadfs` à tester par l'usager.

**Validation :**

✅ LittleFS monté avec succès au boot ("LittleFS mounted successfully" dans les logs).
✅ Lecture d'un fichier `wifi.json` affichée sur le terminal.

---

## Phase 2 — Lecture et parsing de `wifi.json`

**Objectif :**

- Lire les identifiants WiFi depuis un fichier JSON

**Actions :**

- Charger le fichier depuis LittleFS.
- Utiliser `cJSON` pour parser les données (tableau de couples SSID/PWD).
- Stocker en RAM les infos parsées.
- Afficher les paires SSID/PWD en UART.

**Validation :**

- Console affiche chaque couple SSID/Mdp trouvé.

**Exemple de `wifi.json` :**

```json
{
  "networks": [
    { "ssid": "HomeWifi", "password": "secret1" },
    { "ssid": "OfficeNet", "password": "secret2" }
  ]
}
```

---

## Phase 3 — Mode Access Point + Mini Webserver (WebConfig)

**Objectif :**

- Fournir une méthode de configuration WiFi via navigateur.

**Actions :**

- Charger le fichier `wifi.json` depuis LittleFS.
- Etablir la connection WiFi.
- Si aucun réseau connu n’est joignable ➔ Démarrer en AP (ex: SSID `time-tracker-setup`).
- Démarrer un serveur HTTP local.
- Servir une page HTML depuis LittleFS permettant d’ajouter/supprimer des SSID.
- Enregistrer les données soumises dans `wifi.json`.

**Validation :**

- On peut se connecter au point d’accès via téléphone/ordi.
- Accès à une interface web minimale (formulaire de SSID/mdp).
- Données correctement sauvegardées dans le fichier.

---

## Phase 4 — Connexion WiFi Multi-SSID

**Objectif :**

- Se connecter automatiquement à un réseau parmi la liste.

**Actions :**

- Lire `wifi.json`.
- Tenter les connexions une par une.
- Afficher IP en cas de succès.

**Validation :**

- Connexion réussie avec l’un des réseaux listés.
- Adresse IP affichée dans le terminal.

---

## Phase 5 — Synchronisation de l’heure avec NTP

**Objectif :**

- Obtenir une heure système précise après connexion.

**Actions :**

- Appeler `esp_sntp_init()`.
- Vérifier l'heure locale avec DST.
- Vérifier que `time()` retourne une valeur valide, format 24h.

**Validation :**

- Console affiche une date/heure locale correcte.

---

## Phase 6 — Lecture RFID (RC522 ou PN532)

**Objectif :**

- Lire des tags RFID via SPI ou I2C.

**Actions :**

- Ajouter configuration via `Kconfig` pour choisir le module RFID et les gpio utilisé.
- Lire l’UID du tag détecté.
- Obtenir l'UID de la puce ESP32C3.

**Validation :**

- UID affiché en hexadécimal dans le terminal pour tag_id et device_id
- Passage dans les différentes configuration fluide via kconfig, (SPI/I2C - RC522/PN532)
- fonctionne avec différent type de tags 4bits, 7bits, 10bits
- résultats concluants dans les différentes configurations

**Ressources :**

- https://github.com/abobija/esp-idf-rc522
- https://github.com/garag/esp-idf-pn532

---

## Phase 7 — Transmission JSON vers Webhook

**Objectif :**

- Envoyer l’info tag (UID, timestamp, device ID) à un serveur.

**Actions :**

- Construire un payload JSON.
- Poster les données via HTTP (avec `esp_http_client`).

**Validation :**

- Console : "Event sent successfully".
- Webhook distant reçoit le JSON.

**Ressources :**

- /migration/webhook_manager

---

## Phase 8 — Feedback utilisateur via LED NeoPixel

**Objectif :**

- Indiquer l’état du système via couleur LED.

**Actions :**

- Intégrer `led_strip.h` et RMT.
- Couleurs :
  - Bleu : WiFi OK
  - Vert : tag détecté
  - Rouge : erreur / pas de WiFi

**Validation :**

- LED change dynamiquement selon l’état.

**Ressources :**

- /migration/

---

## Annexe : gestion de config via Kconfig

- Utiliser des `Kconfig` pour gérer les options compile-time.
- Ajouter par exemple `CONFIG_USE_RC522`, `CONFIG_USE_PN532`, `CONFIG_WEBHOOK_URL`.
- PlatformIO lit sdkconfig.defaults à la compilation.

---

## Résumé des dépendances

| Phase | Nom                  | Dépendances      |
|-------|----------------------|------------------|
| 0     | Base projet          | —                |
| 1     | LittleFS             | 0                |
| 2     | Lecture config       | 1                |
| 3     | WebConfig            | 1, 2             |
| 4     | Connexion WiFi       | 2                |
| 5     | NTP                  | 4                |
| 6     | Lecture RFID         | 0                |
| 7     | Webhook              | 4, 5, 6          |
| 8     | LED feedback         | 4, 6, 7          |
