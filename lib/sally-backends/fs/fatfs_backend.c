#include <ff.h>

#include <string.h>
#include <time.h>

#include <service/service_os.h>
#include <service/service_filesystem.h>
#include <service/service_err_codes.h>

#define FS_PATH_ROOT                    "/"

// TODO this should set last error
#define FATFS_CHECK_RET(X)              { if((X) != FR_OK) { return(SERVICE_ERR_FILESYSTEM((X))); } else { return(SERVICE_ERR_NONE); } }

// FatFs work area needed for each volume
static FATFS fatFs = { 0 };
static BYTE* work = NULL;
static const size_t work_size = sizeof(BYTE) * FF_MAX_SS;

// forward declaration of functions that modify members of the backend struct
int ff_mount(void);
bool ff_check(int res);
int ff_format(void);

int ff_init(void** args) {
  (void)args;
  if(!work) {
    // allocate the working buffer
    work = service_os_malloc(work_size);
    if(!work) {
      return(SERVICE_ERR_UNKNOWN);
    }

    memset(work, 0, work_size);
  }

  return(SERVICE_ERR_NONE);
}

int ff_unmount(void) {
  FRESULT fres = f_mount(NULL, FS_PATH_ROOT, 0);
  FATFS_CHECK_RET(fres);
}

service_fs_file_handle_t ff_file_handle_malloc() {
  return(service_os_malloc(sizeof (FIL)));
}

void ff_file_handle_free(service_fs_file_handle_t handle) {
  service_os_free(handle);
}

service_fs_dir_handle_t ff_dir_handle_malloc() {
  return(service_os_malloc(sizeof (DIR)));
}

void ff_dir_handle_free(service_fs_dir_handle_t handle) {
  service_os_free(handle);
}

bool ff_is_file(service_fs_file_stat_t* stat) {
  if(!stat) {
    return(false);
  }
  return(stat->name[0] != '\0');
}

int ff_open(service_fs_file_handle_t handle, const char* path, int mode) {
  FRESULT fres = f_open((FIL*)handle, path, mode);
  FATFS_CHECK_RET(fres);
}

int ff_close(service_fs_file_handle_t handle) {
  FRESULT fres = f_close((FIL*)handle);
  FATFS_CHECK_RET(fres);
}

int ff_stat(const char* path, service_fs_file_stat_t* stat) {
  if(!stat) {
    return(SERVICE_ERR_INVALID_ARG);
  }
  FILINFO statbuf;
  FRESULT rv = f_stat(path, &statbuf);
  stat->size = statbuf.fsize;
  FATFS_CHECK_RET(rv);
}

int ff_unlink(const char* path) {
  FRESULT fres = f_unlink(path);
  FATFS_CHECK_RET(fres);
}

int ff_mkdir(const char* path, int mode) {
  (void)mode;
  FRESULT fres = f_mkdir(path);
  FATFS_CHECK_RET(fres);
}

int ff_rmdir(const char* path) {
  FRESULT fres = f_rmdir(path);
  FATFS_CHECK_RET(fres);
}

int ff_rename(const char* path_old, const char* path_new) {
  FRESULT fres = f_rename(path_old, path_new);
  FATFS_CHECK_RET(fres);
}

size_t ff_write(service_fs_file_handle_t handle, const void* buff, size_t count) {
  UINT written = 0;
  FRESULT rv = f_write((FIL*)handle, (void*)buff, (UINT)count, &written);
  if(!ff_check(rv)) {
    return(0);
  }
  return(written);
}

size_t ff_read(service_fs_file_handle_t handle, void* buff, size_t count) {
  UINT read = 0;
  FRESULT rv = f_read((FIL*)handle, buff, (UINT)count, &read);
  if(!ff_check(rv)) {
    return(0);
  }
  return(read);
}

size_t ff_tell(service_fs_file_handle_t handle) {
  return(f_tell((FIL*)handle));
}

int ff_lseek(service_fs_file_handle_t handle, size_t offset) {
  FRESULT fres = f_lseek((FIL*)handle, offset);
  FATFS_CHECK_RET(fres);
}

int ff_opendir(service_fs_dir_handle_t handle, const char* path) {
  FRESULT fres = f_opendir((DIR*)handle, path);
  FATFS_CHECK_RET(fres);
}

int ff_readdir(service_fs_dir_handle_t handle, service_fs_file_stat_t* stat) {
  if(!stat) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  FILINFO statbuf;
  FRESULT rv = f_readdir((DIR*)handle, &statbuf);
  if(strlen(statbuf.fname) == 0) {
    // FatFS returns OK status even if the next file does not exist
    stat->name[0] = '\0';
    return(SERVICE_ERR_NONE);
  }

  if(ff_check(rv)) {
    stat->size = statbuf.fsize;
    stat->mtime.tm_year = (statbuf.fdate & (0x7F << 9)) >> 9;
    stat->mtime.tm_mon = (statbuf.fdate & (0x0F << 5)) >> 5;
    stat->mtime.tm_mday = (statbuf.fdate & (0x1F << 0)) >> 0;
    stat->mtime.tm_hour = (statbuf.ftime & (0x1F << 11)) >> 11;
    stat->mtime.tm_min = (statbuf.ftime & (0x3F << 5)) >> 5;
    stat->mtime.tm_sec = (statbuf.ftime & (0x1F << 0)) >> 0;
    strncpy(stat->name, statbuf.fname, SERVICE_FS_MAX_FILENAME_LEN);
    stat->attr = 0;
    if(statbuf.fattrib & AM_RDO) { stat->attr |= SERVICE_FS_ATTR_RO; };
    if(statbuf.fattrib & AM_HID) { stat->attr |= SERVICE_FS_ATTR_HIDDEN; };
    if(statbuf.fattrib & AM_SYS) { stat->attr |= SERVICE_FS_ATTR_SYSTEM; };
    if(statbuf.fattrib & AM_DIR) { stat->attr |= SERVICE_FS_ATTR_DIR; };
    if(statbuf.fattrib & AM_ARC) { stat->attr |= SERVICE_FS_ATTR_ARCH; };
  }
  FATFS_CHECK_RET(rv);
}

