#ifndef SERVICE_OS_BACKEND_FREERTOS_H_
#define SERVICE_OS_BACKEND_FREERTOS_H_

#include "FreeRTOS.h"

struct freertos_thread_attr_t {
  const char* pcName;
  uint32_t usStackDepth;
  UBaseType_t uxPriority;
};

#endif