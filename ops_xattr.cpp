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

#include "operations.h"
#include "utils.h"
#include "options.h"

#ifdef __linux__
#include <sys/xattr.h>
#endif

int gridfs_listxattr(const char *path, char *list, size_t size) {
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

  size_t len = 0;
  mongo::BSONObj metadata = file.getMetadata();
  std::set<std::string> field_set;
  metadata.getFieldNames(field_set);
  for (auto s : field_set) {
    std::string attr_name = namespace_xattr(s);
    int field_len = attr_name.size() + 1;
    len += field_len;
    if (len < size) {
      memcpy(list, attr_name.c_str(), field_len);
      list += field_len;
    }
  }

  if (size == 0) {
    return len;
  }
  if (len >= size) {
    return -ERANGE;
  }

  return len;
}

int gridfs_getxattr(const char *path, const char *name, char *value, size_t size) {
  if (strcmp(path, "/") == 0) {
    return -ENODATA;
  }

  const char *attr_name = unnamespace_xattr(name);
  if (!attr_name) {
    return -ENODATA;
  }

  path = fuse_to_mongo_path(path);

  if (get_open(path)) {
    return -ENOATTR;
  }

  auto sdc = make_ScopedDbConnection();
  mongo::GridFS gf = get_gridfs(sdc);
  mongo::GridFile file = gf.findFile(path);

  if (!file.exists()) {
    return -ENOENT;
  }

  mongo::BSONObj metadata = file.getMetadata();
  if (metadata.isEmpty()) {
    return -ENOATTR;
  }

  mongo::BSONElement field = metadata[attr_name];
  if (field.eoo()) {
    return -ENOATTR;
  }

  std::string field_str = field.toString();
  size_t len = field_str.size() + 1;
  if (size == 0) {
    return len;
  }
  if (len >= size) {
    return -ERANGE;
  }

  memcpy(value, field_str.c_str(), len);

  return len;
}

int gridfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
  if (strcmp(path, "/") == 0) {
    return -ENODATA;
  }

  const char *attr_name = unnamespace_xattr(name);
  if (!attr_name) {
    return -ENODATA;
  }

  path = fuse_to_mongo_path(path);

  if (get_open(path)) {
    return -ENOATTR;
  }

  auto sdc = make_ScopedDbConnection();
  mongo::DBClientBase &client = sdc->conn();

  mongo::BSONObj file_obj = client.findOne(db_name() + ".files",
                            BSON("filename" << path));

  if (file_obj.isEmpty()) {
    return -ENOENT;
  }

  client.update(db_name() + ".files",
                BSON("filename" << path),
                BSON("$set" <<
                     BSON((std::string("metadata.") + attr_name) << value)
                    ));

  return 0;
}

int gridfs_removexattr(const char *path, const char *name) {
  if (strcmp(path, "/") == 0) {
    return -ENODATA;
  }

  const char *attr_name = unnamespace_xattr(name);
  if (!attr_name) {
    return -ENODATA;
  }

  path = fuse_to_mongo_path(path);

  if (get_open(path)) {
    return -ENOATTR;
  }

  auto sdc = make_ScopedDbConnection();
  mongo::DBClientBase &client = sdc->conn();

  mongo::BSONObj file_obj = client.findOne(db_name() + ".files",
                            BSON("filename" << path));

  if (file_obj.isEmpty()) {
    return -ENOENT;
  }

  client.update(db_name() + ".files",
                BSON("filename" << path),
                BSON("$unset" <<
                     BSON((std::string("metadata.") + attr_name) << "")
                    ));

  return 0;
}

