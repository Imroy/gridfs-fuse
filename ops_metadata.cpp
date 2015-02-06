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

#include <pwd.h>
#include <grp.h>

#include "operations.h"
#include "options.h"
#include "utils.h"

unsigned int subdir_count(mongo::DBClientBase &client, std::string path) {
  mongo::BSONObj proj = BSON("mode" << 1);
  std::string path_start = path;
  if (path.length() > 0) {
    path_start += "/";
  }

  std::unique_ptr<mongo::DBClientCursor> cursor = client.query(db_name() + ".files",
      BSON("filename" <<
           BSON("$regex" << ("^" + path_start + "/[^/]*$"))
          ),
      0, 0,
      &proj);
  std::string lastFN;
  unsigned int count = 0;
  while (cursor->more()) {
    mongo::BSONObj file_obj = cursor->next();
    if (S_ISDIR(file_obj["mode"].Int())) {
      count++;
    }
  }

  return count;
}

int gridfs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));

  auto sdc = make_ScopedDbConnection();
  mongo::DBClientBase &client = sdc->conn();

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2;
    fuse_context *context = fuse_get_context();
    stbuf->st_uid = context->uid;
    stbuf->st_gid = context->gid;
    return 0;
  }

  path = fuse_to_mongo_path(path);

  auto lgf = get_open(path);
  if (lgf) {
    stbuf->st_mode = S_IFREG | (lgf->Mode() & (0xffff ^ S_IFMT));
    stbuf->st_nlink = 1;
    stbuf->st_uid = lgf->Uid();
    stbuf->st_gid = lgf->Gid();
    stbuf->st_ctime = time(NULL);
    stbuf->st_mtime = time(NULL);
    stbuf->st_size = lgf->Length();
    return 0;
  }

  mongo::BSONObj file_obj = client.findOne(db_name() + ".files",
                            BSON("filename" << path));

  if (file_obj.isEmpty()) {
    return -ENOENT;
  }

  passwd *pw = getpwnam(file_obj.hasField("owner") ? file_obj["owner"].str().c_str() : "root");
  if (pw) {
    stbuf->st_uid = pw->pw_uid;
  }

  group *gr = getgrnam(file_obj.hasField("group") ? file_obj["group"].str().c_str() : "root");
  if (gr) {
    stbuf->st_gid = gr->gr_gid;
  }

  stbuf->st_mode = file_obj.hasField("mode") ? file_obj["mode"].Int() : S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
  if (S_ISREG(stbuf->st_mode)) {
    stbuf->st_nlink = 1;
    stbuf->st_size = file_obj["length"].Int();
    stbuf->st_blocks = stbuf->st_size >> 9;
  }
  if (S_ISDIR(stbuf->st_mode)) {
    stbuf->st_nlink = 2 + subdir_count(client, path);
  }
  if (S_ISLNK(stbuf->st_mode)) {
    stbuf->st_size = file_obj["target"].String().length();
  }

  time_t upload_time = mongo_time_to_unix_time(file_obj["uploadDate"].date());
  stbuf->st_ctime = upload_time;
  stbuf->st_mtime = upload_time;

  return 0;
}

int gridfs_chmod(const char *path, mode_t mode) {
  path = fuse_to_mongo_path(path);
  auto lgf = get_open(path);
  if (lgf) {
    lgf->setMode(mode);
  }

  auto sdc = make_ScopedDbConnection();
  sdc->conn().update(db_name() + ".files",
                     BSON("filename" << path),
                     BSON("$set" << BSON("mode" << mode)));

  return 0;
}

int gridfs_chown(const char *path, uid_t uid, gid_t gid) {
  path = fuse_to_mongo_path(path);

  auto lgf = get_open(path);
  if (lgf) {
    lgf->setUid(uid);
    lgf->setGid(gid);
  }

  mongo::BSONObjBuilder b;

  {
    passwd *pw = getpwuid(uid);
    if (pw) {
      b.append("owner", pw->pw_name);
    }
  }
  {
    group *gr = getgrgid(gid);
    if (gr) {
      b.append("group", gr->gr_name);
    }
  }

  if (b.hasField("owner") || b.hasField("group")) {
    auto sdc = make_ScopedDbConnection();
    sdc->conn().update(db_name() + ".files",
                       BSON("filename" << path),
                       BSON("$set" << b.obj()));
  }

  return 0;
}

int gridfs_utimens(const char *path, const struct timespec tv[2]) {
  path = fuse_to_mongo_path(path);

  unsigned long long millis = ((unsigned long long)tv[1].tv_sec * 1000) + (tv[1].tv_nsec / 1e+6);

  auto sdc = make_ScopedDbConnection();
  sdc->conn().update(db_name() + ".files",
                     BSON("filename" << path),
                     BSON("$set" <<
                          BSON("uploadDate" << mongo::Date_t(millis))
                         ));

  return 0;
}

int gridfs_rename(const char *old_path, const char *new_path) {
  old_path = fuse_to_mongo_path(old_path);
  new_path = fuse_to_mongo_path(new_path);

  auto sdc = make_ScopedDbConnection();
  mongo::DBClientBase &client = sdc->conn();

  mongo::BSONObj file_obj = client.findOne(db_name() + ".files",
                            BSON("filename" << old_path));

  if (file_obj.isEmpty()) {
    return -ENOENT;
  }

  client.update(db_name() + ".files",
                BSON("_id" << file_obj.getField("_id")),
                BSON("$set" << BSON("filename" << new_path)));

  return 0;
}