int ff_closedir(service_fs_dir_handle_t handle) {
  FRESULT fres = f_closedir((DIR*)handle);
  FATFS_CHECK_RET(fres);
}

int ff_flush(service_fs_file_handle_t handle) {
  FRESULT fres = f_sync((FIL*)handle);
  FATFS_CHECK_RET(fres);
}

int ff_eof(service_fs_file_handle_t handle) {
  FRESULT fres = f_eof((FIL*)handle);
  FATFS_CHECK_RET(fres);
}

size_t ff_size(service_fs_file_handle_t handle) {
  UINT size = f_size((FIL*)handle);
  return(size);
}

uint32_t ff_get_free_space() {
  DWORD nclst;
  FATFS* fatfs;
  FRESULT ret = f_getfree("", &nclst, &fatfs);
  if(!ff_check(ret)) {
    return(0);
  }
  return(nclst * 512);
}

uint32_t ff_get_bad_blocks() {
  // FatFS has no bad blocks
  return(0);
}

bool ff_path_validate(char* path) {
  // FIXME any validation needed?
  (void)path;
  return(true);
}

int ff_wipe(void) {
  return(SERVICE_ERR_UNKNOWN);
}

int ff_chmod(const char* path, uint32_t mode) {
  UINT f_mode = 0;
  if(mode & SERVICE_FS_ATTR_RO) { f_mode |= AM_RDO; }
  if(mode & SERVICE_FS_ATTR_HIDDEN) { f_mode |= AM_HID; }
  if(mode & SERVICE_FS_ATTR_SYSTEM) { f_mode |= AM_SYS; }
  if(mode & SERVICE_FS_ATTR_DIR) { f_mode |= AM_DIR; }
  if(mode & SERVICE_FS_ATTR_ARCH) { f_mode |= AM_ARC; }
  FRESULT fres = f_chmod(path, f_mode, 0xFF);
  FATFS_CHECK_RET(fres);
}

int ff_gc(void) {
  // FatFS has no garbage collector
  return(SERVICE_ERR_CB_NOT_FOUND);
}

service_fs_t backend_fs_ff = {
  .mode = {
    .RDONLY = FA_READ,
    .WRONLY = FA_WRITE,
    .RDWR = FA_READ | FA_WRITE,
    .CREAT = FA_CREATE_NEW,
    .EXCL = FA_CREATE_ALWAYS,
    .TRUNC = FA_OPEN_ALWAYS,
    .APPEND = FA_OPEN_APPEND,
  },
  .stat_init = FR_NOT_ENABLED,
  .stat_last = FR_NOT_READY,
  .stat_err = FR_OK,
  .total_space = 0,
  .file_handle_malloc = ff_file_handle_malloc,
  .file_handle_free = ff_file_handle_free,
  .dir_handle_malloc = ff_dir_handle_malloc,
  .dir_handle_free = ff_dir_handle_free,
  .check = ff_check,
  .is_file = ff_is_file,
  .get_free_space = ff_get_free_space,
  .get_bad_blocks = ff_get_bad_blocks,
  .path_validate = ff_path_validate,
  .init = ff_init,
  .mount = ff_mount,
  .unmount = ff_unmount,
  .format = ff_format,
  .open = ff_open,
  .close = ff_close,
  .stat = ff_stat,
  .unlink = ff_unlink,
  .mkdir = ff_mkdir,
  .rmdir = ff_rmdir,
  .rename = ff_rename,
  .write = ff_write,
  .read = ff_read,
  .tell = ff_tell,
  .lseek = ff_lseek,
  .opendir = ff_opendir,
  .readdir = ff_readdir,
  .closedir = ff_closedir,
  .flush = ff_flush,
  .eof = ff_eof,
  .size = ff_size,
  .chmod = ff_chmod,
  .gc = ff_gc,
  .wipe = ff_wipe,
};

bool ff_check(int res) {
  backend_fs_ff.stat_last = res;
  if(res != FR_OK) {
    backend_fs_ff.stat_err = res;
  }
  return(res == FR_OK);
}

int ff_mount(void) {
  backend_fs_ff.stat_init = f_mount(&fatFs, FS_PATH_ROOT, 1);
  FATFS_CHECK_RET(backend_fs_ff.stat_init);
}

int ff_format(void) {
  // format may be called prior to init!!!
  if(!work) {
    // allocate the working buffer
    work = service_os_malloc(work_size);
    if(!work) {
      return(SERVICE_ERR_UNKNOWN);
    }
    memset(work, 0, work_size);
  }

  backend_fs_ff.stat_init = f_mkfs(FS_PATH_ROOT, 0, work, work_size);
  FATFS_CHECK_RET(backend_fs_ff.stat_init);

  FRESULT fres = f_mount(&fatFs, FS_PATH_ROOT, 1);
  FATFS_CHECK_RET(fres);
}
