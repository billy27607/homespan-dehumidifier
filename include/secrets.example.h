#pragma once

// Copy this file to include/secrets.h and set your own values.
// secrets.h is gitignored, so it never gets committed.

// Setup access point password (at least 8 characters)
#define AP_PASSWORD "change-me-ap"

// Dashboard login password (the user name is in config.h)
#define DASHBOARD_PASSWORD "change-me-dashboard"

// OTA upload password: must match --auth in secrets.ini.
// Set on every boot, so it survives a factory reset.
#define OTA_PASSWORD "change-me-ota"
