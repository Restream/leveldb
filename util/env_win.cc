// This file contains source that originates from:
// http://code.google.com/p/leveldbwin/source/browse/trunk/win32_impl_src/env_win32.h
// http://code.google.com/p/leveldbwin/source/browse/trunk/win32_impl_src/port_win32.cc
// Those files don't have any explicit license headers but the
// project (http://code.google.com/p/leveldbwin/) lists the 'New BSD License'
// as the license.
#include <map>

#include "leveldb/env.h"

#include "leveldb/slice.h"
#include "port/port.h"
#include "util/logging.h"

#include <errno.h>
#include <io.h>
#include <process.h>
#include <shlwapi.h>
#include <stdio.h>
#include <algorithm>
#include <cstring>

#ifndef va_copy
#define va_copy(d, s) ((d) = (s))
#endif

#if defined DeleteFile
#undef DeleteFile
#endif

// Declarations
namespace leveldb {

namespace Win32 {

std::string GetCurrentDir();
std::wstring GetCurrentDirW();

static const std::string CurrentDir = GetCurrentDir();
static const std::wstring CurrentDirW = GetCurrentDirW();

std::string& ModifyPath(std::string& path);
std::wstring& ModifyPath(std::wstring& path);

std::string GetLastErrSz();
std::wstring GetLastErrSzW();

size_t GetPageSize();

typedef void (*ScheduleProc)(void*);

struct WorkItemWrapper {
  WorkItemWrapper(ScheduleProc proc_, void* content_);
  ScheduleProc proc;
  void* pContent;
};

DWORD WINAPI WorkItemWrapperProc(LPVOID pContent);

class Win32SequentialFile : public SequentialFile {
 public:
  friend class Win32Env;
  ~Win32SequentialFile();
  Status Read(size_t n, Slice* result, char* scratch) override;
  Status Skip(uint64_t n) override;
  BOOL isEnable();

 private:
  BOOL _Init();
  void _CleanUp();
  Win32SequentialFile(const std::string& fname);
  std::string filename_;
  ::HANDLE hFile_;
};

class Win32RandomAccessFile : public RandomAccessFile {
 public:
  friend class Win32Env;
  ~Win32RandomAccessFile();
  Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const override;
  BOOL isEnable();

 private:
  BOOL _Init(LPCWSTR path);
  void _CleanUp();
  Win32RandomAccessFile(const std::string& fname);
  HANDLE hFile_;
  const std::string filename_;
};

class Win32WritableFile : public WritableFile {
 public:
  Win32WritableFile(const std::string& fname, bool append);
  ~Win32WritableFile();

  Status Append(const Slice& data) override;
  Status Close() override;
  Status Flush() override;
  Status Sync() override;
  BOOL isEnable();

 private:
  std::string filename_;
  ::HANDLE hFile_;
};

class Win32FileLock : public FileLock {
 public:
  friend class Win32Env;

  ~Win32FileLock();
  BOOL isEnable();

 private:
  BOOL _Init(LPCWSTR path);
  void _CleanUp();
  Win32FileLock(const std::string& fname);
  HANDLE hFile_;
  std::string filename_;
};

class Win32Logger : public Logger {
 public:
  friend class Win32Env;
  ~Win32Logger();
  void Logv(const char* format, va_list ap) override;

 private:
  explicit Win32Logger(WritableFile* pFile);
  WritableFile* _pFileProxy;
};

class Win32Env : public Env {
 public:
  Win32Env();
  ~Win32Env();
  Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) override;
  Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) override;
  Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) override;
  Status NewAppendableFile(const std::string& fname,
                                   WritableFile** result) override;
  bool FileExists(const std::string& fname) override;
  Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) override;
  Status DeleteFile(const std::string& fname) override;
  Status CreateDir(const std::string& dirname) override;
  Status DeleteDir(const std::string& dirname) override;
  Status GetFileSize(const std::string& fname, uint64_t* file_size) override;
  Status RenameFile(const std::string& src, const std::string& target) override;
  Status LockFile(const std::string& fname, FileLock** lock) override;
  Status UnlockFile(FileLock* lock) override;
  void Schedule(void (*function)(void* arg), void* arg) override;
  void StartThread(void (*function)(void* arg), void* arg) override;
  Status GetTestDirectory(std::string* path) override;
  Status NewLogger(const std::string& fname, Logger** result) override;
  uint64_t NowMicros() override;
  void SleepForMicroseconds(int micros) override;
};

