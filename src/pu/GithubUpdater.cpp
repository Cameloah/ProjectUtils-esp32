#include "GithubUpdater.h"

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "RamLog.h"

namespace pu {

namespace {
const char kTagMarker[] = "/releases/tag/";
const int kHttpFound = 302;

/// Tags end up in a URL, so only allow characters that cannot change its structure.
bool isSafeTag(const String& tag) {
    if (tag.isEmpty() || tag.length() > 40) {
        return false;
    }
    for (size_t i = 0; i < tag.length(); ++i) {
        const char c = tag[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}
} // namespace

void GithubUpdater::begin(const Config& config, RamLog& log) {
    config_ = &config;
    log_ = &log;
}

bool GithubUpdater::configured() const {
    return config_ != nullptr && !config_->githubRepo.isEmpty();
}

UpdateStatus GithubUpdater::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void GithubUpdater::setState(UpdateState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_.state = state;
    status_.message = "";
}

void GithubUpdater::fail(const String& message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.state = UpdateState::Failed;
        status_.message = message;
    }
    log_->error("Update: %s", message.c_str());
}

String GithubUpdater::assetUrl(const String& tag, const String& assetTemplate) const {
    String asset = assetTemplate;
    asset.replace("<version>", tag);
    return "https://github.com/" + config_->githubRepo + "/releases/download/" + tag + "/" + asset;
}

UpdateState GithubUpdater::check() {
    if (!configured()) {
        fail("no GitHub repository configured");
        return UpdateState::Failed;
    }
    if (WiFi.status() != WL_CONNECTED) {
        fail("WiFi not connected");
        return UpdateState::Failed;
    }
    setState(UpdateState::Checking);

    // GitHub answers .../releases/latest with a redirect to .../releases/tag/<tag>, which gives
    // the latest tag without the rate-limited API.
    String location;
    int httpCode = 0;
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient https;
        https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        if (!https.begin(client,
                         "https://github.com/" + config_->githubRepo + "/releases/latest")) {
            fail("could not open connection to GitHub");
            return UpdateState::Failed;
        }
        httpCode = https.GET();
        location = https.getLocation();
        https.end();
    }
    if (httpCode != kHttpFound) {
        fail("GitHub answered with HTTP status " + String(httpCode));
        return UpdateState::Failed;
    }
    const int markerIndex = location.indexOf(kTagMarker);
    if (markerIndex < 0) {
        fail("repository has no published release");
        return UpdateState::Failed;
    }
    String tag = location.substring(markerIndex + static_cast<int>(strlen(kTagMarker)));
    tag.trim();
    Semver latest = Semver{0, 0, 0};
    if (!isSafeTag(tag) || !Semver::parse(tag.c_str(), latest)) {
        fail("latest release tag '" + tag + "' is not of the form v1.2.3");
        return UpdateState::Failed;
    }

    const UpdateState result =
        latest > config_->firmwareVersion ? UpdateState::Available : UpdateState::UpToDate;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.state = result;
        status_.latestTag = tag;
        status_.message = "";
    }
    char current[24];
    config_->firmwareVersion.format(current, sizeof(current));
    log_->info("Update check: running %s, latest release %s", current, tag.c_str());
    return result;
}

bool GithubUpdater::install(const String& requestedTag) {
    if (!configured()) {
        fail("no GitHub repository configured");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        fail("WiFi not connected");
        return false;
    }
    String tag = requestedTag;
    tag.trim();
    if (tag.isEmpty()) {
        tag = status().latestTag;
    }
    if (!isSafeTag(tag)) {
        fail("invalid version '" + tag + "'");
        return false;
    }
    setState(UpdateState::Installing);

    WiFiClientSecure client;
    client.setInsecure();
    httpUpdate.rebootOnUpdate(false); // both images must be written before restarting
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (!config_->githubFilesystemAsset.isEmpty()) {
        log_->info("Installing filesystem image %s", tag.c_str());
        if (httpUpdate.updateSpiffs(client, assetUrl(tag, config_->githubFilesystemAsset)) !=
            HTTP_UPDATE_OK) {
            // Not fatal: releases may ship without a filesystem image.
            log_->warn("Filesystem image not installed: %s",
                       httpUpdate.getLastErrorString().c_str());
        }
    }

    log_->info("Installing firmware %s", tag.c_str());
    if (httpUpdate.update(client, assetUrl(tag, config_->githubFirmwareAsset)) != HTTP_UPDATE_OK) {
        fail("firmware " + tag + " not installed: " + httpUpdate.getLastErrorString());
        return false;
    }
    setState(UpdateState::Installed);
    log_->info("Firmware %s installed, restart pending", tag.c_str());
    return true;
}

const char* GithubUpdater::stateName(UpdateState state) {
    switch (state) {
    case UpdateState::Idle:
        return "idle";
    case UpdateState::Checking:
        return "checking";
    case UpdateState::UpToDate:
        return "upToDate";
    case UpdateState::Available:
        return "available";
    case UpdateState::Installing:
        return "installing";
    case UpdateState::Installed:
        return "installed";
    case UpdateState::Failed:
        return "failed";
    }
    return "idle";
}

} // namespace pu
