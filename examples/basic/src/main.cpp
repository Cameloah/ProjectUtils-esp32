// Minimal project using ProjectUtils: WiFi setup portal, settings page at /settings,
// web console at /webserial, log at /api/pu/log, and one project-specific setting.

#include <Arduino.h>
#include <ProjectUtils.h>

pu::Config config;
pu::Settings appSettings; // the project's own persistent settings

void handleRoot(AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Hello from " + ProjectUtils.wifi().deviceName());
}

void setup() {
    Serial.begin(115200);

    config.deviceName = "My ESP32";
    config.firmwareVersion = pu::Semver{0, 0, 1};
    // config.adminPassword = "change-me";          // protect settings, API, console and OTA
    // config.githubRepo = "your-user/your-repo";   // enable updates from GitHub releases
    ProjectUtils.begin(config);

    appSettings.begin("app", &ProjectUtils.log());
    appSettings.define("bootCount", 0);
    appSettings.define("greeting", "hello");
    appSettings.loadAll();
    appSettings.set("bootCount", static_cast<int>(appSettings.getInt("bootCount")) + 1);

    ProjectUtils.server().on("/", HTTP_GET, handleRoot);
    ProjectUtils.log().info("Boot number %d", static_cast<int>(appSettings.getInt("bootCount")));
}

void loop() {
    ProjectUtils.loop();

    // Anything typed on USB serial or in the web console is echoed back on both.
    Stream& console = ProjectUtils.console();
    if (console.available() > 0) {
        const String line = console.readStringUntil('\n');
        if (line == "log") {
            ProjectUtils.log().printTo(console);
        } else {
            console.println("You said: " + line);
        }
    }
}