void ToWidePath(const std::string& value, std::wstring& target) {
  wchar_t buffer[MAX_PATH];
  MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, buffer, MAX_PATH);
  target = buffer;
}

void ToNarrowPath(const std::wstring& value, std::string& target) {
  char buffer[MAX_PATH];
  WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, buffer, MAX_PATH, NULL,
                      NULL);
  target = buffer;
}

std::string GetCurrentDir() {
  CHAR path[MAX_PATH];
  ::GetModuleFileNameA(::GetModuleHandleA(NULL), path, MAX_PATH);
  *strrchr(path, '\\') = 0;
  return std::string(path);
}

std::wstring GetCurrentDirW() {
  WCHAR path[MAX_PATH];
  ::GetModuleFileNameW(::GetModuleHandleW(NULL), path, MAX_PATH);
  *wcsrchr(path, L'\\') = 0;
  return std::wstring(path);
}

std::string& ModifyPath(std::string& path) {
  if (path[0] == '/' || path[0] == '\\') {
    path = CurrentDir + path;
  }
  std::replace(path.begin(), path.end(), '/', '\\');

  return path;
}

std::wstring& ModifyPath(std::wstring& path) {
  if (path[0] == L'/' || path[0] == L'\\') {
    path = CurrentDirW + path;
  }
  std::replace(path.begin(), path.end(), L'/', L'\\');
  return path;
}

std::string GetLastErrSz() {
  LPWSTR lpMsgBuf;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, GetLastError(),
                 0,  // Default language
                 (LPWSTR)&lpMsgBuf, 0, NULL);
  std::string Err;
  ToNarrowPath(lpMsgBuf, Err);
  LocalFree(lpMsgBuf);
  return Err;
}

std::wstring GetLastErrSzW() {
  LPVOID lpMsgBuf;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, GetLastError(),
                 0,  // Default language
                 (LPWSTR)&lpMsgBuf, 0, NULL);
  std::wstring Err = (LPCWSTR)lpMsgBuf;
  LocalFree(lpMsgBuf);
  return Err;
}

static Status Win32Error(const std::string& context) {
  if (GetLastError() == ERROR_FILE_NOT_FOUND) {
    return Status::NotFound(context, GetLastErrSz());
  } else {
    return Status::IOError(context, GetLastErrSz());
  }
}

WorkItemWrapper::WorkItemWrapper(ScheduleProc proc_, void* content_)
    : proc(proc_), pContent(content_) {}

DWORD WINAPI WorkItemWrapperProc(LPVOID pContent) {
  WorkItemWrapper* item = static_cast<WorkItemWrapper*>(pContent);
  ScheduleProc TempProc = item->proc;
  void* arg = item->pContent;
  delete item;
  TempProc(arg);
  return 0;
}

size_t GetPageSize() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return std::max(si.dwPageSize, si.dwAllocationGranularity);
}

const size_t g_PageSize = GetPageSize();

Win32SequentialFile::Win32SequentialFile(const std::string& fname)
    : filename_(fname), hFile_(NULL) {
  _Init();
}

Win32SequentialFile::~Win32SequentialFile() { _CleanUp(); }

