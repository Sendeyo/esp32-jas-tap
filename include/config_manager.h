#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

struct LightConfig {
  String knownDefaultColor;
  String unknownDefaultColor;
  String knownCardAnimation;
  String unknownCardAnimation;
  int lightDuration;
  int numberOfBlinks;
};

struct WifiConfig {
  String ssid;
  String password;
};

struct IotConfig {
  bool enabled;
  String mode;
};

struct DeviceConfig {
  String deviceName;
  int ledBrightness;
  LightConfig light;
  WifiConfig wifi;
  IotConfig iot;
};

bool loadDeviceConfig(DeviceConfig &config);
void printDeviceConfig(const DeviceConfig &config);

#endif
