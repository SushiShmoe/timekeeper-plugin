#include <yaffs/yaffsfs.h>
#include <yaffs/yaffs_guts.h>
#include <yaffs/yaffs_nor_drv.h>
#include <yaffs/yaffs_ram_drv.h>
#include <yaffs/yaffs_spn_fl512s_device.h>

#include <string.h>
#include <time.h>

#include <service/service_os.h>
#include <service/service_filesystem.h>
#include <service/service_err_codes.h>

// TODO pass from cmd on init?
#define YAFFS_PATH_ROOT                    "/"

#define YAFFS_CHECK_RET(X)              { if((X) != 0) { return(SERVICE_ERR_FILESYSTEM(-(X))); } else { return(SERVICE_ERR_NONE); } }

static struct yaffs_dev* yaffs_device = NULL;

// forward declaration of functions that modify members of the backend struct
bool y_check(int res);
int y_mount(void);
int y_format(void);

service_fs_file_handle_t y_file_handle_malloc() {
  return(service_os_malloc(sizeof (int)));
}

void y_file_handle_free(service_fs_file_handle_t handle) {
  service_os_free(handle);
}

service_fs_dir_handle_t y_dir_handle_malloc() {
  return(service_os_malloc(sizeof (yaffs_DIR*)));
}

void y_dir_handle_free(service_fs_dir_handle_t handle) {
  service_os_free(handle);
}

bool y_is_file(service_fs_file_stat_t* stat) {
  if(!stat) {
    return(false);
  }
  return((stat->attr & S_IFREG) == S_IFREG);
}

uint32_t y_get_free_space() {
  return(yaffs_freespace_reldev(yaffs_device));
}

uint32_t y_get_bad_blocks() {
  int bs[10];
  yaffs_count_blocks_by_state(yaffs_device, bs);
  return bs[YAFFS_BLOCK_STATE_DEAD];
}

bool y_path_validate(char* path) {
  // FIXME implement y_fix_relpath()?
  (void)path;
  return(true);
}

int y_init(void** args) {
  uint32_t start_gc = *(uint32_t*)args[0];
  const struct yaffsfs_init_opts yopts = {
    .start_gc_thread = (bool)start_gc,
  };

  yaffsfs_OSInitialisation(&yopts);

  // get drive ID (chip 0/1/ramdisk)
  uint32_t chip_id = *(uint32_t*)args[1];
  if(chip_id == 2) {
    // setup ram drive
    uint32_t ram_cap = *(uint32_t*)args[2];
    if(yaffs_ram_drv_init(ram_cap) != 0) {
      return(SERVICE_ERR_MALLOC_FAILED);
    }
  }

  yaffs_device = setup_flash_device_yaffs(YAFFS_PATH_ROOT, chip_id);
  if(!yaffs_device) {
    return(SERVICE_ERR_UNKNOWN);
  }

  return(SERVICE_ERR_NONE);
}

int y_unmount(void) {
  yaffsfs_GcThreadStop();

  // TODO use unmount2 to force?
  int rv = yaffs_unmount_reldev(yaffs_device);
  yaffsfs_GcThreadStart();

  YAFFS_CHECK_RET(rv);
}

int y_open(service_fs_file_handle_t handle, const char* path, int mode) {
  int* fd_ptr = (int*)handle;
  *fd_ptr = yaffs_open(path, (mode_t)mode);
  if(*fd_ptr < 0) {
    return(SERVICE_ERR_INVALID_ARG);
  }
  return(SERVICE_ERR_NONE);
}

int y_close(service_fs_file_handle_t handle) {
  int* fd_ptr = (int*)handle;
  int rv = yaffs_close(*fd_ptr);
  YAFFS_CHECK_RET(rv);
}

int y_stat(const char* path, service_fs_file_stat_t* stat) {
  if(!stat) {
    return(SERVICE_ERR_INVALID_ARG);
  }
  struct yaffs_stat statbuf = { 0 };
  int rv = yaffs_stat(path, &statbuf);
  stat->size = statbuf.st_size;
  stat->attr = statbuf.st_mode;
  YAFFS_CHECK_RET(rv);
}

