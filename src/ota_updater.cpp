#include "ota_updater.h"
#include "config.h"        // For FIRMWARE_VERSION, GITHUB defines, ota_status_message etc.
#include "display_handler.h" // For displayMenu()
#include <WiFiClientSecure.h> 
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
// SPIFFS is already included via config.h if needed here, but direct file ops are in main.cpp

bool isVersionNewer(const String& currentVersionStr, const String& latestVersionStr) {
    String current = currentVersionStr;
    String latest = latestVersionStr;
    current.replace("v", "");
    latest.replace("v", "");
    return latest.compareTo(current) > 0;
}

GithubReleaseInfo getLatestGithubReleaseInfo() {
    GithubReleaseInfo info;
    info.isValid = false;
    
    ota_status_message = "Fetching release info...";
    needsImmediateBroadcast = true;
    if(isInMenuMode) displayMenu();


    if (WiFi.status() != WL_CONNECTED) {
        ota_status_message = "Error: WiFi not connected.";
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
        if(isInMenuMode) displayMenu();
        return info;
    }

    HTTPClient http;
    WiFiClientSecure clientSecure; 

    // Use the GITHUB_API_ROOT_CA_STRING loaded from SPIFFS
    if (GITHUB_API_ROOT_CA_STRING.isEmpty()) { 
        ota_status_message = "Error: No Root CA loaded for secure OTA.";
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA_ERR] " + ota_status_message + " Check " + GITHUB_ROOT_CA_FILENAME + " on SPIFFS.");
        if(isInMenuMode) displayMenu();
        return info;
    }
    
    clientSecure.setCACert(GITHUB_API_ROOT_CA_STRING.c_str()); // Use the loaded CA string
    if(serialDebugEnabled) Serial.println("[OTA] Attempting secure connection to GitHub API using CA from SPIFFS...");

    if (http.begin(clientSecure, GITHUB_API_LATEST_RELEASE_URL)) {
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            if(serialDebugEnabled) Serial.println("[OTA] GitHub API Response: " + payload.substring(0, min(500, (int)payload.length())) + "..."); 

            JsonDocument doc; 
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                ota_status_message = "Error: Parse API JSON failed.";
                if(serialDebugEnabled) Serial.printf("[OTA] deserializeJson() failed: %s\n", error.c_str());
            } else {
                info.tagName = doc["tag_name"].as<String>();
                if (info.tagName.isEmpty()) {
                    ota_status_message = "Error: No tag_name in release.";
                     if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
                } else {
                    JsonArray assets = doc["assets"].as<JsonArray>();
                    String expectedFirmwareName = "firmware_" PIO_BUILD_ENV_NAME "_" + info.tagName + ".bin";
                    String expectedSpiffsName = "spiffs_" PIO_BUILD_ENV_NAME "_" + info.tagName + ".bin";

                    for (JsonObject asset : assets) {
                        String assetName = asset["name"].as<String>();
                        if (assetName == expectedFirmwareName) {
                            info.firmwareAssetURL = asset["browser_download_url"].as<String>();
                        } else if (assetName == expectedSpiffsName) {
                            info.spiffsAssetURL = asset["browser_download_url"].as<String>();
                        }
                    }

                    if (!info.firmwareAssetURL.isEmpty()) { 
                        info.isValid = true;
                        ota_status_message = "Latest release: " + info.tagName;
                        if(serialDebugEnabled) Serial.println("[OTA] Found: " + info.tagName + ", FW: " + info.firmwareAssetURL + ", FS: " + info.spiffsAssetURL);
                    } else {
                        ota_status_message = "Error: FW asset missing for " + info.tagName;
                        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
                        if(serialDebugEnabled) Serial.println("[OTA] Expected FW: " + expectedFirmwareName);
                    }
                }
            }
        } else { 
            ota_status_message = "Error: API request failed. Code: " + String(httpCode);
            if(serialDebugEnabled) Serial.printf("[OTA_ERR] Secure GitHub API request failed. HTTP Code: %d, Error: %s\n", httpCode, http.errorToString(httpCode).c_str());
        }
        http.end();
    } else { 
        ota_status_message = "Error: Connect to GitHub API failed.";
        if(serialDebugEnabled) Serial.println("[OTA_ERR] http.begin (secure) failed for GitHub API.");
    }
    
    needsImmediateBroadcast = true;
    if(isInMenuMode) displayMenu();
    return info;
}


