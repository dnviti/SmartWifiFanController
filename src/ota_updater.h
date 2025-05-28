#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include "config.h"

// Call this function to check for updates and start the process if one is available.
// It will update the global ota_status_message and ota_in_progress flags.
void triggerOTAUpdateCheck();

// Function to be called in a task to perform the actual update process
// This is separated to allow UI updates before this blocking operation starts.
void performOTAUpdateProcess(const String& latestVersionTag, const String& firmwareURL, const String& spiffsURL);

// Helper to compare versions. Returns true if latestVersionStr is newer than currentVersionStr.
// Basic string comparison, for "vX.Y.Z" formats.
bool isVersionNewer(const String& currentVersionStr, const String& latestVersionStr);

// Struct to hold release info
struct GithubReleaseInfo {
    String tagName;
    String firmwareAssetURL;
    String spiffsAssetURL;
    bool isValid;
};

// Fetches latest release info from GitHub
GithubReleaseInfo getLatestGithubReleaseInfo();


#endif // OTA_UPDATER_H
