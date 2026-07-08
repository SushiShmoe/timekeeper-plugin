#include <service/service_com.h>
#include <service/service_err_codes.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>

int tty_open(service_com_handle_t handle, service_com_device_t dev, service_com_conf_t* conf) {
  int fd = open((char*)dev, O_RDWR | O_NOCTTY | O_SYNC);
  if(fd < 0) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  // TODO how to make this configurable and which parts?
  struct termios options;

  options.c_cflag |= (CLOCAL | CREAD);
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CRTSCTS;
  options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  options.c_oflag &= ~OPOST;

  int should_block = 0;
  options.c_cc[VMIN] = should_block ? 1 : 0;
  options.c_cc[VTIME] = 5;  // 0.5 seconds read timeout

  // FIXME this should be smarter ...
  switch(conf->speed) {
    case(460800):
      cfsetispeed(&options, B460800);
      cfsetospeed(&options, B460800);
      break;
    default:
      cfsetispeed(&options, B115200);
      cfsetospeed(&options, B115200);
      break;
  }

  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &options);

  *(int*)handle = fd;
  return(SERVICE_ERR_NONE);
}

int tty_close(service_com_handle_t handle) {
  int fd = *(int*)handle;
  return(close(fd));
}

size_t tty_write(service_com_handle_t handle, const void* buff, size_t count) {
  int fd = *(int*)handle;
  ssize_t written = write(fd, buff, count);
  if(written < 0) {
    return(0);
  }
  return((size_t)written);
}

size_t tty_read(service_com_handle_t handle, void* buff, size_t count) {
  int fd = *(int*)handle;
  ssize_t rd = read(fd, buff, count);
  if(rd < 0) {
    return(0);
  }
  return((size_t)rd);
}

size_t tty_available(service_com_handle_t handle) {
  int fd = *(int*)handle;
  int bytes;
  // Note: in some systems, ioctl could return a non-zero value
  if (ioctl(fd, FIONREAD, &bytes) == 0) {
    return bytes;
  }
  return 0;
}

service_com_handle_t tty_handle_malloc(void) {
  return((service_com_handle_t*)malloc(sizeof(int)));
}

void tty_handle_free(service_com_handle_t handle) {
  free(handle);
}

service_com_t backend_posix_tty = {
  .open = tty_open,
  .close = tty_close,
  .write = tty_write,
  .read = tty_read,
  .available = tty_available,
  .com_handle_malloc = tty_handle_malloc,
  .com_handle_free = tty_handle_free,
};
