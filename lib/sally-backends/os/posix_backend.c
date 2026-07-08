#include <service/service_os.h>

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define SERVICE_OS_POSIX_CHECK(X)    { if((X) != 0) { return(X); } return(SERVICE_OS_PASS); }

service_os_res_t posix_thread_create(service_os_thread_handle_t* handle, service_os_thread_attr_t attr, service_os_thread_func_t func, service_os_thread_arg_t arg) {
  if(!handle) {
    return(SERVICE_OS_FAIL);
  }

  if(!*handle) {
    *handle = malloc(sizeof(pthread_t));
  }
  int res = pthread_create((pthread_t*)*handle, (pthread_attr_t*)attr, func, arg);
  SERVICE_OS_POSIX_CHECK(res);
}

void posix_thread_delay(unsigned long delay_ms) {
  usleep(delay_ms * 1000UL);
}

service_os_res_t posix_thread_suspend(service_os_thread_handle_t handle) {
  // FIXME implement this!
  (void)handle;
  return(SERVICE_OS_FAIL);
}

service_os_res_t posix_thread_resume(service_os_thread_handle_t handle) {
  // FIXME implement this!
  (void)handle;
  return(SERVICE_OS_FAIL);
}

service_os_res_t posix_thread_delete(service_os_thread_handle_t handle) {
  if(!handle) {
    pthread_exit(NULL);
    // unreachable
    return(SERVICE_OS_FAIL);
  }

  // TODO pass the signal?
  return(pthread_cancel(*(pthread_t*)handle));
}

service_os_res_t posix_thread_wait_for(service_os_thread_handle_t handle) {
  int res = pthread_join(*(pthread_t*)handle, NULL);
  SERVICE_OS_POSIX_CHECK(res);
}

unsigned long posix_thread_get_time_count(void) {
  struct timespec ts = { 0 };
  // FIXME this measured CPU time consumed by the thread, not time since thread creation!
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return((ts.tv_sec * 1000UL) + (ts.tv_nsec / 1000UL));
}

service_os_res_t posix_is_inside_interrupt(void) {
  return(SERVICE_OS_FAIL);
}

void* posix_malloc(size_t size) {
  return(malloc(size));
}

void posix_free(void* buff) {
  free(buff);
}

service_os_mutex_handle_t posix_mutex_create(void) {
  pthread_mutex_t* mux = posix_malloc(sizeof(pthread_mutex_t));
  if(pthread_mutex_init(mux, NULL) != 0) {
    return(NULL);
  }
  return((service_os_mutex_handle_t)mux);
}

service_os_res_t posix_mutex_lock(service_os_mutex_handle_t handle, unsigned long timeout_ms) {
  const struct timespec ts = {
    .tv_sec = timeout_ms / 1000UL,
    .tv_nsec = (timeout_ms % 1000UL)*1000UL*1000UL,
  };
  int res = pthread_mutex_timedlock((pthread_mutex_t*)handle, &ts);
  SERVICE_OS_POSIX_CHECK(res);
}

service_os_res_t posix_mutex_unlock(service_os_mutex_handle_t handle) {
  int res = pthread_mutex_unlock((pthread_mutex_t*)handle);
  SERVICE_OS_POSIX_CHECK(res);
}

service_os_queue_handle_t posix_queue_create(size_t len, size_t size) {
  // FIXME implement queues!
  (void)len;
  (void)size;
  return(NULL);
}

service_os_res_t posix_queue_send(service_os_queue_handle_t handle, void* item, unsigned long timeout_ms) {
  // FIXME implement queues!
  (void)handle;
  (void)item;
  (void)timeout_ms;
  return(SERVICE_OS_FAIL);
}

service_os_res_t posix_queue_receive(service_os_queue_handle_t handle, void* item, unsigned long timeout_ms) {
  // FIXME implement queues!
  (void)handle;
  (void)item;
  (void)timeout_ms;
  return(SERVICE_OS_FAIL);
}

void posix_queue_delete(service_os_queue_handle_t handle) {
  // FIXME implement queues!
  (void)handle;
}

service_os_t backend_os_posix = {
  .thread_create = posix_thread_create,
  .thread_delay = posix_thread_delay,
  .thread_suspend = posix_thread_suspend,
  .thread_resume = posix_thread_resume,
  .thread_delete = posix_thread_delete,
  .thread_wait_for = posix_thread_wait_for,
  .thread_get_time_count = posix_thread_get_time_count,
  .is_inside_interrupt = posix_is_inside_interrupt,
  .malloc = posix_malloc,
  .free = posix_free,
  .mutex_create = posix_mutex_create,
  .mutex_lock = posix_mutex_lock,
  .mutex_unlock = posix_mutex_unlock,
  .queue_create = posix_queue_create,
  .queue_send = posix_queue_send,
  .queue_receive = posix_queue_receive,
  .queue_delete = posix_queue_delete,
};
