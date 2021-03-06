#include "./database.hh"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <zlib.h>
#include <boost/filesystem/operations.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

namespace fs = std::filesystem;
namespace io = boost::iostreams;

namespace git {

using std::basic_stringstream;
using std::ios;
using std::stringstream;

void Database::Store(Object& object) const {
  const auto& odata = object.data();

  stringstream stream;

  auto type = object.type();
  stream.write(type.c_str(), type.size());

  string space = string(" ");
  stream.write(space.c_str(), space.size());

  string size = std::to_string(object.data().size());
  stream.write(size.c_str(), size.size());

  char nil = 0;
  stream.write(&nil, 1);
  stream.write(reinterpret_cast<const char*>(odata.data()), odata.size());
  stream.flush();

  stream.seekg(0, ios::end);

  boost::uuids::detail::sha1 sha1;
  sha1.process_bytes(stream.str().c_str(), stream.tellg());

  object.set_oid(sha1);

  writeObject(object.hex(), stream, stream.tellg());

  if (type != "blob")
    delete odata.data();
}

void Database::writeObject(const string& hex, stringstream& content,
                           size_t size) const {
  using std::ofstream;

  auto dname = fs::path(pathname_) / hex.substr(0, 2);
  auto fname = fs::path(pathname_) / hex.substr(0, 2) / hex.substr(2);

  fs::create_directories(dname);

  std::cout << fname << std::endl;

  z_stream stream;
  uLong compressed_size;
  int ret;
  int fd;

  /* allocate deflate state */
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
  compressed_size = deflateBound(&stream, size);
  auto compressed = new unsigned char[compressed_size];

  /* Compress it */
  stream.next_out = compressed;
  stream.avail_out = compressed_size;

  /* First header.. */
  auto buffer = content.str().c_str();
  stream.next_in = const_cast<unsigned char*>(
      reinterpret_cast<const unsigned char*>(buffer));
  stream.avail_in = size;
  while (deflate(&stream, Z_FINISH) == Z_OK) /* nothing */
    ;
  deflateEnd(&stream);
  compressed_size = stream.total_out;

  auto tmpname = fs::path(dname) / boost::filesystem::unique_path().string();

  ofstream object_file;
  object_file.open(tmpname);
  object_file.write(reinterpret_cast<char*>(compressed), compressed_size);
  object_file.close();

  fs::rename(tmpname, fname);
}

}  // namespace git
