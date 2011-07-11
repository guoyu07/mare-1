/**
* @file Directory.cpp
* Implementation of a class for accessing directories.
* @author Colin Graf
*/

#include <cassert>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Directory.h"
#include "List.h"
#include "Map.h"
#include "File.h"

Directory::Directory()
{
#ifdef _WIN32
  findFile = INVALID_HANDLE_VALUE;
  assert(sizeof(ffd) >= sizeof(WIN32_FIND_DATA));
#endif
}

Directory::~Directory()
{
#ifdef _WIN32
  if(findFile != INVALID_HANDLE_VALUE)
    FindClose((HANDLE)findFile);
#endif
}

bool Directory::remove(const String& dir)
{
  String path = dir;
  while(path != ".")
  {
#ifdef _WIN32
    if(!RemoveDirectory(path.getData()))
      return false;
#else
    if(rmdir(path.getData()) != 0)
      return false;
#endif
    path = File::getDirname(path);
  }
  return true;
}

bool Directory::open(const String& dirpath, const String& pattern)
{
#ifdef _WIN32
  if(findFile)
  {
    FindClose((HANDLE)findFile);
    findFile = INVALID_HANDLE_VALUE;
  }

  this->dirpath = dirpath;
  String searchPath = dirpath;
  searchPath.setCapacity(dirpath.getLength() + 1 + pattern.getLength());
  searchPath.append('/');
  searchPath.append(pattern);

  findFile = FindFirstFile(searchPath.getData(), (LPWIN32_FIND_DATA)ffd);
  if(findFile == INVALID_HANDLE_VALUE)
    return false;
  bufferedEntry = true;
  return true;
#endif
}

bool Directory::read(bool dirsOnly, String& name, bool& isDir)
{
#ifdef _WIN32
  if(!findFile)
    return false;
  for(;;)
  {
    if(bufferedEntry)
      bufferedEntry = false;
    else if(!FindNextFile((HANDLE)findFile, (LPWIN32_FIND_DATA)ffd))
    {
      FindClose((HANDLE)findFile);
      findFile = INVALID_HANDLE_VALUE;
      return false;
    }
    isDir = (((LPWIN32_FIND_DATA)ffd)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
    if(dirsOnly && !isDir)
      continue;
    const char* str = ((LPWIN32_FIND_DATA)ffd)->cFileName;
    if(isDir && *str == '.' && (str[1] == '\0' || (str[1] == '.' && str[2] == '\0')))
      continue;
    name = String(str, -1);
    return true;
  }
#endif
}

void Directory::findFiles(const String& pattern, List<String>& files)
{
  // replace ** with * / ** and split in chunks
  List<String> chunks;
  int start = 0, i = 0;
  for(const char* str = pattern.getData(), * end = str + pattern.getLength(); str <= end; ++str, ++i)
    switch(*str)
    {
    case '/':
    case '\\':
    case '\0':
      {
        int len = i - start;
        if(len > 0)
          chunks.append(pattern.substr(start, len));
        start = i + 1;
      }
      break;
    case '*':
      if(str + 1 < end && str[1] == '*')
      {
        chunks.append(pattern.substr(start, i - start + 1));
        start = i;
      }
      break;
    }

  //
  struct FindFiles
  {
    List<String>* files;

    void handleSubPath(const String& path, const String& name, bool isDir, const String& nextPattern, const List<String>::Node* nextNextChunk)
    {
      String subpath = path;
      subpath.setCapacity(path.getLength() + 1 + name.getLength());
      if(!path.isEmpty())
        subpath.append('/');
      subpath.append(name);
      if(nextPattern.isEmpty())
        files->append(subpath);
      else if(isDir)
        handlePath(subpath, nextPattern, nextNextChunk);
    }

    void handlePath(const String& path, const String& pattern, const List<String>::Node* nextChunk)
    {
      if(nextChunk && nextChunk->data.getLength() >= 2 && nextChunk->data.getData()[0] == '*' && nextChunk->data.getData()[1] == '*')
      {
        /*
        if a* / **b / c
        use a*b / c
        foreach a* use * / **b / c
        */

        const String& nextPattern = nextChunk->data;
        String testPattern = pattern;
        testPattern.setCapacity(pattern.getLength() + nextPattern.getLength() - 2);
        testPattern.append(nextPattern.getData() + 2, nextPattern.getLength() - 2);
        handlePath(path, testPattern, nextChunk->getNext());

        Directory dir; String name; bool isDir;
        if(dir.open(path, pattern))
          while(dir.read(true, name, isDir))
            handleSubPath(path, name, isDir, "*", nextChunk);
      }
      else if(strpbrk(pattern.getData(), "*?") || nextChunk == 0)
      {
        Directory dir; String name; bool isDir;
        if(dir.open(path, pattern))
          while(dir.read(nextChunk != 0, name, isDir))
            handleSubPath(path, name, isDir, nextChunk ? nextChunk->data : String(), nextChunk ? nextChunk->getNext() : 0);
      }
      else // not a pattern
        handleSubPath(path, pattern, true, nextChunk->data, nextChunk->getNext());
    }
  };

  if(!chunks.isEmpty())
  {
    FindFiles ff;
    ff.files = &files;
    ff.handlePath(String(), chunks.getFirst()->data, chunks.getFirst()->getNext());
  }
}

bool Directory::exists(const String& dir)
{
#ifdef _WIN32
  WIN32_FIND_DATAA wfd;
  HANDLE hFind = FindFirstFileA(dir.getData(), &wfd);
  if(hFind == INVALID_HANDLE_VALUE) 
    return false;
  bool isDir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
  FindClose(hFind);
  return isDir;
#else
  struct stat buf;
  if(stat(dir.getData(), &buf) != 0)
    return false;
  return S_ISDIR(buf.st_mode);
#endif
}

bool Directory::create(const String& dir)
{
  static Map<String, bool> createdDirs;
  Map<String, bool>::Node* i  = createdDirs.find(dir);
  if(i)
    return i->data;

  if(exists(dir))
  {
    createdDirs.append(dir, true);
    return true;
  }
  
  
  const char* start = dir.getData();
  const char* pos = &start[dir.getLength() - 1];
  for(; pos >= start; --pos)
    if(*pos == '\\' || *pos == '/')
    {
      if(!create(dir.substr(0, pos - start)))
      {
        createdDirs.append(dir, false);
        return false;
      }
      break;
    }
  ++pos;
  bool result = false;
  if(*pos)
#ifdef _WIN32
    result = CreateDirectory(dir.getData(), NULL) == TRUE;
#else
    result = mkdir(dir.getData(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0;
#endif
  createdDirs.append(dir, result);
  return result;
}