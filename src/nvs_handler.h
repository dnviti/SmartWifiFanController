#ifndef NVS_HANDLER_H
#define NVS_HANDLER_H

#include "config.h"

// RAII-style wrapper for NVS Preferences to automate begin() and end() calls.
// Ensures preferences.end() is called when the object goes out of scope.
class NVSNamespace {
public:
  /**
   * @brief Constructs an NVSNamespace object and attempts to open the specified namespace.
   * @param ns The name of the NVS namespace.
   * @param readonly True to open in read-only mode, false for read/write.
   */
  NVSNamespace(const char* ns, bool readonly) {
    ok = preferences.begin(ns, readonly);
    if (!ok && serialDebugEnabled) {
      Serial.printf("[NVS_ERR] Failed to open namespace '%s' for %s.\n", ns, readonly ? "reading" : "writing");
    }
  }

  /**
   * @brief Destroys the NVSNamespace object, automatically calling preferences.end()
   *        if the namespace was successfully opened.
   */
  ~NVSNamespace() {
    if (ok) {
      preferences.end();
    }
  }

  /**
   * @brief Checks if the NVS namespace was successfully opened.
   * @return True if the namespace is open, false otherwise.
   */
  bool isOpen() const { return ok; }

private:
  bool ok; // Stores the success status of preferences.begin()
};

void saveWiFiConfig();
void loadWiFiConfig();
void saveFanCurveToNVS();
void loadFanCurveFromNVS();

// MQTT NVS Functions
void saveMqttConfig();
void loadMqttConfig();

// MQTT Discovery NVS Functions - ADDED
void saveMqttDiscoveryConfig();
void loadMqttDiscoveryConfig();

#endif // NVS_HANDLER_H
