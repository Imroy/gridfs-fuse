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
#include <string.h>

#include "operations.h"
#include "utils.h"

int gridfs_readlink(const char *path, char *buf, size_t size) {
  path = fuse_to_mongo_path(path);

  auto sdc = make_ScopedDbConnection();
  mongo::BSONObj file_obj = sdc->conn().findOne(db_name() + ".files",
                            BSON("filename" << path));

  if (file_obj.isEmpty()) {
    return -ENOENT;
  }

  if (!file_obj.hasField("target")) {
    return -ENOENT;
  }

  std::string target = file_obj["target"].String();
  strncpy(buf, target.c_str(), size);
  if (target.length() >= size) {
    buf[size] = 0;
  }

  return 0;
}

int gridfs_symlink(const char *target, const char *path) {
  path = fuse_to_mongo_path(path);

  mongo::OID id;
  id.init();
  fuse_context *context = fuse_get_context();

  mongo::BSONObjBuilder file;
  file << "_id" << id
       << "filename" << path
       << "chunkSize" << 0
       << "uploadDate" << mongo::DATENOW
       << "md5" << 0
       << "mode" << (S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO)
       << "target" << target;
  {
    passwd *pw = getpwuid(context->uid);
    if (pw) {
      file << "owner" << pw->pw_name;
    }
  }
  {
    group *gr = getgrgid(context->gid);
    if (gr) {
      file << "group" << gr->gr_name;
    }
  }

  auto sdc = make_ScopedDbConnection();
  sdc->conn().insert(db_name() + ".files", file.obj());

  return 0;
}

