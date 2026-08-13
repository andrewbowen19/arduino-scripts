#pragma once

#include <Arduino.h>

struct PlantConfig {
  const char *plantName;
  const char *plantId;
  uint8_t analogPin;
  int dryThreshold;
};

const PlantConfig PLANTS[] = {
  {
    "Dracacena",
    "dracacena",
    A0,
    850
  },
  {
    "Parlor Palm",
    "parlor_palm",
    A1,
    850
  },
  {
    "Calathea",
    "calathea",
    A2,
    850
  }
};

const size_t PLANT_COUNT = sizeof(PLANTS) / sizeof(PLANTS[0]);
