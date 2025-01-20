//////////////////////////////////////////////////
// Mutex support for ESP8266.
// Copyright 2015 Richard A Burton
// richardaburton@gmail.com
// See license.txt for license terms.
//////////////////////////////////////////////////

#pragma once

#include <Arduino.h>

typedef int32_t mutex_t;

void ICACHE_FLASH_ATTR CreateMutux(mutex_t *mutex);
bool ICACHE_FLASH_ATTR GetMutex(mutex_t *mutex);
void ICACHE_FLASH_ATTR ReleaseMutex(mutex_t *mutex);
