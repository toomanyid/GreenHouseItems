#include "MotorPositionConfig.h"
#include <Preferences.h>

#define NVS_NAMESPACE "motor_cfg"
#define KEY_MIN "min"
#define KEY_ZERO "zero"
#define KEY_LIMIT "limit"
#define KEY_MAX "max"
#define KEY_MAX_CURRENT "maxCur"

bool MotorPositionConfigManager::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only
    return false;
  }
  
  MotorPositionConfig& cfg = getInstance();
  cfg.min = prefs.getFloat(KEY_MIN, 0.0f);
  cfg.zero = prefs.getFloat(KEY_ZERO, 5.0f);
  cfg.limit = prefs.getFloat(KEY_LIMIT, 10.0f);
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
  prefs.putFloat(KEY_MIN, cfg.min);
  prefs.putFloat(KEY_ZERO, cfg.zero);
  prefs.putFloat(KEY_LIMIT, cfg.limit);
  prefs.putFloat(KEY_MAX, cfg.max);
  prefs.putFloat(KEY_MAX_CURRENT, cfg.maxCurrent);
  
  prefs.end();
  return true;
}
