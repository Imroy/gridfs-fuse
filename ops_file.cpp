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

#include <algorithm>
#include <pwd.h>
#include <grp.h>

#include <mongo/bson/bson.h>
#include <mongo/client/gridfs.h>

#include "operations.h"
#include "utils.h"
#include "options.h"

unsigned int FH = 1;

int gridfs_open(const char *path, struct fuse_file_info *fi) {
  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    return -EACCES;
  }

  path = fuse_to_mongo_path(path);

  if (get_open(path)) {
    return 0;
  }

  auto sdc = make_ScopedDbConnection();
  mongo::GridFS gf = get_gridfs(sdc);

  mongo::GridFile file = gf.findFile(path);

  if (!file.exists()) {
    return -ENOENT;
  }

  return 0;
}

int gridfs_release(const char *path, struct fuse_file_info *ffi) {
  // fh is not set if file is opened read only
  // Would check ffi->flags for O_RDONLY instead but MacFuse doesn't
  // seem to properly pass flags into release
  if (!ffi->fh) {
    return 0;
  }

  path = fuse_to_mongo_path(path);
  remove_open(path);
  return 0;
}

int gridfs_create(const char *path, mode_t mode, struct fuse_file_info *ffi) {
  fuse_context *context = fuse_get_context();
  path = fuse_to_mongo_path(path);
  set_open(path, context->uid, context->gid, mode);

  ffi->fh = FH++;

  return 0;
}

int gridfs_unlink(const char *path) {
  auto sdc = make_ScopedDbConnection();
  mongo::GridFS gf = get_gridfs(sdc);

  path = fuse_to_mongo_path(path);
  gf.removeFile(path);

  return 0;
}

int gridfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  path = fuse_to_mongo_path(path);

  auto lgf = get_open(path);
  if (lgf) {
    return lgf->read(buf, size, offset);
  }

  auto sdc = make_ScopedDbConnection();
  mongo::GridFS gf = get_gridfs(sdc);
  mongo::GridFile file = gf.findFile(path);

  if (!file.exists()) {
    return -EBADF;
  }

  int chunk_size = file.getChunkSize();
  int chunk_num = offset / chunk_size;

  size_t len = 0;
  while (len < size && chunk_num < file.getNumChunks()) {
    mongo::GridFSChunk chunk = file.getChunk(chunk_num);
    int to_read;
    int cl = chunk.len();

    const char *d = chunk.data(cl);

    if (len) {
      to_read = std::min((long unsigned)cl, (long unsigned)(size - len));
      memcpy(buf + len, d, to_read);
    } else {
      to_read = std::min((long unsigned)(cl - (offset % chunk_size)), (long unsigned)(size - len));
      memcpy(buf + len, d + (offset % chunk_size), to_read);
    }

    len += to_read;
    chunk_num++;
  }

  return len;
}

int gridfs_write(const char *path, const char *buf, size_t nbyte, off_t offset, struct fuse_file_info *ffi) {
  path = fuse_to_mongo_path(path);
  auto lgf = get_open(path);
  if (!lgf) {
    return -ENOENT;
  }
  return lgf->write(buf, nbyte, offset);
}

int gridfs_flush(const char *path, struct fuse_file_info *ffi) {
  if (!ffi->fh) {
    return 0;
  }

  path = fuse_to_mongo_path(path);
  auto lgf = get_open(path);
  if (!lgf) {
    return -ENOENT;
  }

  if (lgf->is_clean()) {
    return 0;
  }

  auto sdc = make_ScopedDbConnection();
  mongo::GridFS gf = get_gridfs(sdc);

  if (gf.findFile(path).exists()) {
    gf.removeFile(path);
  }

  size_t len = lgf->Length();
  char *buf = new char[len];
  lgf->read(buf, len, 0);

  gf.storeFile(buf, len, path);

  mongo::BSONObjBuilder b;
  {
    passwd *pw = getpwuid(lgf->Uid());
    if (pw) {
      b.append("owner", pw->pw_name);
    }
  }
  {
    group *gr = getgrgid(lgf->Gid());
    if (gr) {
      b.append("group", gr->gr_name);
    }
  }
  b.append("mode", lgf->Mode());

  sdc->conn().update(db_name() + ".files",
                     BSON("filename" << path),
                     BSON("$set" << b.obj()));

  lgf->set_flushed();

  return 0;
}

