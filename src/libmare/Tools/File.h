
#pragma once

#include "String.h"

class File
{
public:
  enum Flags
  {
    readFlag = 0x0001,
    writeFlag = 0x0002,
  };

  File();
  ~File();

  bool open(const String& file, Flags flags = readFlag);
  void close();
  int read(char* buffer, int len);
  int write(const char* buffer, int len);
  bool write(const String& data);

  static String getDirname(const String& file);
  static String getBasename(const String& file);
  static String getExtension(const String& file);
  static String getWithoutExtension(const String& file);
  static String simplifyPath(const String& path);

  static bool getWriteTime(const String& file, long long& writeTime);

  static bool exists(const String& file);
  static bool unlink(const String& file);

private:
  void* fp;
};
