#include "local_gridfile.h"

#include <algorithm>

using namespace std;

int LocalGridFile::write(const char *buf, size_t nbyte, off_t offset) {
  size_t last_chunk = (offset + nbyte) / _chunkSize;
  size_t written = 0;

  while (last_chunk > _chunks.size() - 1) {
    char *new_buf = new char[_chunkSize];
    memset(new_buf, 0, _chunkSize);
    _chunks.push_back(new_buf);
  }

  off_t chunk_num = offset / _chunkSize;
  off_t buf_offset = offset % _chunkSize;

  char *dest_buf = _chunks[chunk_num];
  if (buf_offset) {
    dest_buf += offset % _chunkSize;
    size_t to_write = min<size_t>(nbyte - written, _chunkSize - buf_offset);
    memcpy(dest_buf, buf, to_write);
    written += to_write;
    chunk_num++;
  }

  while (written < nbyte) {
    dest_buf = _chunks[chunk_num];
    size_t to_write = min<size_t>(nbyte - written, _chunkSize);
    memcpy(dest_buf, buf, to_write);
    written += to_write;
    chunk_num++;
  }

  _length = max(_length, offset + written);
  _dirty = true;

  return written;
}

int LocalGridFile::read(char *buf, size_t size, off_t offset) {
  size_t len = 0;
  size_t chunk_num = offset / _chunkSize;

  while (len < size && chunk_num < _chunks.size()) {
    const char *chunk = _chunks[chunk_num];
    size_t to_read = min<size_t>((size_t)_chunkSize, size - len);

    if (!len && offset) {
      chunk += offset % _chunkSize;
      to_read = min<size_t>(to_read, (size_t)(_chunkSize - (offset % _chunkSize)));
    }

    memcpy(buf + len, chunk, to_read);
    len += to_read;
    chunk_num++;
  }

  return len;
}
