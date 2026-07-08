#include <service/service_os.h>
#include <service/service_filesystem.h>
#include <service/service_err_codes.h>

#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

// TODO make file permissions configurable?
#define SERVICE_FS_POSIX_CREATE_PERMISSIONS       (0777)

// FIXME return code checking is not resolved well at the moment

service_fs_file_handle_t posix_file_malloc(void) {
  return((service_fs_file_handle_t*)malloc(sizeof(int)));
}

void posix_file_handle_free(service_fs_file_handle_t handle) {
  free(handle);
}

service_fs_dir_handle_t posix_dir_handle_malloc(void) {
  return((service_fs_file_handle_t*)malloc(sizeof(int)));
}

void posix_dir_handle_free(service_fs_dir_handle_t handle) {
  free(handle);
}

bool posix_check(int res);

bool posix_is_file(service_fs_file_stat_t* st) {
  if(!st) {
    return(false);
  }
  // FIXME implement this e.g. by checking file name
  return(true);
}

uint32_t posix_get_free_space() {
  // FIXME emulate drive as e.g. some directory?
  return(1024UL*1024UL);
}

uint32_t posix_get_bad_blocks() {
  return(0);
}

bool posix_path_validate(char* path) {
  (void)path;
  // FIXME is some validation needed?
  return(true);
}

int posix_init(void** args) {
  // FIXME emulate drive as e.g. some directory?
  // this always returns OK even though it's not implemented
  (void)args;
  return(SERVICE_ERR_NONE);
}

int posix_mount(void);

int posix_unmount(void) {
  // FIXME emulate drive as e.g. some directory?
  return(SERVICE_ERR_CB_NOT_FOUND);
}

int posix_format(void) {
  // FIXME emulate drive as e.g. some directory?
  return(SERVICE_ERR_CB_NOT_FOUND);
}

int posix_open(service_fs_file_handle_t handle, const char* path, int mode) {
  int fd = 0;
  if(mode & O_CREAT) {
    fd = open(path, (mode_t)mode, SERVICE_FS_POSIX_CREATE_PERMISSIONS);
  } else {
    fd = open(path, (mode_t)mode);
  }

  if(fd < 0) {
    return(SERVICE_ERR_FILESYSTEM(fd));
  }

  *(int*)handle = fd;
  return(SERVICE_ERR_NONE);
}

int posix_close(service_fs_file_handle_t handle) {
  int fd = *(int*)handle;
  return(close(fd));
}

int posix_stat(const char* path, service_fs_file_stat_t* st) {
  if(!st) {
    return(SERVICE_ERR_INVALID_ARG);
  }
  struct stat statbuf;
  int res = stat(path, &statbuf);
  st->size = statbuf.st_size;
  return(res);
}

int posix_unlink(const char* path) {
  return(unlink(path));
}

int posix_mkdir(const char* path, int mode) {
  return(mkdir(path, (mode_t)mode));
}

int posix_rmdir(const char* path) {
  return(rmdir(path));
}

int posix_rename(const char* path_old, const char* path_new) {
  return(rename(path_old, path_new));
}

size_t posix_write(service_fs_file_handle_t handle, const void* buff, size_t count) {
  int fd = *(int*)handle;
  ssize_t written = write(fd, buff, count);
  if(written < 0) {
    posix_check(written);
    return(0);
  }
  return((size_t)written);
}

size_t posix_read(service_fs_file_handle_t handle, void* buff, size_t count) {
  int fd = *(int*)handle;
  ssize_t rd = read(fd, buff, count);
  if(rd < 0) {
    posix_check(rd);
    return(0);
  }
  return(rd);
}

size_t posix_tell(service_fs_file_handle_t handle) {
  int fd = *(int*)handle;
  return(lseek(fd, 0, SEEK_CUR));
}

int posix_lseek(service_fs_file_handle_t handle, size_t offset) {
  int fd = *(int*)handle;
  return(lseek(fd, offset, SEEK_SET));
}

int posix_opendir(service_fs_dir_handle_t handle, const char* path) {
  handle = opendir(path);
  return(SERVICE_ERR_NONE);
}

int posix_readdir(service_fs_dir_handle_t handle, service_fs_file_stat_t* st) {
  if(!st) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  // FIXME implement other parameters (e.g. time)

  struct dirent* statbuf = readdir((DIR*)handle);
  st->size = statbuf->d_reclen;
  return(SERVICE_ERR_NONE);
}

int posix_closedir(service_fs_dir_handle_t handle) {
  return(closedir((DIR*)handle));
}

int posix_flush(service_fs_dir_handle_t handle) {
  (void)handle;
  return(SERVICE_ERR_NONE);
}

int posix_eof(service_fs_file_handle_t handle) {
  int fd = *(int*)handle;
  size_t cur = lseek(fd, 0, SEEK_CUR);
  size_t end = lseek(fd, 0, SEEK_END);
  return(cur == end);
}

size_t posix_size(service_fs_file_handle_t handle) {
  int fd = *(int*)handle;
  lseek(fd, 0, SEEK_END);
  return(lseek(fd, 0, SEEK_CUR));
}

int posix_chmod(const char* path, uint32_t mode) {
  // FIXME the argument should be converted from SERVICE_FS_ATTR_*!
  return(chmod(path, mode));
}

int posix_gc(void) {
  return(SERVICE_ERR_CB_NOT_FOUND);
}

int posix_wipe(void) {
  return(SERVICE_ERR_CB_NOT_FOUND);
}

service_fs_t backend_fs_posix = {
  .mode = {
    .RDONLY = O_RDONLY,
    .WRONLY = O_WRONLY,
    .RDWR = O_RDWR,
    .CREAT = O_CREAT,
    .EXCL = O_EXCL,
    .TRUNC = O_TRUNC,
    .APPEND = O_APPEND,
  },
  .stat_init = SERVICE_ERR_UNKNOWN,
  .stat_last = SERVICE_ERR_UNKNOWN,
  .stat_err = SERVICE_ERR_NONE,
  .total_space = 0,
  .file_handle_malloc = posix_file_malloc,
  .file_handle_free = posix_file_handle_free,
  .dir_handle_malloc = posix_dir_handle_malloc,
  .dir_handle_free = posix_dir_handle_free,
  .check = posix_check,
  .is_file = posix_is_file,
  .get_free_space = posix_get_free_space,
  .get_bad_blocks = posix_get_bad_blocks,
  .path_validate = posix_path_validate,
  .init = posix_init,
  .mount = posix_mount,
  .unmount = posix_unmount,
  .format = posix_format,
  .open = posix_open,
  .close = posix_close,
  .stat = posix_stat,
  .unlink = posix_unlink,
  .mkdir = posix_mkdir,
  .rmdir = posix_rmdir,
  .rename = posix_rename,
  .write = posix_write,
  .read = posix_read,
  .tell = posix_tell,
  .lseek = posix_lseek,
  .opendir = posix_opendir,
  .readdir = posix_readdir,
  .closedir = posix_closedir,
  .flush = posix_flush,
  .eof = posix_eof,
  .size = posix_size,
  .chmod = posix_chmod,
  .gc = posix_gc,
  .wipe = posix_wipe,
};

bool posix_check(int res) {
  backend_fs_posix.stat_last = res;
  if(res != 0) {
    backend_fs_posix.stat_err = res;
  }
  return(res == 0);
}

int posix_mount(void) {
  // FIXME emulate drive as e.g. some directory?
  backend_fs_posix.stat_init = SERVICE_ERR_NONE;
  return(SERVICE_ERR_NONE);
}
