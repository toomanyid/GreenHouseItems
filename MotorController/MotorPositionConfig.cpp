#include "MotorPositionConfig.h"
#include <Preferences.h>

#define NVS_NAMESPACE "motor_cfg"
#define KEY_MIN "min"
#define KEY_BOTTOM "bottom"
#define KEY_TOP "top"
#define KEY_MAX "max"
#define KEY_MAX_CURRENT "maxCur"

bool MotorPositionConfigManager::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only
    return false;
  }
  
  MotorPositionConfig& cfg = getInstance();
  cfg.min_x10 = prefs.getInt(KEY_MIN, 0);
  cfg.bottom_x10 = prefs.getInt(KEY_BOTTOM, 50);
  cfg.top_x10 = prefs.getInt(KEY_TOP, 100);
  cfg.max_x10 = prefs.getInt(KEY_MAX, 150);
  cfg.maxCurrent_x10 = prefs.getInt(KEY_MAX_CURRENT, 20);
  
  prefs.end();
  return true;
}

bool MotorPositionConfigManager::save() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write
    return false;
  }
  
  MotorPositionConfig& cfg = getInstance();
  prefs.putInt(KEY_MIN, cfg.min_x10);
  prefs.putInt(KEY_BOTTOM, cfg.bottom_x10);
  prefs.putInt(KEY_TOP, cfg.top_x10);
  prefs.putInt(KEY_MAX, cfg.max_x10);
  prefs.putInt(KEY_MAX_CURRENT, cfg.maxCurrent_x10);
  
  prefs.end();
  return true;
}
