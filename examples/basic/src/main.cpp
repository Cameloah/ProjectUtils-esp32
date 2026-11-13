// Example project using ProjectUtils.
//
// What you get without writing any HTML:
//   /            the home page declared below
//   /settings    WiFi and device name      /update   firmware upload and online updates
//   /console     web console               /info     device info and log
//
// On first start the device opens a WiFi access point; connect to it to enter your WiFi.

#include <Arduino.h>
#include <ProjectUtils.h>

pu::Config config;
pu::Settings appSettings; // the project's own persistent settings

// State shown and changed on the home page
bool lampOn = false;
bool motorRunning = false;
int clicks = 0;
double volume = 30;

// A page written by hand. The two /pu/ files give it the shared style and the menu.
const char kAboutHtml[] = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>About</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="/pu/app.css">
  <script src="/pu/app.js"></script>
</head>
<body>
  <h1>About</h1>
  <div class="content">
    <div class="card">
      <p>This page is plain HTML from the example sketch.</p>
      <p>Free memory: <span class="value" id="heap"></span></p>
    </div>
  </div>
  <script>
    pu.get("/api/pu/info").then((info) => {
      document.getElementById("heap").textContent = Math.round(info.heapFree / 1024) + " KB";
    });
  </script>
</body>
</html>)HTML";

void buildHomePage() {
    pu::Page& home = ProjectUtils.web().home("My ESP32");

    home.section("Settings"); // inputs bound to settings: loaded, checked and saved for you
    home.text(appSettings, "greeting", "Greeting");
    home.number(appSettings, "interval", "Interval").unit("s").min(0.1).max(60).step(0.1);
    home.slider(appSettings, "brightness", "Brightness").unit("%");
    home.toggle(appSettings, "nightMode", "Night mode");
    home.select(appSettings, "speed", "Speed", {"slow", "normal", "fast"});

    home.section("Controls"); // inputs and buttons bound to your own code
    home.toggle("Lamp", []() { return lampOn; }, [](bool on) { lampOn = on; });
    home.slider("Volume", []() { return volume; }, [](double value) { volume = value; });
    home.holdButton("Motor", [](bool pressed) { motorRunning = pressed; });
    home.button("Count a click", []() { ++clicks; });
    // immediate(): runs even while loop() is busy, as an emergency stop must
    home.button("Stop everything",
                []() {
                    lampOn = false;
                    motorRunning = false;
                })
        .color(pu::Color::Red)
        .immediate();

    home.section("Live values");
    home.value("Clicks", []() { return clicks; }).decimals(0);
    home.value("Running for", []() { return millis() / 1000.0; }).unit("s").decimals(1);
    home.value("Greeting", []() { return appSettings.getString("greeting"); });
    home.indicator("Motor running", []() { return motorRunning; });
    home.indicator("Lamp", []() { return lampOn; }).color(pu::Color::Orange);
}

void setup() {
    Serial.begin(115200);

    config.deviceName = "My ESP32";
    config.firmwareVersion = pu::Semver{0, 0, 1};
    // config.adminPassword = "change-me";          // protect all pages, the API and OTA
#ifdef PU_EXAMPLE_ADMIN_PASSWORD
    config.adminPassword = PU_EXAMPLE_ADMIN_PASSWORD; // set by the on-device tests
#endif
    // config.githubRepo = "your-user/your-repo";   // enable updates from GitHub releases
    ProjectUtils.begin(config);

    appSettings.begin("app", &ProjectUtils.log());
    appSettings.define("bootCount", 0);
    appSettings.define("greeting", "hello");
    appSettings.define("interval", 1.5F);
    appSettings.define("brightness", 50);
    appSettings.define("nightMode", false);
    appSettings.define("speed", "normal");
    appSettings.loadAll();
    appSettings.set("bootCount", static_cast<int>(appSettings.getInt("bootCount")) + 1);

    buildHomePage();
    ProjectUtils.web().addPage("/about", "About", kAboutHtml);

    // Your own routes still work as with any ESPAsyncWebServer project.
    ProjectUtils.server().on("/hello", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Hello from " + ProjectUtils.wifi().deviceName());
    });

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