int y_unlink(const char* path) {
  int rv = yaffs_unlink(path);
  YAFFS_CHECK_RET(rv);
}

int y_mkdir(const char* path, int mode) {
  int rv = yaffs_mkdir(path, (mode_t)mode);
  YAFFS_CHECK_RET(rv);
}

int y_rmdir(const char* path) {
  int rv = yaffs_rmdir(path);
  YAFFS_CHECK_RET(rv);
}

int y_rename(const char* path_old, const char* path_new) {
  int rv = yaffs_rename(path_old, path_new);
  YAFFS_CHECK_RET(rv);
}

size_t y_write(service_fs_file_handle_t handle, const void* buff, size_t count) {
  int* fd_ptr = (int*)handle;
  int rv = yaffs_write(*fd_ptr, buff, count);
  if(rv < 0) {
    return(0);
  }
  return(rv);
}
size_t y_read(service_fs_file_handle_t handle, void* buff, size_t count) {
  int* fd_ptr = (int*)handle;
  int rv = yaffs_read(*fd_ptr, buff, count);
  if(rv < 0) {
    return(0);
  }
  return(rv);
}

size_t y_tell(service_fs_file_handle_t handle) {
  int* fd_ptr = (int*)handle;
  return((size_t)yaffs_tell(*fd_ptr));
}

int y_lseek(service_fs_file_handle_t handle, size_t offset) {
  int* fd_ptr = (int*)handle;
  size_t rv = yaffs_lseek(*fd_ptr, offset, SEEK_SET);
  if(rv != offset) {
    return(SERVICE_ERR_INVALID_ARG);
  }
  return(SERVICE_ERR_NONE);
}

int y_opendir(service_fs_dir_handle_t handle, const char* path) {
  yaffs_DIR* dir = yaffs_opendir_reldev(yaffs_device, path);
  if(!dir) {
    return(SERVICE_ERR_MALLOC_FAILED);
  }
  yaffs_DIR** dir_ptr = (yaffs_DIR**)handle;
  *dir_ptr = dir;
  return(SERVICE_ERR_NONE);
}

int y_readdir(service_fs_dir_handle_t handle, service_fs_file_stat_t* stat) {
  if(!stat) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  struct yaffs_dirent* ent = yaffs_readdir(*(yaffs_DIR**)handle);
  if(!ent) {
    return(SERVICE_ERR_MALLOC_FAILED);
  }

  struct yaffs_stat statbuf = { 0 };
  int rv = yaffs_stat_obj(ent->obj, &statbuf);
  if(rv < 0) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  stat->size = statbuf.st_size;
  const time_t time = (time_t)statbuf.yst_mtime;
  struct tm* t = gmtime(&time);
  memcpy(&stat->mtime, t, sizeof(struct tm));
  strncpy(stat->name, ent->d_name, SERVICE_FS_MAX_FILENAME_LEN);
  stat->attr = 0;
  if(statbuf.st_mode & S_IFREG) { stat->attr |= SERVICE_FS_ATTR_ARCH; };
  if(statbuf.st_mode & S_IFDIR) { stat->attr |= SERVICE_FS_ATTR_DIR; };
  // TODO how to check hidden, system and readonly?
  return(SERVICE_ERR_NONE);
}

int y_closedir(service_fs_dir_handle_t handle) {
  int rv = yaffs_closedir(*(yaffs_DIR**)handle);
  YAFFS_CHECK_RET(rv);
}

int y_flush(service_fs_file_handle_t handle) {
  int* fd_ptr = (int*)handle;
  int rv = yaffs_flush(*fd_ptr);
  YAFFS_CHECK_RET(rv);
}

int y_eof(service_fs_file_handle_t handle) {
  int* fd_ptr = (int*)handle;
  int rv = yaffs_eof(*fd_ptr);
  YAFFS_CHECK_RET(rv);
}

