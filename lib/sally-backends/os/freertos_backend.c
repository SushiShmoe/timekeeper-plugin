#include "freertos_backend.h"

#include <service/service_os.h>

#include "semphr.h"
#include "task.h"
#include "FreeRTOSConfig.h"

#define SERVICE_OS_FREERTOS_CHECK(X)    { if((X) != pdPASS) { return(SERVICE_OS_FAIL); } return(SERVICE_OS_PASS); }

service_os_res_t freertos_thread_create(service_os_thread_handle_t* handle, service_os_thread_attr_t attr, service_os_thread_func_t func, service_os_thread_arg_t arg) {
  if(!handle) {
    return(SERVICE_OS_FAIL);
  }
  
  struct freertos_thread_attr_t* freertos_attr = (struct freertos_thread_attr_t*)attr;
  xTaskCreate((TaskFunction_t)func, freertos_attr->pcName, freertos_attr->usStackDepth, arg, freertos_attr->uxPriority, handle);
  if(*handle == NULL) {
    return(SERVICE_OS_FAIL);
  }
  return(SERVICE_OS_PASS);
}

void freertos_thread_delay(unsigned long delay_ms) {
  vTaskDelay(delay_ms / configTICK_RATE_HZ);
}

void freertos_thread_delay_until(unsigned long* prev_ms, unsigned long delay_ms) {
  vTaskDelayUntil(prev_ms, delay_ms / configTICK_RATE_HZ);
}

unsigned long freertos_thread_get_time_count(void) {
  return(xTaskGetTickCount() / (configTICK_RATE_HZ/1000));
}

service_os_res_t freertos_thread_suspend(service_os_thread_handle_t handle) {
  vTaskSuspend(handle);
  // TODO error checking
  return(SERVICE_OS_PASS);
}

service_os_res_t freertos_thread_resume(service_os_thread_handle_t handle) {
  vTaskResume(handle);
  // TODO error checking
  return(SERVICE_OS_PASS);
}

service_os_res_t freertos_thread_delete(service_os_thread_handle_t handle) {
  vTaskDelete(handle);
  // TODO error checking
  return(SERVICE_OS_PASS);
}

service_os_res_t freertos_thread_wait_for(service_os_thread_handle_t handle) {
  // TODO implement this
  (void)handle;
  return(SERVICE_OS_FAIL);
}

service_os_res_t freertos_is_inside_interrupt(void) {
  // TODO implement this!!!!
  //BaseType_t res = xPortIsInsideInterrupt();
  BaseType_t res = pdFAIL;
  SERVICE_OS_FREERTOS_CHECK(res);
}

void* freertos_malloc(size_t size) {
  return(pvPortMalloc(size));
}

void freertos_free(void* buff) {
  vPortFree(buff);
}

service_os_mutex_handle_t freertos_mutex_create(void) {
  return((service_os_mutex_handle_t)xSemaphoreCreateMutex());
}

service_os_res_t freertos_mutex_lock(service_os_mutex_handle_t handle, unsigned long timeout_ms) {
  TickType_t delay = timeout_ms / configTICK_RATE_HZ;
  if(timeout_ms == SERVICE_OS_MAX_DELAY) {
    delay = portMAX_DELAY;
  }
  BaseType_t res = xSemaphoreTake((SemaphoreHandle_t)handle, delay);
  SERVICE_OS_FREERTOS_CHECK(res);
}

service_os_res_t freertos_mutex_unlock(service_os_mutex_handle_t handle) {
  BaseType_t res = xSemaphoreGive((SemaphoreHandle_t)handle);
  SERVICE_OS_FREERTOS_CHECK(res);
}

service_os_queue_handle_t freertos_queue_create(size_t len, size_t size) {
  return((service_os_queue_handle_t)xQueueCreate(len, size));
}

service_os_res_t freertos_queue_send(service_os_queue_handle_t handle, void* item, unsigned long timeout_ms) {
  TickType_t delay = timeout_ms / configTICK_RATE_HZ;
  if(timeout_ms == SERVICE_OS_MAX_DELAY) {
    delay = portMAX_DELAY;
  }
  BaseType_t res = xQueueSend(handle, item, delay);
  SERVICE_OS_FREERTOS_CHECK(res);
}

service_os_res_t freertos_queue_receive(service_os_queue_handle_t handle, void* item, unsigned long timeout_ms) {
  TickType_t delay = timeout_ms / configTICK_RATE_HZ;
  if(timeout_ms == SERVICE_OS_MAX_DELAY) {
    delay = portMAX_DELAY;
  }
  BaseType_t res = xQueueReceive(handle, item, delay);
  SERVICE_OS_FREERTOS_CHECK(res);
}

void freertos_queue_delete(service_os_queue_handle_t handle) {
  vQueueDelete(handle);
}

service_os_t backend_os_freertos = {
  .thread_create = freertos_thread_create,
  .thread_delay = freertos_thread_delay,
  .thread_delay_until = freertos_thread_delay_until,
  .thread_get_time_count = freertos_thread_get_time_count,
  .thread_suspend = freertos_thread_suspend,
  .thread_resume = freertos_thread_resume,
  .thread_delete = freertos_thread_delete,
  .thread_wait_for = freertos_thread_wait_for,
  .is_inside_interrupt = freertos_is_inside_interrupt,
  .malloc = freertos_malloc,
  .free = freertos_free,
  .mutex_create = freertos_mutex_create,
  .mutex_lock = freertos_mutex_lock,
  .mutex_unlock = freertos_mutex_unlock,
  .queue_create = freertos_queue_create,
  .queue_send = freertos_queue_send,
  .queue_receive = freertos_queue_receive,
  .queue_delete = freertos_queue_delete,
};
