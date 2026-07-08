#include <service/service_os.h>

#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "cmsis_os.h"

#define SERVICE_OS_CMSIS_CHECK(X)    { if((X) != pdPASS) { return(SERVICE_OS_FAIL); } return(SERVICE_OS_PASS); }

service_os_res_t cmsis_thread_create(service_os_thread_handle_t* handle, service_os_thread_attr_t attr, service_os_thread_func_t func, service_os_thread_arg_t arg) {
  if(!handle) {
    return(SERVICE_OS_FAIL);
  }
  
  *handle = osThreadNew((osThreadFunc_t)func, arg, (osThreadAttr_t*)attr);
  if(*handle == NULL) {
    return(SERVICE_OS_FAIL);
  }
  return(SERVICE_OS_PASS);
}

void cmsis_thread_delay(unsigned long delay_ms) {
  vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

void cmsis_thread_delay_until(unsigned long* prev_ms, unsigned long delay_ms) {
  vTaskDelayUntil(prev_ms, pdMS_TO_TICKS(delay_ms));
}

unsigned long cmsis_thread_get_time_count(void) {
  return(xTaskGetTickCount() / (configTICK_RATE_HZ/1000));
}

service_os_res_t cmsis_thread_suspend(service_os_thread_handle_t handle) {
  return(osThreadSuspend(handle));
}

service_os_res_t cmsis_thread_resume(service_os_thread_handle_t handle) {
  return(osThreadResume(handle));
}

service_os_res_t cmsis_thread_delete(service_os_thread_handle_t handle) {
  if(!handle) {
    osThreadExit();
    // unreachable
    return(SERVICE_OS_FAIL);
  }
  return(osThreadTerminate(handle));
}

service_os_res_t cmsis_thread_wait_for(service_os_thread_handle_t handle) {
  // TODO implement this
  (void)handle;
  return(SERVICE_OS_FAIL);
}

service_os_thread_handle_t cmsis_thread_find(char* name) {
  uint32_t th_count = osThreadGetCount();

  // TODO replace this with static array
  osThreadId_t* thread_array = pvPortMalloc(th_count*sizeof(osThreadId_t));
  if(!thread_array) {
    return(NULL);
  }

  // enumerate the threads
  uint32_t num_threads = osThreadEnumerate(thread_array, th_count);
  if(num_threads != th_count) {
    vPortFree(thread_array);
    return(NULL);
  }

  // find the task
  for(uint32_t i = 0; i < num_threads; i++) {
    // TODO configurable max len?
    if(strncmp(osThreadGetName(thread_array[i]), name, 32) == 0) {
      service_os_thread_handle_t handle = thread_array[i];
      vPortFree(thread_array);
      return(handle);
    }
  }

  vPortFree(thread_array);
  return(NULL);
}

service_os_res_t cmsis_is_inside_interrupt(void) {
  BaseType_t res = xPortIsInsideInterrupt();
  SERVICE_OS_CMSIS_CHECK(res);
}

void* cmsis_malloc(size_t size) {
  return(pvPortMalloc(size));
}

void cmsis_free(void* buff) {
  vPortFree(buff);
}

service_os_mutex_handle_t cmsis_mutex_create(void) {
  return((service_os_mutex_handle_t)xSemaphoreCreateMutex());
}

service_os_res_t cmsis_mutex_lock(service_os_mutex_handle_t handle, unsigned long timeout_ms) {
  TickType_t delay = pdMS_TO_TICKS(timeout_ms);
  if(timeout_ms == SERVICE_OS_MAX_DELAY) {
    delay = portMAX_DELAY;
  }
  BaseType_t res = xSemaphoreTake((SemaphoreHandle_t)handle, delay);
  SERVICE_OS_CMSIS_CHECK(res);
}

service_os_res_t cmsis_mutex_unlock(service_os_mutex_handle_t handle) {
  BaseType_t res = xSemaphoreGive((SemaphoreHandle_t)handle);
  SERVICE_OS_CMSIS_CHECK(res);
}

service_os_queue_handle_t cmsis_queue_create(size_t len, size_t size) {
  return((service_os_queue_handle_t)xQueueCreate(len, size));
}

service_os_res_t cmsis_queue_send(service_os_queue_handle_t handle, void* item, unsigned long timeout_ms) {
  TickType_t delay = pdMS_TO_TICKS(timeout_ms);
  if(timeout_ms == SERVICE_OS_MAX_DELAY) {
    delay = portMAX_DELAY;
  }
  BaseType_t res = xQueueSend(handle, item, delay);
  SERVICE_OS_CMSIS_CHECK(res);
}

service_os_res_t cmsis_queue_receive(service_os_queue_handle_t handle, void* item, unsigned long timeout_ms) {
  TickType_t delay = pdMS_TO_TICKS(timeout_ms);
  if(timeout_ms == SERVICE_OS_MAX_DELAY) {
    delay = portMAX_DELAY;
  }
  BaseType_t res = xQueueReceive(handle, item, delay);
  SERVICE_OS_CMSIS_CHECK(res);
}

void cmsis_queue_delete(service_os_queue_handle_t handle) {
  vQueueDelete(handle);
}

service_os_t backend_os_cmsis = {
  .thread_create = cmsis_thread_create,
  .thread_delay = cmsis_thread_delay,
  .thread_delay_until = cmsis_thread_delay_until,
  .thread_get_time_count = cmsis_thread_get_time_count,
  .thread_suspend = cmsis_thread_suspend,
  .thread_resume = cmsis_thread_resume,
  .thread_delete = cmsis_thread_delete,
  .thread_wait_for = cmsis_thread_wait_for,
  .thread_find = cmsis_thread_find,
  .is_inside_interrupt = cmsis_is_inside_interrupt,
  .malloc = cmsis_malloc,
  .free = cmsis_free,
  .mutex_create = cmsis_mutex_create,
  .mutex_lock = cmsis_mutex_lock,
  .mutex_unlock = cmsis_mutex_unlock,
  .queue_create = cmsis_queue_create,
  .queue_send = cmsis_queue_send,
  .queue_receive = cmsis_queue_receive,
  .queue_delete = cmsis_queue_delete,
};
