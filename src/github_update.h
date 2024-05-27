//
// Created by Camleoah on 08/12/2022.
//

#pragma once

#include "webserial_monitor.h"
#include "../../../include/version.h"

#define URL_FW_VERSION "https://github.com/" GITHUB_REPO "/releases/latest"
#define URL_FW_BIN "https://github.com/" GITHUB_REPO "/releases/download/<version>/" GITHUB_FW_BIN
#define URL_FS_BIN "https://github.com/" GITHUB_REPO "/releases/download/<version>/" GITHUB_FS_BIN

typedef enum{
    GITHUB_UPDATE_ERROR_NO_ERROR        = 0x00,
    GITHUB_UPDATE_ERROR_WIFI            = 0x01,
    GITHUB_UPDATE_ERROR_HTTP            = 0x02,
    GITHUB_UPDATE_ERROR_NO_UPDATE       = 0x03,
    GITHUB_UPDATE_ERROR_URL             = 0x04,
    GITHUB_UPDATE_ERROR_UNKNOWN         = 0xFF
} GITHUB_UPDATE_ERROR_t;

/// Tries to download the bin file matching the version saved in private buffer
/// \return GITHUB_UPDATE_ERROR_t error code
GITHUB_UPDATE_ERROR_t github_update_firmwareUpdate();

/// Tries to download bin file for a specific firmware version
/// \param desired_version version string in v0.0.0 format
/// \return GITHUB_UPDATE_ERROR_t error code
GITHUB_UPDATE_ERROR_t github_update_firmwareUpdate(const char *desired_version);

/// Checks for the latest release version on a github repository and compares to current version
/// \return GITHUB_UPDATE_ERROR_t status if new version is available
GITHUB_UPDATE_ERROR_t github_update_checkforlatest(String *ret_version = NULL);

/// Initializes the github update module
void github_update_init();
