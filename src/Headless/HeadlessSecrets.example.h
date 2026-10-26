// Plantilla de credenciales locales. Copie este archivo como HeadlessSecrets.h
// en la misma carpeta; ese archivo está en .gitignore y no se versiona.
#pragma once

// Red local a la que se conecta el equipo, además del AP de setup.
#define HEADLESS_SETUP_STA_SSID "mi-red"
#define HEADLESS_SETUP_STA_PASSWORD "mi-clave"

// Opcional: contraseña propia para el AP de setup (WPA2, 8 a 63 caracteres).
// #define HEADLESS_SETUP_AP_PASSWORD "otra-clave"