void performOTAUpdateProcess(const String& latestVersionTag, const String& firmwareURL, const String& spiffsURL) {
    ota_in_progress = true;
    ota_status_message = "Starting update to " + latestVersionTag + "...";
    needsImmediateBroadcast = true;
    if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
    if(isInMenuMode) displayMenu(); 

    vTaskDelay(pdMS_TO_TICKS(200)); 

    t_httpUpdate_return ret;

    // Use the GITHUB_API_ROOT_CA_STRING loaded from SPIFFS
    if (GITHUB_API_ROOT_CA_STRING.isEmpty()) { 
        ota_status_message = "Error: No Root CA loaded for secure OTA.";
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA_ERR] " + ota_status_message + " Firmware/SPIFFS update aborted.");
        if(isInMenuMode) displayMenu();
        ota_in_progress = false;
        return;
    }

    // --- Firmware Update ---
    if (!firmwareURL.isEmpty()) {
        ota_status_message = "Updating FW: " + latestVersionTag;
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
        if(isInMenuMode) displayMenu();
        vTaskDelay(pdMS_TO_TICKS(100));

        WiFiClientSecure fwClient; 
        fwClient.setCACert(GITHUB_API_ROOT_CA_STRING.c_str()); // Use loaded CA string
        if(serialDebugEnabled) Serial.println("[OTA_FW] Attempting secure FW update...");
        
        httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); 
        ret = httpUpdate.update(fwClient, firmwareURL);
        
        switch (ret) {
            case HTTP_UPDATE_FAILED:
                ota_status_message = "FW update failed: " + httpUpdate.getLastErrorString();
                if(serialDebugEnabled) Serial.printf("[OTA_FW_ERR] Secure Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES: 
                ota_status_message = "FW: No updates (server).";
                if(serialDebugEnabled) Serial.println("[OTA_FW] No firmware updates.");
                break;
            case HTTP_UPDATE_OK:
                ota_status_message = "FW update OK! Rebooting...";
                if(serialDebugEnabled) Serial.println("[OTA_FW] Firmware update OK.");
                needsImmediateBroadcast = true;
                if(isInMenuMode) displayMenu();
                delay(1000); 
                ESP.restart(); 
                return; 
        }
    } else {
        ota_status_message = "FW URL missing. Skip FW.";
        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
    }
    needsImmediateBroadcast = true;
    if(isInMenuMode) displayMenu();

    // --- SPIFFS Update ---
    if (!spiffsURL.isEmpty()) {
        ota_status_message = "Updating FS: " + latestVersionTag;
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
        if(isInMenuMode) displayMenu();
        vTaskDelay(pdMS_TO_TICKS(100));

        WiFiClientSecure fsClient; 
        fsClient.setCACert(GITHUB_API_ROOT_CA_STRING.c_str()); // Use loaded CA string
        if(serialDebugEnabled) Serial.println("[OTA_FS] Attempting secure FS update...");

        httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        ret = httpUpdate.updateSpiffs(fsClient, spiffsURL);

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                ota_status_message = "FS update failed: " + httpUpdate.getLastErrorString();
                if(serialDebugEnabled) Serial.printf("[OTA_FS_ERR] Secure Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                ota_status_message = "FS: No updates (server).";
                if(serialDebugEnabled) Serial.println("[OTA_FS] No SPIFFS updates.");
                break;
            case HTTP_UPDATE_OK:
                ota_status_message = "FS update OK! Rebooting...";
                if(serialDebugEnabled) Serial.println("[OTA_FS] SPIFFS update OK.");
                needsImmediateBroadcast = true;
                if(isInMenuMode) displayMenu();
                delay(1000);
                ESP.restart();
                return; 
        }
    } else {
        ota_status_message = "FS URL missing. Skip FS.";
         if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
    }

    ota_in_progress = false; 
    needsImmediateBroadcast = true;
    if(isInMenuMode) displayMenu();
}


void triggerOTAUpdateCheck() {
    if (ota_in_progress) {
        ota_status_message = "OTA update already in progress.";
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
        if(isInMenuMode) displayMenu();
        return;
    }

    ota_in_progress = true; 
    ota_status_message = "Checking for updates...";
    needsImmediateBroadcast = true;
    if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
    if(isInMenuMode) displayMenu(); 

    GithubReleaseInfo releaseInfo = getLatestGithubReleaseInfo();

    if (!releaseInfo.isValid) {
        ota_in_progress = false; 
        needsImmediateBroadcast = true;
        if(isInMenuMode) displayMenu();
        return;
    }

    if (isVersionNewer(FIRMWARE_VERSION, releaseInfo.tagName)) {
        ota_status_message = "New: " + releaseInfo.tagName + " Curr: " FIRMWARE_VERSION;
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
        if(isInMenuMode) displayMenu();
        
        ota_status_message = "Preparing to update...";
        needsImmediateBroadcast = true;
        if(isInMenuMode) displayMenu();
        vTaskDelay(pdMS_TO_TICKS(2000)); 

        performOTAUpdateProcess(releaseInfo.tagName, releaseInfo.firmwareAssetURL, releaseInfo.spiffsAssetURL);
    } else {
        ota_status_message = "Firmware is up to date (" FIRMWARE_VERSION ").";
        needsImmediateBroadcast = true;
        if(serialDebugEnabled) Serial.println("[OTA] " + ota_status_message);
        ota_in_progress = false; 
    }
    if(isInMenuMode) displayMenu(); 
}
