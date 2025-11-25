#ifndef WEB_ROUTES_H
#define WEB_ROUTES_H

#include <ESPAsyncWebServer.h>
#include <WiFi.h>

void setupWebRoutes(AsyncWebServer &server) {
    // Root page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = "<html><head><title>LoRa Gateway</title></head><body>";
        html += "<h1>LoRa-MQTT Gateway</h1>";
        html += "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>";
        html += "<p>Access the <a href=\"/webserial\" target=\"_blank\">WebSerial Log</a>.</p>";
        html += "<br>";
        html += "<form action=\"/reboot\" method=\"post\" onsubmit=\"return confirm('Are you sure you want to reboot the device?');\">";
        html += "<button type=\"submit\">Reboot Device</button>";
        html += "</body></html>";
        request->send(200, "text/html", html);
    });

    // Reboot endpoint
    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Rebooting device...");
        // Delay slightly to ensure the response is sent before restarting
        delay(100);
        ESP.restart();
    });
}

#endif // WEB_ROUTES_H
