#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float pressure;
    uint64_t timestamp;
} PressureData;

typedef struct {
    float temperature;
    uint64_t timestamp;
} TemperatureData;

typedef struct {
    PressureData pressure_data;
    TemperatureData temperature_data;
    TemperatureData internal_temp_data;
} SensorData_t;


#define DATA_BUFFER_SIZE 500
