#pragma once
#include <SdFat.h>

#include <iostream>

namespace serialization {
template <typename T>
static void writePod(std::ostream& os, const T& value) {
  os.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
static bool writePod(FsFile& file, const T& value) {
  return file.write(reinterpret_cast<const uint8_t*>(&value), sizeof(T)) == sizeof(T);
}

template <typename T>
static void readPod(std::istream& is, T& value) {
  is.read(reinterpret_cast<char*>(&value), sizeof(T));
}

template <typename T>
static bool readPod(FsFile& file, T& value) {
  return file.read(reinterpret_cast<uint8_t*>(&value), sizeof(T)) == sizeof(T);
}

static void writeString(std::ostream& os, const std::string& s) {
  const uint32_t len = s.size();
  writePod(os, len);
  os.write(s.data(), len);
}

static bool writeString(FsFile& file, const std::string& s) {
  const uint32_t len = s.size();
  if (!writePod(file, len)) return false;
  return file.write(reinterpret_cast<const uint8_t*>(s.data()), len) == len;
}

static void readString(std::istream& is, std::string& s) {
  uint32_t len;
  readPod(is, len);
  s.resize(len);
  is.read(&s[0], len);
}

static bool readString(FsFile& file, std::string& s) {
  uint32_t len;
  if (!readPod(file, len)) return false;
  if (len > 65536) return false; // Sanity check: max 64KB strings
  s.resize(len);
  return file.read(&s[0], len) == len;
}
}  // namespace serialization