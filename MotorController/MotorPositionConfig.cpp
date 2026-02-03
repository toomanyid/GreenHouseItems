#include "MotorPositionConfig.h"
#include <Preferences.h>

#define NVS_NAMESPACE "motor_cfg"
#define KEY_BOTTOM "bottom"
#define KEY_READY "ready"
#define KEY_TOP "top"
#define KEY_MAX "max"
#define KEY_MAX_CURRENT "maxCur"

bool MotorPositionConfigManager::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only
    return false;
  }
  
  MotorPositionConfig& cfg = getInstance();
  cfg.bottom = prefs.getFloat(KEY_BOTTOM, 0.0f);
  cfg.ready = prefs.getFloat(KEY_READY, 5.0f);
  cfg.top = prefs.getFloat(KEY_TOP, 10.0f);
  cfg.max = prefs.getFloat(KEY_MAX, 15.0f);
  cfg.maxCurrent = prefs.getFloat(KEY_MAX_CURRENT, 2.0f);
  
  prefs.end();
  return true;
}

bool MotorPositionConfigManager::save() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write
    return false;
  }
  
  MotorPositionConfig& cfg = getInstance();
  prefs.putFloat(KEY_BOTTOM, cfg.bottom);
  prefs.putFloat(KEY_READY, cfg.ready);
  prefs.putFloat(KEY_TOP, cfg.top);
  prefs.putFloat(KEY_MAX, cfg.max);
  prefs.putFloat(KEY_MAX_CURRENT, cfg.maxCurrent);
  
  prefs.end();
  return true;
}
