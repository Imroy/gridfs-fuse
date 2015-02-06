#ifndef _LOCAL_GRIDFILE_H
#define _LOCAL_GRIDFILE_H

#include <vector>
#include <cstring>
#include <iostream>
#include <memory>

#ifdef __linux__
#include "sys/types.h"
#endif

const unsigned int DEFAULT_CHUNK_SIZE = 256 * 1024;

class LocalGridFile {
public:
  LocalGridFile(uid_t u, gid_t g, mode_t m, int chunkSize = DEFAULT_CHUNK_SIZE) :
    _length(0),
    _chunkSize(chunkSize),
    _uid(u),
    _gid(g),
    _mode(m),
    _dirty(true)
  {
    _chunks.push_back(new char[_chunkSize]);
  }

  LocalGridFile(const LocalGridFile &other) = delete;
  LocalGridFile &operator=(const LocalGridFile &other) = delete;

  ~LocalGridFile() {
    for (auto i : _chunks) {
      delete i;
    }
  }

  int Length() const {
    return _length;
  }

  int ChunkSize() const {
    return _chunkSize;
  }

  int NumChunks() const {
    return _chunks.size();
  }

  char *Chunk(int n) const {
    return _chunks[n];
  }

  uid_t Uid() const {
    return _uid;
  }
  void setUid(uid_t u) {
    _uid = u;
  }

  gid_t Gid() const {
    return _gid;
  }
  void setGid(gid_t g) {
    _gid = g;
  }

  mode_t Mode() const {
    return _mode;
  }
  void setMode(mode_t m) {
    _mode = m;
  }

  bool is_dirty() const {
    return _dirty;
  }
  bool is_clean() const {
    return !_dirty;
  }

  void set_flushed() {
    _dirty = false;
  }

  int write(const char *buf, size_t nbyte, off_t offset);
  int read(char *buf, size_t size, off_t offset);

  typedef std::shared_ptr<LocalGridFile> ptr;

private:
  size_t _length, _chunkSize;
  uid_t _uid;
  gid_t _gid;
  mode_t _mode;

  bool _dirty;
  std::vector<char *> _chunks;
};

#endif
