
#include <cstring>
#include <cassert>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "File.h"
#include "String.h"

File::File()
{
#ifdef _WIN32
  assert(sizeof(void*) >= sizeof(HANDLE));
  fp = INVALID_HANDLE_VALUE;
#else
  fp = 0;
#endif
}

File::~File()
{
  close();
}

void File::close()
{
#ifdef _WIN32
  if(fp != INVALID_HANDLE_VALUE)
  {
    CloseHandle((HANDLE)fp);
    fp = INVALID_HANDLE_VALUE;
  }
#else
  if(fp)
  {
    fclose((FILE*)fp);
    fp = 0;
  }
#endif
}

bool File::open(const String& file, Flags flags)
{
#ifdef _WIN32
  if(fp != INVALID_HANDLE_VALUE)
    return false;
  DWORD desiredAccess = 0, creationDisposition = 0;
  if(flags & writeFlag)
  {
    desiredAccess |= GENERIC_WRITE;
    creationDisposition |= CREATE_ALWAYS;
  }
  if(flags & readFlag)
  {
    desiredAccess |= GENERIC_READ;
    if(!(flags & writeFlag))
      creationDisposition |= OPEN_EXISTING;
  }
  fp = CreateFileA(file.getData(), desiredAccess, FILE_SHARE_READ, NULL, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
  if(fp == INVALID_HANDLE_VALUE)
  {
    error = GetLastError();
    return false;
  }
#else
  if(fp)
    return false;
  const char* mode = flags & (writeFlag | readFlag) == (writeFlag | readFlag) ? "w+" : (flags & writeFlag ? "w" : "r");
  fp = fopen(file.getData(), mode);
  if(!fp)
  {
    error = errno;
    return false;
  }
#endif

  return true;
}

int File::read(char* buffer, int len)
{
#ifdef _WIN32
  DWORD i;
  if(!ReadFile((HANDLE)fp, buffer, len, &i, NULL))
  {
    error = GetLastError();
    return 0;
  }
  return i;
#else
  size_t i = fread(buffer, 1, len, (FILE*)fp);
  if(i == 0)
  {
    error = errno;
    return 0;
  }
  return (int)i;
#endif
}

int File::write(const char* buffer, int len)
{
#ifdef _WIN32
  DWORD i;
  if(!WriteFile((HANDLE)fp, buffer, len, &i, NULL))
  {
    error = GetLastError();
    return 0;
  }
  if(i != len)
  {
    error = GetLastError();
    return i;
  }
  return i;
#else
  size_t i = fwrite(buffer, 1, len, (FILE*)fp);
  if(i != len)
  {
    error = errno;
    return (int)i;
  }
  return (int)i;
#endif
}

bool File::write(const String& data)
{
  return write(data.getData(), data.getLength()) == data.getLength();
}

String File::getDirname(const String& file)
{
  const char* start = file.getData();
  const char* pos = &start[file.getLength() - 1];
  for(; pos >= start; --pos)
    if(*pos == '\\' || *pos == '/')
      return file.substr(0, pos - start);
  return String(".");
}

String File::getBasename(const String& file)
{
  const char* start = file.getData();
  const char* pos = &start[file.getLength() - 1];
  for(; pos >= start; --pos)
    if(*pos == '\\' || *pos == '/')
      return file.substr(pos - start + 1);
  return file;
}

String File::getExtension(const String& file)
{
  const char* start = file.getData();
  const char* pos = &start[file.getLength() - 1];
  for(; pos >= start; --pos)
    if(*pos == '.')
      return file.substr(pos - start + 1);
    else if(*pos == '\\' || *pos == '/')
      return String();
  return String();
}

String File::getWithoutExtension(const String& file)
{
  const char* start = file.getData();
  const char* pos = &start[file.getLength() - 1];
  for(; pos >= start; --pos)
    if(*pos == '.')
      return file.substr(0, pos - start);
    else if(*pos == '\\' || *pos == '/')
      return String();
  return String();
}

bool File::getWriteTime(const String& file, long long& writeTime)
{
#ifdef _WIN32
  WIN32_FIND_DATAA wfd;
  HANDLE hFind = FindFirstFileA(file.getData(), &wfd);
  if(hFind == INVALID_HANDLE_VALUE) 
    return false;
  assert(sizeof(DWORD) == 4);
  writeTime = ((long long)wfd.ftLastWriteTime.dwHighDateTime) << 32LL | ((long long)wfd.ftLastWriteTime.dwLowDateTime);
  FindClose(hFind);
  return true;
#else
  struct stat buf;
  if(stat(file.getData(), &buf) != 0)
    return false;
  return buf.st_mtime;
#endif
}