size_t y_size(service_fs_file_handle_t handle) {
  int* fd_ptr = (int*)handle;
  struct yaffs_stat statbuf = { 0 };
  int rv = yaffs_fstat(*fd_ptr, &statbuf);
  if(rv != YAFFS_OK) {
    return(0);
  }
  return(statbuf.st_size);
}

int y_chmod(const char* path, uint32_t mode) {
  mode_t y_mode = 0;
  // TODO configure mode
  (void)mode;
  int rv = yaffs_chmod(path, y_mode);
  YAFFS_CHECK_RET(rv);
}

int y_gc(void) {
  int rv = yaffs_manual_gc_reldev(yaffs_device);
  if(rv != YAFFS_OK) {
    return(SERVICE_ERR_FILESYSTEM(rv));
  }
  return(SERVICE_ERR_NONE);
}

int y_wipe(void) {
  yaffsfs_GcThreadStop();

  if(yaffs_device->is_mounted) {
    // return code is disregarded on purpose
    yaffs_unmount2_reldev(yaffs_device, true);
  }

  // TODO manual erase block-by-block in case this fails?
  int rv = Y_ChipErase();
  if(rv != YAFFS_OK) {
    return(SERVICE_ERR_UNKNOWN);
  }

  yaffs_mount_reldev(yaffs_device);

  yaffsfs_GcThreadStart();
  return(SERVICE_ERR_NONE);
}

service_fs_t backend_fs_yaffs = {
  .mode = {
    .RDONLY = O_RDONLY,
    .WRONLY = O_WRONLY,
    .RDWR = O_RDWR,
    .CREAT = O_CREAT,
    .EXCL = O_EXCL,
    .TRUNC = O_TRUNC,
    .APPEND = O_APPEND,
  },
  .stat_init = -1,
  .stat_last = -1,
  .stat_err = 0,
  .total_space = 0,
  .file_handle_malloc = y_file_handle_malloc,
  .file_handle_free = y_file_handle_free,
  .dir_handle_malloc = y_dir_handle_malloc,
  .dir_handle_free = y_dir_handle_free,
  .check = y_check,
  .is_file = y_is_file,
  .get_free_space = y_get_free_space,
  .get_bad_blocks = y_get_bad_blocks,
  .path_validate = y_path_validate,
  .init = y_init,
  .mount = y_mount,
  .unmount = y_unmount,
  .format = y_format,
  .open = y_open,
  .close = y_close,
  .stat = y_stat,
  .unlink = y_unlink,
  .mkdir = y_mkdir,
  .rmdir = y_rmdir,
  .rename = y_rename,
  .write = y_write,
  .read = y_read,
  .tell = y_tell,
  .lseek = y_lseek,
  .opendir = y_opendir,
  .readdir = y_readdir,
  .closedir = y_closedir,
  .flush = y_flush,
  .eof = y_eof,
  .size = y_size,
  .chmod = y_chmod,
  .gc = y_gc,
  .wipe = y_wipe,
};

bool y_check(int res) {
  backend_fs_yaffs.stat_last = res;
  if(res != 0) {
    backend_fs_yaffs.stat_err = res;
  }
  return(res == 0);
}

int y_mount(void) {
  // TODO use mount2 to use force+readonly?
  backend_fs_yaffs.stat_init = yaffs_mount_reldev(yaffs_device);
  if(backend_fs_yaffs.stat_init == 0) {
    yaffsfs_GcThreadStart();
  }
  YAFFS_CHECK_RET(backend_fs_yaffs.stat_init);
}

int y_format(void) {
  // format may be called prior to init!!!
  if(!yaffs_device) {
    // TODO init here?
    return(SERVICE_ERR_UNKNOWN);
  }

  // TODO how to pass the flags?
  backend_fs_yaffs.stat_init = yaffs_format_reldev(yaffs_device, 1, 1, 1);
  YAFFS_CHECK_RET(backend_fs_yaffs.stat_init);

  // TODO remount here?
}