#include "buffio/config.hpp"
#include "buffio/fs.hpp"
#include "buffio/flags.hpp"
#include "buffio/worker.hpp"
#include "buffio/ecode.hpp"
#include <windows.h>

int buffio::OpenFileAwaiter::await_resume(){ 
  if(op_state.fd == BUFFIO_FD_INVALID) return op_state.error;

  buffio::File *file = 
       static_cast<buffio::File*>(rval); 

  file->fd = op_state.fd;
  file->flags = 0;
  return 0;
};
bool buffio::File::OpenStdIn()
{
  assert(fd == BUFFIO_FD_INVALID);
  buffio_fd tmp_fd = GetStdHandle(STD_INPUT_HANDLE);
  if (tmp_fd == INVALID_HANDLE_VALUE)
    return false;
  fd = tmp_fd;
  return true;
};

bool buffio::File::OpenStdOut()
{
  assert(fd == BUFFIO_FD_INVALID);
  buffio_fd tmp_fd = GetStdHandle(STD_OUTPUT_HANDLE);
  if (tmp_fd == INVALID_HANDLE_VALUE)
    return false;

  fd = tmp_fd;
  return true;
};

bool buffio::OpenFileAwaiter::action(std::pair<void *, void *> info)
{
  auto [p_sqe, p_self] = info;
  buffio::OpenFileAwaiter *obj =
      static_cast<buffio::OpenFileAwaiter *>(p_self);

  int rwflags = obj->flags;

  DWORD desiredAccess = 0;
  DWORD dispositionFlag = 0;
  
  //TODO: map modes to windows specific api
  
  if (rwflags & B_RDONLY)
    desiredAccess |= GENERIC_READ;

  if (rwflags & B_WRONLY)
    desiredAccess |= GENERIC_WRITE;

  if (rwflags & B_APPEND)
  {
    desiredAccess &= ~GENERIC_WRITE;
    desiredAccess |= FILE_APPEND_DATA;
  }

  dispositionFlag = (rwflags & B_CREAT && rwflags & B_EXCL) ? CREATE_NEW : OPEN_EXISTING;
  dispositionFlag = (rwflags & B_CREAT && rwflags & B_TRUNC) ? CREATE_ALWAYS : dispositionFlag;
  dispositionFlag = (rwflags & B_TRUNC && !(rwflags & B_CREAT)) ? TRUNCATE_EXISTING : dispositionFlag;
  dispositionFlag = (rwflags & B_CREAT && !(rwflags & B_EXCL)) ? OPEN_ALWAYS : dispositionFlag;

  // TODO Add support for tmp_File
  DWORD shareMode = 0;
  DWORD flagsAttr = FILE_ATTRIBUTE_NORMAL;
  flagsAttr |= rwflags & B_NONBLOCK ? FILE_FLAG_OVERLAPPED : 0;

  // create directory only if the desiredAccess mode results in OPEN_EXISTING
  // else allow the call to fail
  if (rwflags & B_DIRECTORY && desiredAccess == OPEN_EXISTING)
  {
    flagsAttr = FILE_FLAG_BACKUP_SEMANTICS;
    shareMode |= FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  };

  HANDLE fd = CreateFileW(
      (LPCWSTR)obj->path.c_str(),
      desiredAccess,
      shareMode,
      NULL,
      dispositionFlag,
      flagsAttr,
      INVALID_HANDLE_VALUE);

  if (fd == INVALID_HANDLE_VALUE)
  {
    obj->op_state.error = buffio::error_from_os(GetLastError());
    obj->op_state.fd = BUFFIO_FD_INVALID;
    return true;
  };

  obj->op_state.fd = fd;
  return true;
};

bool buffio::AwaitableFileBase::action(std::pair<void *, void *> info)
{

  auto [p_sqe, p_self] = info;
  buffio::AwaitableFileBase *obj =
      static_cast<buffio::AwaitableFileBase *>(p_self);

  auto [fd, buffer, size, offset] = obj->state;

  WINBOOL rval = false;
  ssize_t bytesNum = 0;
  DWORD bytesDone = 0; // platform typed data to store bytes read/write;
  obj->op_state.error = 0;


  switch (obj->op_state.op_code)
  {

  case OpCode::Read:
  {
    rval = ReadFile(
        fd,
        static_cast<LPVOID>(buffer),
        static_cast<DWORD>(size),
        &bytesDone,
        nullptr);

    bytesNum = bytesDone;
    break;
  }
  case OpCode::Write:
  {
    rval = WriteFile(
        fd,
        static_cast<LPVOID>(buffer),
        static_cast<DWORD>(size),
        &bytesDone,
        NULL);

    bytesNum = bytesDone;
    break;
  }

  case OpCode::Readv:
  {
    Bufferiov *buf = (Bufferiov *)buffer;
    for (int i = 0; i < size; i++)
    {
      rval = ReadFile(
          fd,
          static_cast<LPVOID>(buf[i].buffer),
          static_cast<DWORD>(buf[i].size),
          &bytesDone,
          NULL);
      if (!rval)
        break;
      bytesNum += static_cast<ssize_t>(bytesDone);
    };
    break;
  }
  case OpCode::Writev:
  {
    Bufferiov *buf = (Bufferiov *)buffer;
    for (int i = 0; i < size; i++)
    {
      rval = WriteFile(
          fd,
          static_cast<LPVOID>(buf[i].buffer),
          static_cast<DWORD>(buf[i].size),
          &bytesDone,
          NULL);
      if (!rval)
        break;
      bytesNum += static_cast<ssize_t>(bytesDone);
    };
    break;
  }
  case OpCode::pRead:
  {
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(offset);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);

    rval = ReadFile(
        fd,
        static_cast<LPVOID>(buffer),
        static_cast<DWORD>(size),
        &bytesDone,
        &ov);

    bytesNum = bytesDone;
    break;
  }

  case OpCode::pWrite:
  {
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(offset);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);

    rval = WriteFile(
        fd,
        static_cast<LPVOID>(buffer),
        static_cast<DWORD>(size),
        &bytesDone,
        &ov);

    bytesNum = bytesDone;
    break;
  }
  case OpCode::pReadv:
  {
    OVERLAPPED ov{};

    Bufferiov *buf = (Bufferiov *)buffer;
    for (int i = 0; i < size; i++)
    {
      ov.Offset = static_cast<DWORD>(offset);
      ov.OffsetHigh = static_cast<DWORD>(offset >> 32);

      rval = ReadFile(
          fd,
          static_cast<LPVOID>(buf[i].buffer),
          static_cast<DWORD>(buf[i].size),
          &bytesDone,
          &ov);
      if (!rval)
        break;
      bytesNum += static_cast<ssize_t>(bytesDone);
      offset += static_cast<uint64_t>(bytesDone);
    };

    break;
  }
  case OpCode::pWritev:
  {
    OVERLAPPED ov{};

    Bufferiov *buf = (Bufferiov *)buffer;
    for (int i = 0; i < size; i++)
    {
      ov.Offset = static_cast<DWORD>(offset);
      ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
      rval = WriteFile(
          fd,
          static_cast<LPVOID>(buf[i].buffer),
          static_cast<DWORD>(buf[i].size),
          &bytesDone,
          &ov);
      if (!rval)
        break;
      bytesNum += static_cast<ssize_t>(bytesDone);
      offset += static_cast<uint64_t>(bytesDone);
    };

    break;
  }

  default:
    rval = false;
    break;
  };

  if (!rval)
    obj->op_state.error = buffio::error_from_os(GetLastError());

  obj->op_state.op_done = static_cast<ssize_t>(bytesNum);
  return true;
};
