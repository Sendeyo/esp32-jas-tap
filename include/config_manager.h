#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

// Light settings
struct LightConfig {
  String knownDefaultColor;
  String unknownDefaultColor;
  String knownCardAnimation;
  String unknownCardAnimation;
  int lightDuration;
  int numberOfBlinks;
};

// Sound settings
struct SoundConfig {
  bool tapDetection;
  int volume;
  bool onStatus;
  int duration;
};

// WiFi settings
struct WifiConfig {
  String ssid;
  String password;
};

// Server settings
struct ServerConfig {
  String address;
  int port;
};

// MQTT settings
struct MqttConfig {
  bool enable;
  String host;
  int port;
  String topic;
  String user;
  String pass;
};

// IOT settings
struct IotConfig {
  bool enabled;
};

// Full device config
struct DeviceConfig {
  String deviceName;
  int ledBrightness;
  int mode;
  String hotspotPassword;
  LightConfig light;
  SoundConfig sound;
  WifiConfig wifi;
  ServerConfig server;
  MqttConfig mqtt;
  IotConfig iot;
};

// Function declarations
bool loadDeviceConfig(DeviceConfig &config);
void printDeviceConfig(const DeviceConfig &config);

#endif
