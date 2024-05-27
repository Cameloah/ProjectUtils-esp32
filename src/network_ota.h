//
// Created by Cameloah on 05.02.2023.
//

#pragma once

/// initializes the ota update service for platformio
void network_ota_init();

/// update routine that needs to be run periodically
void network_ota_update();