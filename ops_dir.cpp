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

#include <mongo/bson/bson.h>
#include <mongo/client/gridfs.h>

#include "operations.h"
#include "options.h"
#include "utils.h"

int gridfs_mkdir(const char *path, mode_t mode) {
  path = fuse_to_mongo_path(path);

  auto sdc = make_ScopedDbConnection();
  mongo::DBClientBase &client = sdc->conn();

  mongo::OID id;
  id.init();
  fuse_context *context = fuse_get_context();

  mongo::BSONObjBuilder file;
  file << "_id" << id
       << "filename" << path
       << "chunkSize" << 0
       << "uploadDate" << mongo::DATENOW
       << "md5" << 0
       << "mode" << (mode | S_IFDIR);
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

  client.insert(db_name() + ".files",
                file.obj());

  return 0;
}

int gridfs_rmdir(const char *path) {
  auto sdc = make_ScopedDbConnection();
  mongo::GridFS gf = get_gridfs(sdc);

  path = fuse_to_mongo_path(path);
  gf.removeFile(path);

  return 0;
}

int gridfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
  path = fuse_to_mongo_path(path);

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  auto sdc = make_ScopedDbConnection();
  mongo::BSONObj proj = BSON("filename" << 1);
  std::string path_start = path;
  if (strlen(path) > 0) {
    path_start += "/";
  }
  std::unique_ptr<mongo::DBClientCursor> cursor = sdc->conn().query(db_name() + ".files",
      BSON("filename" <<
           BSON("$regex" << "^" + path_start)
          ),
      0, 0,
      &proj);
  std::string lastFN;
  while (cursor->more()) {
    std::string filename = cursor->next()["filename"].String();
    std::string rel = filename.substr(path_start.length());
    if (rel.find("/") != std::string::npos) {
      continue;
    }

    /* If this filename matches the last filename we've seen, *do not* add it to the buffer because it's a duplicate filename */
    if (lastFN != filename) {
      filler(buf, rel.c_str(), NULL, 0);
    }

    /* Update lastFN with our cursor's current filename */
    lastFN = filename;
    fprintf(stderr, "DEBUG: %s\n", lastFN.c_str());
  }

  auto opened = all_open();
  for (auto i : opened) {
    if (i.find(path_start) == 0) {
      std::string rel = i.substr(path_start.length());
      if (rel.find("/") != std::string::npos) {
        continue;
      }

      filler(buf, i.c_str(), NULL, 0);
    }
  }

  return 0;
}