Status Win32SequentialFile::Read(size_t n, Slice* result, char* scratch) {
  Status sRet;
  DWORD hasRead = 0;
  if (hFile_ && ReadFile(hFile_, scratch, n, &hasRead, NULL)) {
    *result = Slice(scratch, hasRead);
  } else {
    sRet = Win32Error(filename_);
  }
  return sRet;
}

Status Win32SequentialFile::Skip(uint64_t n) {
  Status sRet;
  LARGE_INTEGER Move, NowPointer;
  Move.QuadPart = n;
  if (!SetFilePointerEx(hFile_, Move, &NowPointer, FILE_CURRENT)) {
    sRet = Win32Error(filename_);
  }
  return sRet;
}

BOOL Win32SequentialFile::isEnable() { return hFile_ ? TRUE : FALSE; }

BOOL Win32SequentialFile::_Init() {
  std::wstring path;
  ToWidePath(filename_, path);
  hFile_ = CreateFileW(path.c_str(), GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (hFile_ == INVALID_HANDLE_VALUE) hFile_ = NULL;
  return hFile_ ? TRUE : FALSE;
}

void Win32SequentialFile::_CleanUp() {
  if (hFile_) {
    CloseHandle(hFile_);
    hFile_ = NULL;
  }
}

Win32RandomAccessFile::Win32RandomAccessFile(const std::string& fname)
    : filename_(fname), hFile_(NULL) {
  std::wstring path;
  ToWidePath(fname, path);
  _Init(path.c_str());
}

Win32RandomAccessFile::~Win32RandomAccessFile() { _CleanUp(); }

Status Win32RandomAccessFile::Read(uint64_t offset, size_t n, Slice* result,
                                   char* scratch) const {
  Status sRet;
  OVERLAPPED ol = {0};
  ZeroMemory(&ol, sizeof(ol));
  ol.Offset = (DWORD)offset;
  ol.OffsetHigh = (DWORD)(offset >> 32);
  DWORD hasRead = 0;
  if (!ReadFile(hFile_, scratch, n, &hasRead, &ol))
    sRet = Win32Error(filename_);
  else
    *result = Slice(scratch, hasRead);
  return sRet;
}

BOOL Win32RandomAccessFile::_Init(LPCWSTR path) {
  BOOL bRet = FALSE;
  if (!hFile_)
    hFile_ = ::CreateFileW(
        path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
  if (!hFile_ || hFile_ == INVALID_HANDLE_VALUE)
    hFile_ = NULL;
  else
    bRet = TRUE;
  return bRet;
}

BOOL Win32RandomAccessFile::isEnable() { return hFile_ ? TRUE : FALSE; }

void Win32RandomAccessFile::_CleanUp() {
  if (hFile_) {
    ::CloseHandle(hFile_);
    hFile_ = NULL;
  }
}

Win32WritableFile::Win32WritableFile(const std::string& fname, bool append)
    : filename_(fname) {
  std::wstring path;
  ToWidePath(fname, path);
  // NewAppendableFile: append to an existing file, or create a new one
  //     if none exists - this is OPEN_ALWAYS behavior, with
  //     FILE_APPEND_DATA to avoid having to manually position the file
  //     pointer at the end of the file.
  // NewWritableFile: create a new file, delete if it exists - this is
  //     CREATE_ALWAYS behavior. This file is used for writing only so
  //     use GENERIC_WRITE.
  hFile_ = CreateFileW(path.c_str(), append ? FILE_APPEND_DATA : GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE,
                       NULL, append ? OPEN_ALWAYS : CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
  // CreateFileW returns INVALID_HANDLE_VALUE in case of error, always check
  // isEnable() before use
}

Win32WritableFile::~Win32WritableFile() {
  if (hFile_ != INVALID_HANDLE_VALUE) {
    Close();
  }
}

Status Win32WritableFile::Append(const Slice& data) {
  DWORD r = 0;
  if (!WriteFile(hFile_, data.data(), data.size(), &r, NULL) ||
      r != data.size()) {
    return Win32Error("Win32WritableFile.Append::WriteFile: " + filename_);
  }
  return Status::OK();
}

Status Win32WritableFile::Close() {
  if (!CloseHandle(hFile_)) {
    return Win32Error("Win32WritableFile.Close::CloseHandle: " + filename_);
  }
  hFile_ = INVALID_HANDLE_VALUE;
  return Status::OK();
}

Status Win32WritableFile::Flush() {
  // Nothing to do here, there are no application-side buffers
  return Status::OK();
}

Status Win32WritableFile::Sync() {
  if (!FlushFileBuffers(hFile_)) {
    return Win32Error("Win32WritableFile.Sync::FlushFileBuffers " + filename_);
  }
  return Status::OK();
}

BOOL Win32WritableFile::isEnable() { return hFile_ != INVALID_HANDLE_VALUE; }

Win32FileLock::Win32FileLock(const std::string& fname)
    : hFile_(NULL), filename_(fname) {
  std::wstring path;
  ToWidePath(fname, path);
  _Init(path.c_str());
}

Win32FileLock::~Win32FileLock() { _CleanUp(); }

BOOL Win32FileLock::_Init(LPCWSTR path) {
  BOOL bRet = FALSE;
  if (!hFile_)
    hFile_ = ::CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (!hFile_ || hFile_ == INVALID_HANDLE_VALUE) {
    hFile_ = NULL;
  } else
    bRet = TRUE;
  return bRet;
}

void Win32FileLock::_CleanUp() {
  ::CloseHandle(hFile_);
  hFile_ = NULL;
}

BOOL Win32FileLock::isEnable() { return hFile_ ? TRUE : FALSE; }

Win32Logger::Win32Logger(WritableFile* pFile) : _pFileProxy(pFile) {
  assert(_pFileProxy);
}

Win32Logger::~Win32Logger() {
  if (_pFileProxy) delete _pFileProxy;
}

void Win32Logger::Logv(const char* format, va_list ap) {
  uint64_t thread_id = ::GetCurrentThreadId();

  // We try twice: the first time with a fixed-size stack allocated buffer,
  // and the second time with a much larger dynamically allocated buffer.
  char buffer[500];
  for (int iter = 0; iter < 2; iter++) {
    char* base;
    int bufsize;
    if (iter == 0) {
      bufsize = sizeof(buffer);
      base = buffer;
    } else {
      bufsize = 30000;
      base = new char[bufsize];
    }
    char* p = base;
    char* limit = base + bufsize;

    SYSTEMTIME st;
    GetLocalTime(&st);
    p += snprintf(p, limit - p, "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
                  int(st.wYear), int(st.wMonth), int(st.wDay), int(st.wHour),
                  int(st.wMinute), int(st.wMinute), int(st.wMilliseconds),
                  static_cast<long long unsigned int>(thread_id));

    // Print the message
    if (p < limit) {
      va_list backup_ap;
      va_copy(backup_ap, ap);
      p += vsnprintf(p, limit - p, format, backup_ap);
      va_end(backup_ap);
    }

    // Truncate to available space if necessary
    if (p >= limit) {
      if (iter == 0) {
        continue;  // Try again with larger buffer
      } else {
        p = limit - 1;
      }
    }

    // Add newline if necessary
    if (p == base || p[-1] != '\n') {
      *p++ = '\n';
    }

    assert(p <= limit);
    DWORD hasWritten = 0;
    if (_pFileProxy) {
      _pFileProxy->Append(Slice(base, p - base));
      _pFileProxy->Flush();
    }
    if (base != buffer) {
      delete[] base;
    }
    break;
  }
}

bool Win32Env::FileExists(const std::string& fname) {
  std::string path = fname;
  std::wstring wpath;
  ToWidePath(ModifyPath(path), wpath);
  return ::PathFileExistsW(wpath.c_str()) ? true : false;
}

Status Win32Env::GetChildren(const std::string& dir,
                             std::vector<std::string>* result) {
  Status sRet;
  ::WIN32_FIND_DATAW wfd;
  std::string path = dir;
  ModifyPath(path);
  path += "\\*.*";
  std::wstring wpath;
  ToWidePath(path, wpath);

  ::HANDLE hFind = ::FindFirstFileW(wpath.c_str(), &wfd);
  if (hFind && hFind != INVALID_HANDLE_VALUE) {
    BOOL hasNext = TRUE;
    std::string child;
    while (hasNext) {
      ToNarrowPath(wfd.cFileName, child);
      result->push_back(child);
      hasNext = ::FindNextFileW(hFind, &wfd);
    }
    ::FindClose(hFind);
  } else
    sRet = Status::IOError(dir, "Could not get children.");
  return sRet;
}

void Win32Env::SleepForMicroseconds(int micros) {
  ::Sleep((micros + 999) / 1000);
}

Status Win32Env::DeleteFile(const std::string& fname) {
  Status sRet;
  std::string path = fname;
  std::wstring wpath;
  ToWidePath(ModifyPath(path), wpath);

  if (!::DeleteFileW(wpath.c_str())) {
    sRet = Win32Error(path);
  }
  return sRet;
}

Status Win32Env::GetFileSize(const std::string& fname, uint64_t* file_size) {
  Status sRet;
  std::string path = fname;
  std::wstring wpath;
  ToWidePath(ModifyPath(path), wpath);

  HANDLE file = ::CreateFileW(wpath.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  LARGE_INTEGER li;
  if (::GetFileSizeEx(file, &li)) {
    *file_size = (uint64_t)li.QuadPart;
  } else
    sRet = Win32Error(path + " Could not get the file size.");
  CloseHandle(file);
  return sRet;
}

Status Win32Env::RenameFile(const std::string& src, const std::string& target) {
  Status sRet;
  std::string src_path = src;
  std::wstring wsrc_path;
  ToWidePath(ModifyPath(src_path), wsrc_path);
  std::string target_path = target;
  std::wstring wtarget_path;
  ToWidePath(ModifyPath(target_path), wtarget_path);

  if (!MoveFileW(wsrc_path.c_str(), wtarget_path.c_str())) {
    DWORD err = GetLastError();
    if (err == 0x000000b7) {
      if (!::DeleteFileW(wtarget_path.c_str()))
        sRet = Win32Error(src + "Could not rename file.");
      else if (!::MoveFileW(wsrc_path.c_str(), wtarget_path.c_str()))
        sRet = Win32Error(src + "Could not rename file.");
    }
  }
  return sRet;
}

Status Win32Env::LockFile(const std::string& fname, FileLock** lock) {
  Status sRet;
  std::string path = fname;
  ModifyPath(path);
  Win32FileLock* _lock = new Win32FileLock(path);
  if (!_lock->isEnable()) {
    delete _lock;
    *lock = NULL;
    sRet = Status::IOError(path, "Could not lock file.");
  } else
    *lock = _lock;
  return sRet;
}

Status Win32Env::UnlockFile(FileLock* lock) {
  Status sRet;
  delete lock;
  return sRet;
}

void Win32Env::Schedule(void (*function)(void* arg), void* arg) {
  QueueUserWorkItem(Win32::WorkItemWrapperProc,
                    new Win32::WorkItemWrapper(function, arg),
                    WT_EXECUTEDEFAULT);
}

void Win32Env::StartThread(void (*function)(void* arg), void* arg) {
  ::_beginthread(function, 0, arg);
}

Status Win32Env::GetTestDirectory(std::string* path) {
  Status sRet;
  WCHAR TempPath[MAX_PATH];
  ::GetTempPathW(MAX_PATH, TempPath);
  ToNarrowPath(TempPath, *path);
  path->append("leveldb\\test");
  ModifyPath(*path);
  return sRet;
}

uint64_t Win32Env::NowMicros() {
#ifndef USE_VISTA_API
#define GetTickCount64 GetTickCount
#endif
  return (uint64_t)(GetTickCount64() * 1000);
}

static Status CreateDirInner(const std::string& dirname) {
  Status sRet;
  DWORD attr = ::GetFileAttributes(dirname.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES) {  // doesn't exist:
    std::size_t slash = dirname.find_last_of("\\");
    if (slash != std::string::npos) {
      sRet = CreateDirInner(dirname.substr(0, slash));
      if (!sRet.ok()) return sRet;
    }
    BOOL result = ::CreateDirectory(dirname.c_str(), NULL);
    if (result == FALSE && GetLastError() != ERROR_ALREADY_EXISTS) {
      sRet = Win32Error(dirname + ":Could not create directory.");
      return sRet;
    }
  }
  return sRet;
}

Status Win32Env::CreateDir(const std::string& dirname) {
  std::string path = dirname;
  if (path[path.length() - 1] != '\\') {
    path += '\\';
  }
  ModifyPath(path);

  return CreateDirInner(path);
}

Status Win32Env::DeleteDir(const std::string& dirname) {
  Status sRet;
  std::wstring path;
  ToWidePath(dirname, path);
  ModifyPath(path);
  if (!::RemoveDirectoryW(path.c_str())) {
    sRet = Win32Error(dirname + ":Could not delete directory.");
  }
  return sRet;
}

Status Win32Env::NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) {
  Status sRet;
  std::string path = fname;
  ModifyPath(path);
  Win32SequentialFile* pFile = new Win32SequentialFile(path);
  if (pFile->isEnable()) {
    *result = pFile;
  } else {
    delete pFile;
    sRet = Win32Error(path);
  }
  return sRet;
}

Status Win32Env::NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) {
  Status sRet;
  std::string path = fname;
  Win32RandomAccessFile* pFile = new Win32RandomAccessFile(ModifyPath(path));
  if (!pFile->isEnable()) {
    delete pFile;
    *result = NULL;
    sRet = Win32Error(path);
  } else
    *result = pFile;
  return sRet;
}

Status Win32Env::NewLogger(const std::string& fname, Logger** result) {
  Status sRet;
  std::string path = fname;
  // Logs are opened with write semantics, not with append semantics
  // (see PosixEnv::NewLogger)
  Win32WritableFile* pMapFile = new Win32WritableFile(ModifyPath(path), false);
  if (!pMapFile->isEnable()) {
    delete pMapFile;
    *result = NULL;
    sRet = Status::IOError(path, "could not create a logger.");
  } else
    *result = new Win32Logger(pMapFile);
  return sRet;
}

Status Win32Env::NewWritableFile(const std::string& fname,
                                 WritableFile** result) {
  Status sRet;
  std::string path = fname;
  Win32WritableFile* pFile = new Win32WritableFile(ModifyPath(path), false);
  if (!pFile->isEnable()) {
    *result = NULL;
    sRet = Win32Error(path);
  } else
    *result = pFile;
  return sRet;
}

Status Win32Env::NewAppendableFile(const std::string& fname,
                                   WritableFile** result) {
  Status sRet;
  std::string path = fname;
  Win32WritableFile* pFile = new Win32WritableFile(ModifyPath(path), true);
  if (!pFile->isEnable()) {
    *result = NULL;
    sRet = Win32Error(path);
  } else
    *result = pFile;
  return sRet;
}

Win32Env::Win32Env() {}

Win32Env::~Win32Env() {}

}  // namespace Win32

static port::OnceType once = LEVELDB_ONCE_INIT;
static Env* default_env;
static void InitDefaultEnv() { default_env = new Win32::Win32Env(); }

Env* Env::Default() {
  port::InitOnce(&once, InitDefaultEnv);
  return default_env;
}

}  // namespace leveldb
