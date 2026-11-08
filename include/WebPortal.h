#pragma once

// Web interface for the dehumidifier:
//   - Captive-portal Wi-Fi setup served from an internal access point
//     (replaces HomeSpan's built-in AP via homeSpan.setApFunction()).
//   - Password-protected dashboard on DASHBOARD_PORT with live status,
//     pump-out, and a button to re-enter Wi-Fi setup.

// Call in setup() BEFORE homeSpan.begin().
void webPortalInit();

// Call from loop() after homeSpan.poll().
void webPortalLoop();
