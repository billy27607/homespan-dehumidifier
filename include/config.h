#pragma once

// Passwords live in secrets.h, which is gitignored.
// First build: cp include/secrets.example.h include/secrets.h  and edit it.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "include/secrets.h not found: copy include/secrets.example.h to include/secrets.h and set your passwords"
#endif

// ---- Firmware ----------------------------------------------------------------
#define FW_VERSION "0.10.0"

// ---- Setup access point / captive portal ------------------------------------
// Join this network from a phone to configure Wi-Fi (password: AP_PASSWORD in secrets.h).
#define AP_SSID "Dehumidifier-Setup"

// Captive portal gives up and reboots after this long without a save.
#define AP_TIMEOUT_MS (5UL * 60UL * 1000UL)

// If saved Wi-Fi never connects within this long after boot, launch the portal.
#define WIFI_FALLBACK_MS (3UL * 60UL * 1000UL)

// ---- Dashboard --------------------------------------------------------------
// HomeSpan's HomeKit server owns port 80, so the dashboard lives on its own port.
#define DASHBOARD_PORT 8080
#define DASHBOARD_USER "admin" // password: DASHBOARD_PASSWORD in secrets.h
