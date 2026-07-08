#include <service/service_com.h>
#include <service/service_err_codes.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// hopefully this never needs to be changed
#define RAMBUFF_SIZE          (256)
#define RAMBUFF_NUM_PORTS     (16)

// struct to keep information about the emulated port
struct memcom_com_buff_t {
  uint32_t write_pos;
  uint32_t read_pos;
  size_t size;
  uint8_t buff[RAMBUFF_SIZE];
};

// each port has two buffers, Rx and Tx (duh)
struct memcom_com_t {
  struct memcom_com_buff_t buffs[2];
};

// some emulated "COM ports"
// neighboring buffers (N and N+1) are connected - one writes to the other
struct memcom_com_t memcom_port_pool[RAMBUFF_NUM_PORTS];

// helper function to check if the circular buffer is full
static bool memcom_buff_is_full(struct memcom_com_buff_t* buff) {
  if((!buff) || (buff->size == 0)) {
    return(true);
  }
  return(((buff->write_pos + 1) % buff->size) == buff->read_pos);
}

// helper function to check if the circular buffer is empty
static bool memcom_buff_is_empty(struct memcom_com_buff_t* buff) {
  if((!buff) || (buff->size == 0)) {
    return(true);
  }
  return(buff->write_pos == buff->read_pos);
}

int memcom_open(service_com_handle_t handle, service_com_device_t dev, service_com_conf_t* conf) {
  (void)conf;
  if(!handle) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  // get the port and check it
  int port = *(int*)dev;
  if((port < 0) || (port > RAMBUFF_NUM_PORTS)) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  // get a port from the pool and reset it
  int port_num = port/2;
  int buff_num = port%2;
  struct memcom_com_t* memcom = &memcom_port_pool[port_num];
  memcom->buffs[buff_num].write_pos = 0;
  memcom->buffs[buff_num].read_pos = 0;
  memcom->buffs[buff_num].size = RAMBUFF_SIZE;
  memset(memcom->buffs[buff_num].buff, 0x00, memcom->buffs[buff_num].size);

  *(int*)handle = port;
  return(SERVICE_ERR_NONE);
}

int memcom_close(service_com_handle_t handle) {
  if(!handle) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  // nothing to do here
  return(SERVICE_ERR_NONE);
}

size_t memcom_write(service_com_handle_t handle, const void* buff, size_t count) {
  if(!handle) {
    return(0);
  }
  
  int port = *(int*)handle;
  if((port < 0) || (port > RAMBUFF_NUM_PORTS)) {
    return(0);
  }

  int port_num = port/2;
  // when writing use the other buffer
  // this is what actually emulates the physical connection
  int buff_num = 1 - port%2;
  struct memcom_com_t* memcom = &memcom_port_pool[port_num];
  if(memcom_buff_is_full(&memcom->buffs[buff_num])) {
    return(0);
  }

  size_t wr = 0;
  char* buff_ptr = (char*)buff;
  for(; wr < count; wr++) {
    memcom->buffs[buff_num].buff[memcom->buffs[buff_num].write_pos] = buff_ptr[wr];
    memcom->buffs[buff_num].write_pos = (memcom->buffs[buff_num].write_pos + 1) % memcom->buffs[buff_num].size;
  }

  return(wr);
}

size_t memcom_read(service_com_handle_t handle, void* buff, size_t count) {
  if(!handle || !buff) {
    return(0);
  }
  
  int port = *(int*)handle;
  if((port < 0) || (port > RAMBUFF_NUM_PORTS)) {
    return(0);
  }

  int port_num = port/2;
  int buff_num = port%2;
  struct memcom_com_t* memcom = &memcom_port_pool[port_num];
  if(memcom_buff_is_empty(&memcom->buffs[buff_num])) {
    return(0);
  }

  size_t rd = 0;
  char* buff_ptr = (char*)buff;
  for(; rd < count; rd++) {
    buff_ptr[rd] = memcom->buffs[buff_num].buff[memcom->buffs[buff_num].read_pos];
    memcom->buffs[buff_num].read_pos = (memcom->buffs[buff_num].read_pos + 1) % memcom->buffs[buff_num].size;
  }

  return(rd);
}

size_t memcom_available(service_com_handle_t handle) {
  if(!handle) {
    return(0);
  }

  int port = *(int*)handle;
  if((port < 0) || (port > RAMBUFF_NUM_PORTS)) {
    return(0);
  }

  int port_num = port/2;
  int buff_num = port%2;
  struct memcom_com_t* memcom = &memcom_port_pool[port_num];
  return((memcom->buffs[buff_num].write_pos + memcom->buffs[buff_num].size - memcom->buffs[buff_num].read_pos) % memcom->buffs[buff_num].size);
}

service_com_handle_t memcom_handle_malloc(void) {
  return((service_com_handle_t*)malloc(sizeof(int)));
}

void memcom_handle_free(service_com_handle_t handle) {
  if(handle) {
    free(handle);
  }
}

service_com_t backend_memcom = {
  .open = memcom_open,
  .close = memcom_close,
  .write = memcom_write,
  .read = memcom_read,
  .available = memcom_available,
  .com_handle_malloc = memcom_handle_malloc,
  .com_handle_free = memcom_handle_free,
};
