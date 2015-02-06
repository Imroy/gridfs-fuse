/*
 *  Copyright 2009 Michael Stephens
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OPERATIONS_H
#define __OPERATIONS_H

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26

// ENOATTR is not blessed by POSIX. Darwin uses 93.
#ifndef ENOATTR
#define ENOATTR 93
#endif

#include <mutex>
#include <map>
#include <fuse.h>
#include <mongo/client/connpool.h>

#include "local_gridfile.h"
#include "options.h"

LocalGridFile::ptr
get_open(const char *path);

LocalGridFile::ptr
set_open(const char *path, uid_t u, gid_t g, mode_t m, int chunkSize = DEFAULT_CHUNK_SIZE);

void
remove_open(const char *path);

std::vector<std::string>
all_open();


int gridfs_getattr(const char *path, struct stat *stbuf);

int gridfs_readlink(const char *path, char *buf, size_t size);

int gridfs_mkdir(const char *path, mode_t mode);

int gridfs_unlink(const char *path);

int gridfs_rmdir(const char *path);

int gridfs_symlink(const char *target, const char *path);

int gridfs_rename(const char *old_path, const char *new_path);

int gridfs_chmod(const char *path, mode_t mode);

int gridfs_chown(const char *path, uid_t uid, gid_t gid);

int gridfs_open(const char *path, struct fuse_file_info *fi);

int gridfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int gridfs_write(const char *path, const char *buf, size_t nbyte, off_t offset, struct fuse_file_info *ffi);

int gridfs_flush(const char *path, struct fuse_file_info *ffi);

int gridfs_release(const char *path, struct fuse_file_info *ffi);

int gridfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);

int gridfs_getxattr(const char *path, const char *name, char *value, size_t size);

int gridfs_listxattr(const char *path, char *list, size_t size);

int gridfs_removexattr(const char *path, const char *name);

int gridfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);

int gridfs_create(const char *path, mode_t mode, struct fuse_file_info *ffi);

int gridfs_utimens(const char *path, const struct timespec tv[2]);

std::shared_ptr<mongo::ScopedDbConnection> make_ScopedDbConnection(void);

inline mongo::GridFS get_gridfs(std::shared_ptr<mongo::ScopedDbConnection> sdc) {
  return mongo::GridFS(sdc->conn(), gridfs_options.db, gridfs_options.prefix);
}

#endif
