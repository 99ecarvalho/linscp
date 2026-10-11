// Win32 functions core calls for modules, processes, the environment, paths,
// string order, shell folders, MIME types, code pages, reparse points and
// shell deletion -- implemented for POSIX. See win32_on_posix.h for what each
// promises; the comments here are about how.
#include "win32_on_posix.h"
#include "win32_kernel_internal.h"
#include "secext.h"

#include <pwd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <vector>

extern char ** environ;

namespace {

// --- UTF-16 helpers (the layer has no dependency on the shim) ---------------

std::u16string Wide(const wchar_t * S)
{
  std::u16string R;
  for (; (S != nullptr) && (*S != 0); ++S)
  {
    R += static_cast<char16_t>(*S);
  }
  return R;
}

std::u16string FromUtf8(const std::string & S)
{
  std::u16string R;
  for (std::size_t I = 0; I < S.size();)
  {
    const unsigned char C = static_cast<unsigned char>(S[I]);
    char32_t CP;
    int Len;
    if (C < 0x80) { CP = C; Len = 1; }
    else if ((C >> 5) == 0x6) { CP = C & 0x1F; Len = 2; }
    else if ((C >> 4) == 0xE) { CP = C & 0x0F; Len = 3; }
    else if ((C >> 3) == 0x1E) { CP = C & 0x07; Len = 4; }
    else { CP = 0xFFFD; Len = 1; }
    for (int K = 1; K < Len; ++K)
    {
      if ((I + K >= S.size()) || ((static_cast<unsigned char>(S[I + K]) >> 6) != 0x2))
      {
        CP = 0xFFFD;
        Len = K;
        break;
      }
      CP = (CP << 6) | (static_cast<unsigned char>(S[I + K]) & 0x3F);
    }
    I += Len;
    if (CP >= 0x10000)
    {
      CP -= 0x10000;
      R += static_cast<char16_t>(0xD800 + (CP >> 10));
      R += static_cast<char16_t>(0xDC00 + (CP & 0x3FF));
    }
    else
    {
      R += static_cast<char16_t>(CP);
    }
  }
  return R;
}

std::string ToUtf8(const std::u16string & S)
{
  std::u16string Z = S;
  return linscp::WideToUtf8(reinterpret_cast<const wchar_t *>(Z.c_str()));
}

// Copies into a Win32-style output buffer. Returns the length written, or
// when it does not fit the size needed including the terminator.
DWORD CopyOut(const std::u16string & S, LPWSTR Dst, DWORD Size)
{
  const DWORD Needed = static_cast<DWORD>(S.size()) + 1;
  if ((Dst == nullptr) || (Size < Needed))
  {
    return Needed;
  }
  for (std::size_t I = 0; I < S.size(); ++I)
  {
    Dst[I] = static_cast<wchar_t>(S[I]);
  }
  Dst[S.size()] = 0;
  return Needed - 1;
}

char16_t Fold(char16_t C)
{
  return ((C >= u'A') && (C <= u'Z')) ? static_cast<char16_t>(C - u'A' + u'a') : C;
}

bool SameIgnoringCase(const std::u16string & A, const char * B)
{
  std::size_t I = 0;
  for (; (I < A.size()) && (B[I] != '\0'); ++I)
  {
    if (Fold(A[I]) != Fold(static_cast<char16_t>(static_cast<unsigned char>(B[I]))))
    {
      return false;
    }
  }
  return (I == A.size()) && (B[I] == '\0');
}

// --- modules -------------------------------------------------------------------

struct TModule
{
  const char * Name;
};
TModule Kernel32Module{"kernel32"};
TModule NtDllModule{"ntdll"};

TModule * KnownModule(LPCWSTR Name)
{
  std::u16string N = Wide(Name);
  const std::size_t Slash = N.find_last_of(u"\\/");
  if (Slash != std::u16string::npos)
  {
    N = N.substr(Slash + 1);
  }
  if ((N.size() > 4) && SameIgnoringCase(N.substr(N.size() - 4), ".dll"))
  {
    N = N.substr(0, N.size() - 4);
  }
  if (SameIgnoringCase(N, "kernel32"))
  {
    return &Kernel32Module;
  }
  if (SameIgnoringCase(N, "ntdll"))
  {
    return &NtDllModule;
  }
  return nullptr;
}

// --- processes ----------------------------------------------------------------

struct TSnapshot final : linscp::TKernelObject
{
  std::vector<PROCESSENTRY32W> Entries;
  std::size_t Next = 0;
  bool IsSignaled() const override { return false; }
};

struct TProcess final : linscp::TKernelObject
{
  DWORD Id = 0;
  bool IsSignaled() const override { return false; }
};

bool ReadProcessEntry(DWORD Pid, PROCESSENTRY32W & Entry)
{
  // /proc/<pid>/stat: "pid (comm) state ppid ..."; comm may hold spaces and
  // parentheses, so it ends at the last ')'.
  std::ifstream Stat("/proc/" + std::to_string(Pid) + "/stat");
  std::string Line;
  if (!std::getline(Stat, Line))
  {
    return false;
  }
  const std::size_t Open = Line.find('(');
  const std::size_t Close = Line.rfind(')');
  if ((Open == std::string::npos) || (Close == std::string::npos) || (Close < Open))
  {
    return false;
  }
  std::istringstream Rest(Line.substr(Close + 1));
  std::string State;
  unsigned long Parent = 0;
  Rest >> State >> Parent;
  std::memset(&Entry, 0, sizeof(Entry));
  Entry.dwSize = sizeof(Entry);
  Entry.th32ProcessID = Pid;
  Entry.th32ParentProcessID = static_cast<DWORD>(Parent);
  Entry.cntThreads = 1;
  CopyOut(FromUtf8(Line.substr(Open + 1, Close - Open - 1)), Entry.szExeFile, MAX_PATH);
  return true;
}

// --- environment ----------------------------------------------------------------

std::string Env(const char * Name)
{
  const char * V = getenv(Name);
  return (V != nullptr) ? std::string(V) : std::string();
}

std::string Home()
{
  const std::string H = Env("HOME");
  return H.empty() ? std::string("/") : H;
}

// An XDG user directory from user-dirs.dirs (XDG_DOCUMENTS_DIR="$HOME/..."),
// else $HOME/Fallback when that exists, else $HOME.
std::string UserDir(const char * Key, const char * Fallback)
{
  std::string Config = Env("XDG_CONFIG_HOME");
  if (Config.empty())
  {
    Config = Home() + "/.config";
  }
  std::ifstream Dirs(Config + "/user-dirs.dirs");
  std::string Line;
  const std::string Prefix = std::string(Key) + "=";
  while (std::getline(Dirs, Line))
  {
    if (Line.compare(0, Prefix.size(), Prefix) == 0)
    {
      std::string V = Line.substr(Prefix.size());
      if ((V.size() >= 2) && (V.front() == '"') && (V.back() == '"'))
      {
        V = V.substr(1, V.size() - 2);
      }
      if (V.compare(0, 5, "$HOME") == 0)
      {
        V = Home() + V.substr(5);
      }
      if (!V.empty() && (V[0] == '/'))
      {
        return V;
      }
    }
  }
  struct stat St{};
  const std::string Guess = Home() + "/" + Fallback;
  return ((stat(Guess.c_str(), &St) == 0) && S_ISDIR(St.st_mode)) ? Guess : Home();
}

// --- logical comparison --------------------------------------------------------

bool IsDigit(char16_t C)
{
  return (C >= u'0') && (C <= u'9');
}

// --- MIME -------------------------------------------------------------------------

// The shared-mime-info globs, "weight:type:pattern" per line; the
// highest-weighted "*.ext" pattern matching the name, case-insensitively.
std::string MimeForName(const std::string & Name)
{
  std::string Lower = Name;
  for (char & C : Lower)
  {
    if ((C >= 'A') && (C <= 'Z')) { C = static_cast<char>(C - 'A' + 'a'); }
  }
  std::string Best;
  int BestWeight = -1;
  std::size_t BestLength = 0;
  for (const char * Path : { "/usr/share/mime/globs2", "/usr/local/share/mime/globs2" })
  {
    std::ifstream Globs(Path);
    std::string Line;
    while (std::getline(Globs, Line))
    {
      if (Line.empty() || (Line[0] == '#'))
      {
        continue;
      }
      const std::size_t A = Line.find(':');
      const std::size_t B = (A == std::string::npos) ? A : Line.find(':', A + 1);
      if (B == std::string::npos)
      {
        continue;
      }
      std::string Pattern = Line.substr(B + 1);
      const std::size_t Flags = Pattern.find(':');
      if (Flags != std::string::npos)
      {
        Pattern = Pattern.substr(0, Flags);
      }
      if ((Pattern.size() < 2) || (Pattern[0] != '*') ||
          (Pattern.find_first_of("*?[", 1) != std::string::npos))
      {
        continue;
      }
      const std::string Suffix = Pattern.substr(1);
      if ((Lower.size() >= Suffix.size()) &&
          (Lower.compare(Lower.size() - Suffix.size(), Suffix.size(), Suffix) == 0))
      {
        const int Weight = std::atoi(Line.substr(0, A).c_str());
        if ((Weight > BestWeight) || ((Weight == BestWeight) && (Suffix.size() > BestLength)))
        {
          Best = Line.substr(A + 1, B - A - 1);
          BestWeight = Weight;
          BestLength = Suffix.size();
        }
      }
    }
  }
  return Best;
}

// --- shell deletion --------------------------------------------------------------

bool RemoveTree(const std::string & Path)
{
  struct stat St{};
  if (lstat(Path.c_str(), &St) != 0)
  {
    return false;
  }
  if (S_ISDIR(St.st_mode))
  {
    if (DIR * D = opendir(Path.c_str()))
    {
      while (dirent * E = readdir(D))
      {
        const std::string N = E->d_name;
        if ((N != ".") && (N != ".."))
        {
          if (!RemoveTree(Path + "/" + N))
          {
            closedir(D);
            return false;
          }
        }
      }
      closedir(D);
    }
    return rmdir(Path.c_str()) == 0;
  }
  return unlink(Path.c_str()) == 0;
}

// Percent-encoding of a path for a .trashinfo Path= line (RFC 2396, keeping /).
std::string TrashEncode(const std::string & Path)
{
  static const char Hex[] = "0123456789ABCDEF";
  std::string R;
  for (unsigned char C : Path)
  {
    if (((C >= 'A') && (C <= 'Z')) || ((C >= 'a') && (C <= 'z')) ||
        ((C >= '0') && (C <= '9')) || (std::strchr("/-_.!~*'()", C) != nullptr))
    {
      R += static_cast<char>(C);
    }
    else
    {
      R += '%';
      R += Hex[C >> 4];
      R += Hex[C & 15];
    }
  }
  return R;
}

// The freedesktop.org trash specification, home trash only: the file moves
// to Trash/files under a unique name, with a Trash/info/<name>.trashinfo
// recording where it came from and when. A file on another file system
// cannot be renamed into the home trash, and fails, as the spec allows.
bool MoveToTrash(const std::string & Path)
{
  std::string Data = Env("XDG_DATA_HOME");
  if (Data.empty())
  {
    Data = Home() + "/.local/share";
  }
  const std::string Trash = Data + "/Trash";
  mkdir(Data.c_str(), 0700);
  mkdir(Trash.c_str(), 0700);
  mkdir((Trash + "/files").c_str(), 0700);
  mkdir((Trash + "/info").c_str(), 0700);

  std::string Absolute = Path;
  if (Absolute.empty() || (Absolute[0] != '/'))
  {
    char Cwd[4096];
    if (getcwd(Cwd, sizeof(Cwd)) == nullptr)
    {
      return false;
    }
    Absolute = std::string(Cwd) + "/" + Absolute;
  }
  const std::size_t Slash = Absolute.find_last_of('/');
  const std::string Base = Absolute.substr(Slash + 1);
  for (int Attempt = 0; Attempt < 1000; ++Attempt)
  {
    const std::string Name = (Attempt == 0) ? Base : Base + "." + std::to_string(Attempt);
    const std::string Info = Trash + "/info/" + Name + ".trashinfo";
    // O_EXCL claims the name, as the specification requires.
    const int Fd = open(Info.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (Fd < 0)
    {
      if (errno == EEXIST)
      {
        continue;
      }
      return false;
    }
    char When[32];
    const std::time_t Now = std::time(nullptr);
    std::tm Local{};
    localtime_r(&Now, &Local);
    std::strftime(When, sizeof(When), "%Y-%m-%dT%H:%M:%S", &Local);
    const std::string Text =
      "[Trash Info]\nPath=" + TrashEncode(Absolute) + "\nDeletionDate=" + When + "\n";
    const bool Written = write(Fd, Text.data(), Text.size()) == static_cast<ssize_t>(Text.size());
    close(Fd);
    if (Written && (rename(Absolute.c_str(), (Trash + "/files/" + Name).c_str()) == 0))
    {
      return true;
    }
    unlink(Info.c_str());
    return false;
  }
  return false;
}

}  // namespace

// --- modules ----------------------------------------------------------------------

HMODULE GetModuleHandle(LPCWSTR ModuleName)
{
  TModule * M = KnownModule(ModuleName);
  if (M == nullptr)
  {
    SetLastError(ERROR_MOD_NOT_FOUND);
  }
  return M;
}

HMODULE LoadLibrary(LPCWSTR FileName)
{
  return GetModuleHandle(FileName);
}

FARPROC GetProcAddress(HMODULE Module, LPCSTR ProcName)
{
  if ((Module == &Kernel32Module) && (ProcName != nullptr) &&
      (std::strcmp(ProcName, "GetTimeZoneInformationForYear") == 0))
  {
    return reinterpret_cast<FARPROC>(&GetTimeZoneInformationForYear);
  }
  SetLastError(ERROR_PROC_NOT_FOUND);
  return nullptr;
}

BOOL FreeLibrary(HMODULE)
{
  return TRUE;
}

// --- processes ----------------------------------------------------------------------

HANDLE CreateToolhelp32Snapshot(DWORD Flags, DWORD)
{
  if ((Flags & TH32CS_SNAPPROCESS) == 0)
  {
    SetLastError(ERROR_NOT_SUPPORTED);
    return INVALID_HANDLE_VALUE;
  }
  auto Snapshot = std::make_shared<TSnapshot>();
  if (DIR * D = opendir("/proc"))
  {
    while (dirent * E = readdir(D))
    {
      char * End = nullptr;
      const unsigned long Pid = std::strtoul(E->d_name, &End, 10);
      PROCESSENTRY32W Entry;
      if ((End != E->d_name) && (*End == '\0') && ReadProcessEntry(static_cast<DWORD>(Pid), Entry))
      {
        Snapshot->Entries.push_back(Entry);
      }
    }
    closedir(D);
  }
  return linscp::RegisterKernelObject(Snapshot);
}

namespace {

BOOL SnapshotEntry(HANDLE Handle, PROCESSENTRY32W * Entry, bool First)
{
  auto Snapshot = std::dynamic_pointer_cast<TSnapshot>(linscp::FindKernelObject(Handle));
  if ((Snapshot == nullptr) || (Entry == nullptr))
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  if (First)
  {
    Snapshot->Next = 0;
  }
  if (Snapshot->Next >= Snapshot->Entries.size())
  {
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
  }
  *Entry = Snapshot->Entries[Snapshot->Next++];
  return TRUE;
}

}  // namespace

BOOL Process32First(HANDLE Snapshot, PROCESSENTRY32W * Entry)
{
  return SnapshotEntry(Snapshot, Entry, true);
}

BOOL Process32Next(HANDLE Snapshot, PROCESSENTRY32W * Entry)
{
  return SnapshotEntry(Snapshot, Entry, false);
}

HANDLE OpenProcess(DWORD, BOOL, DWORD ProcessId)
{
  struct stat St{};
  if (stat(("/proc/" + std::to_string(ProcessId)).c_str(), &St) != 0)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return nullptr;
  }
  auto Process = std::make_shared<TProcess>();
  Process->Id = ProcessId;
  return linscp::RegisterKernelObject(Process);
}

HANDLE GetCurrentProcess(void)
{
  return INVALID_HANDLE_VALUE;
}

DWORD GetModuleFileNameEx(HANDLE Process, HMODULE Module, LPWSTR FileName, DWORD Size)
{
  DWORD Pid = 0;
  if (Process == INVALID_HANDLE_VALUE)
  {
    Pid = static_cast<DWORD>(getpid());
  }
  else if (auto P = std::dynamic_pointer_cast<TProcess>(linscp::FindKernelObject(Process)))
  {
    Pid = P->Id;
  }
  if ((Pid == 0) || (Module != nullptr) || (FileName == nullptr) || (Size == 0))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  char Target[4096];
  const ssize_t N = readlink(("/proc/" + std::to_string(Pid) + "/exe").c_str(), Target, sizeof(Target) - 1);
  if (N < 0)
  {
    SetLastError(Win32ErrorFromErrno(errno));
    return 0;
  }
  // As Win32: truncated to fit, and the length copied returned.
  std::u16string Path = FromUtf8(std::string(Target, static_cast<std::size_t>(N)));
  if (Path.size() >= Size)
  {
    Path.resize(Size - 1);
  }
  CopyOut(Path, FileName, Size);
  return static_cast<DWORD>(Path.size());
}

BOOL EnumProcessModules(HANDLE, HMODULE *, DWORD, LPDWORD Needed)
{
  if (Needed != nullptr)
  {
    *Needed = 0;
  }
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}

BOOL IsWow64Process(HANDLE, BOOL * Wow64Process)
{
  if (Wow64Process != nullptr)
  {
    *Wow64Process = FALSE;
  }
  return TRUE;
}

BOOL GetProductInfo(DWORD, DWORD, DWORD, DWORD, PDWORD ReturnedProductType)
{
  if (ReturnedProductType != nullptr)
  {
    *ReturnedProductType = 0;     // PRODUCT_UNDEFINED
  }
  return FALSE;
}

// --- environment, paths, strings ------------------------------------------------------

DWORD ExpandEnvironmentStrings(LPCWSTR Src, LPWSTR Dst, DWORD Size)
{
  const std::u16string In = Wide(Src);
  std::u16string Out;
  for (std::size_t I = 0; I < In.size();)
  {
    const std::size_t End = (In[I] == u'%') ? In.find(u'%', I + 1) : std::u16string::npos;
    if (End == std::u16string::npos)
    {
      Out += In[I++];
      continue;
    }
    const std::u16string Name = In.substr(I + 1, End - I - 1);
    const char * Value = Name.empty() ? nullptr : getenv(ToUtf8(Name).c_str());
    if (Value != nullptr)
    {
      Out += FromUtf8(Value);
      I = End + 1;
    }
    else
    {
      // Left as written; the closing % may open the next reference.
      Out += In.substr(I, End - I);
      I = End;
    }
  }
  const DWORD Needed = static_cast<DWORD>(Out.size()) + 1;
  if ((Dst != nullptr) && (Size >= Needed))
  {
    CopyOut(Out, Dst, Size);
  }
  return Needed;
}

LPCWSTR PathSkipRoot(LPCWSTR Path)
{
  if (Path == nullptr)
  {
    return nullptr;
  }
  const auto IsSep = [](wchar_t C) { return C == L'\\'; };
  // "\\server\share\": past the share and its separator.
  if (IsSep(Path[0]) && IsSep(Path[1]))
  {
    const wchar_t * P = Path + 2;
    for (int Part = 0; Part < 2; ++Part)
    {
      while ((*P != 0) && !IsSep(*P))
      {
        ++P;
      }
      if (*P == 0)
      {
        return (Part == 0) ? nullptr : P;
      }
      ++P;
    }
    return P;
  }
  // "C:\". A bare leading backslash is not a root to PathSkipRoot: null, as
  // shlwapi returns under Wine, which the same test runs against.
  if ((Path[0] != 0) && (Path[1] == L':') && IsSep(Path[2]))
  {
    return Path + 3;
  }
  return nullptr;
}

BOOL PathIsRelative(LPCWSTR Path)
{
  if ((Path == nullptr) || (Path[0] == 0))
  {
    return TRUE;
  }
  if (Path[0] == L'\\')
  {
    return FALSE;
  }
  return (Path[1] == L':') ? FALSE : TRUE;
}

int StrCmpLogicalW(LPCWSTR A, LPCWSTR B)
{
  const std::u16string X = Wide(A), Y = Wide(B);
  std::size_t I = 0, J = 0;
  while ((I < X.size()) && (J < Y.size()))
  {
    if (IsDigit(X[I]) && IsDigit(Y[J]))
    {
      // Numbers by value: skip leading zeros, then longer is larger.
      std::size_t I0 = I, J0 = J;
      while ((I0 < X.size()) && (X[I0] == u'0')) { ++I0; }
      while ((J0 < Y.size()) && (Y[J0] == u'0')) { ++J0; }
      std::size_t I1 = I0, J1 = J0;
      while ((I1 < X.size()) && IsDigit(X[I1])) { ++I1; }
      while ((J1 < Y.size()) && IsDigit(Y[J1])) { ++J1; }
      if ((I1 - I0) != (J1 - J0))
      {
        return ((I1 - I0) < (J1 - J0)) ? -1 : 1;
      }
      for (std::size_t K = 0; K < I1 - I0; ++K)
      {
        if (X[I0 + K] != Y[J0 + K])
        {
          return (X[I0 + K] < Y[J0 + K]) ? -1 : 1;
        }
      }
      I = I1;
      J = J1;
      continue;
    }
    const char16_t P = Fold(X[I]), Q = Fold(Y[J]);
    if (P != Q)
    {
      return (P < Q) ? -1 : 1;
    }
    ++I;
    ++J;
  }
  if (I < X.size()) { return 1; }
  if (J < Y.size()) { return -1; }
  return 0;
}

int lstrcmpi(LPCWSTR A, LPCWSTR B)
{
  const std::u16string X = Wide(A), Y = Wide(B);
  for (std::size_t I = 0; (I < X.size()) && (I < Y.size()); ++I)
  {
    const char16_t P = Fold(X[I]), Q = Fold(Y[I]);
    if (P != Q)
    {
      return (P < Q) ? -1 : 1;
    }
  }
  return (X.size() == Y.size()) ? 0 : ((X.size() < Y.size()) ? -1 : 1);
}

int lstrcmp(LPCWSTR A, LPCWSTR B)
{
  const int Folded = lstrcmpi(A, B);
  if (Folded != 0)
  {
    return Folded;
  }
  // Equal but for case: lower case sorts first.
  const std::u16string X = Wide(A), Y = Wide(B);
  for (std::size_t I = 0; I < X.size(); ++I)
  {
    if (X[I] != Y[I])
    {
      return (X[I] > Y[I]) ? -1 : 1;
    }
  }
  return 0;
}

FILE * _wfopen(LPCWSTR FileName, LPCWSTR Mode)
{
  return fopen(linscp::WideToUtf8(FileName).c_str(), linscp::WideToUtf8(Mode).c_str());
}

wchar_t * _wcsdup(const wchar_t * S)
{
  if (S == nullptr)
  {
    return nullptr;
  }
  const std::size_t N = linscp_WideLength(S);
  auto * R = static_cast<wchar_t *>(std::malloc((N + 1) * sizeof(wchar_t)));
  if (R != nullptr)
  {
    std::memcpy(R, S, (N + 1) * sizeof(wchar_t));
  }
  return R;
}

// --- shell folders --------------------------------------------------------------------

HRESULT SHGetFolderPath(HWND, int Folder, HANDLE, DWORD, LPWSTR Path)
{
  std::string Result;
  switch (Folder & 0xFF)
  {
    case CSIDL_PERSONAL: Result = UserDir("XDG_DOCUMENTS_DIR", "Documents"); break;
    case CSIDL_DESKTOPDIRECTORY: Result = UserDir("XDG_DESKTOP_DIR", "Desktop"); break;
    case CSIDL_APPDATA:
      Result = Env("XDG_CONFIG_HOME");
      if (Result.empty()) { Result = Home() + "/.config"; }
      break;
    case CSIDL_LOCAL_APPDATA:
      Result = Env("XDG_DATA_HOME");
      if (Result.empty()) { Result = Home() + "/.local/share"; }
      break;
    case CSIDL_COMMON_APPDATA: Result = "/etc/xdg"; break;
    case CSIDL_PROFILE: Result = Home(); break;
    default:
      return E_INVALIDARG;
  }
  if (Path == nullptr)
  {
    return E_INVALIDARG;
  }
  CopyOut(FromUtf8(Result), Path, MAX_PATH);
  return S_OK;
}

// --- memory -------------------------------------------------------------------------------

HGLOBAL GlobalFree(HGLOBAL Memory)
{
  std::free(Memory);
  return nullptr;
}

void CoTaskMemFree(LPVOID Memory)
{
  std::free(Memory);
}

// --- MIME --------------------------------------------------------------------------------

HRESULT FindMimeFromData(LPVOID, LPCWSTR Url, LPVOID, DWORD, LPCWSTR, DWORD MimeFlags,
  LPWSTR * MimeOut, DWORD)
{
  if ((MimeOut == nullptr) || (Url == nullptr) || ((MimeFlags & FMFD_URLASFILENAME) == 0))
  {
    return E_INVALIDARG;
  }
  *MimeOut = nullptr;
  std::string Name = linscp::WideToUtf8(Url);
  const std::size_t Slash = Name.find_last_of("/\\");
  if (Slash != std::string::npos)
  {
    Name = Name.substr(Slash + 1);
  }
  const std::string Type = MimeForName(Name);
  if (Type.empty())
  {
    return E_FAIL;
  }
  const std::u16string W = FromUtf8(Type);
  auto * R = static_cast<wchar_t *>(std::malloc((W.size() + 1) * sizeof(wchar_t)));
  if (R == nullptr)
  {
    return E_FAIL;
  }
  CopyOut(W, R, static_cast<DWORD>(W.size() + 1));
  *MimeOut = R;
  return S_OK;
}

// --- code pages and locale ---------------------------------------------------------------

BOOL GetCPInfoEx(UINT CodePage, DWORD, CPINFOEXW * Info)
{
  if (Info == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  std::memset(Info, 0, sizeof(*Info));
  Info->DefaultChar[0] = '?';
  Info->UnicodeDefaultChar = L'?';
  if ((CodePage == CP_ACP) || (CodePage == CP_OEMCP) || (CodePage == 28591))
  {
    Info->MaxCharSize = 1;
    Info->CodePage = 28591;
    CopyOut(u"28591  (ISO 8859-1 Latin I)", Info->CodePageName, MAX_PATH);
    return TRUE;
  }
  if (CodePage == CP_UTF8)
  {
    Info->MaxCharSize = 4;
    Info->CodePage = CP_UTF8;
    CopyOut(u"65001  (UTF-8)", Info->CodePageName, MAX_PATH);
    return TRUE;
  }
  SetLastError(ERROR_INVALID_PARAMETER);
  return FALSE;
}

LCID GetUserDefaultLCID(void)
{
  return 0x0409;
}

// --- reparse points ------------------------------------------------------------------------

namespace {

// The NT layout core declares for itself (Common.cpp), symbolic-link arm.
struct TSymbolicLinkReparse
{
  ULONG ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  USHORT SubstituteNameOffset;
  USHORT SubstituteNameLength;
  USHORT PrintNameOffset;
  USHORT PrintNameLength;
  ULONG Flags;
  // PathBuffer follows.
};

}  // namespace

BOOL DeviceIoControl(HANDLE Device, DWORD IoControlCode, LPVOID, DWORD, LPVOID OutBuffer,
  DWORD OutSize, LPDWORD BytesReturned, LPOVERLAPPED)
{
  if (IoControlCode != FSCTL_GET_REPARSE_POINT)
  {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
  }
  const auto Value = reinterpret_cast<std::intptr_t>(Device);
  if ((Device == INVALID_HANDLE_VALUE) || (Value < 0) ||
      (static_cast<std::uintptr_t>(Value) >= LinscpKernelHandleBase))
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  char Target[4096];
  const ssize_t N = readlinkat(static_cast<int>(Value), "", Target, sizeof(Target) - 1);
  if (N < 0)
  {
    SetLastError((errno == EINVAL) || (errno == ENOENT) ? ERROR_NOT_A_REPARSE_POINT
                                                        : Win32ErrorFromErrno(errno));
    return FALSE;
  }
  const std::u16string Name = FromUtf8(std::string(Target, static_cast<std::size_t>(N)));
  const std::size_t NameBytes = Name.size() * sizeof(char16_t);
  // Substitute name then print name, each followed by a terminator.
  const std::size_t Total = sizeof(TSymbolicLinkReparse) + 2 * (NameBytes + sizeof(char16_t));
  if ((OutBuffer == nullptr) || (OutSize < Total) || (NameBytes > 0x7FFF))
  {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
  }
  auto * R = static_cast<TSymbolicLinkReparse *>(OutBuffer);
  std::memset(R, 0, Total);
  R->ReparseTag = IO_REPARSE_TAG_SYMLINK;
  R->ReparseDataLength = static_cast<USHORT>(Total - 8);
  R->SubstituteNameOffset = 0;
  R->SubstituteNameLength = static_cast<USHORT>(NameBytes);
  R->PrintNameOffset = static_cast<USHORT>(NameBytes + sizeof(char16_t));
  R->PrintNameLength = static_cast<USHORT>(NameBytes);
  R->Flags = (!Name.empty() && (Name[0] == u'/')) ? 0 : SYMLINK_FLAG_RELATIVE;
  auto * Buffer = reinterpret_cast<char *>(R + 1);
  std::memcpy(Buffer, Name.data(), NameBytes);
  std::memcpy(Buffer + R->PrintNameOffset, Name.data(), NameBytes);
  if (BytesReturned != nullptr)
  {
    *BytesReturned = static_cast<DWORD>(Total);
  }
  return TRUE;
}

// --- shell file operations ------------------------------------------------------------------

int SHFileOperation(LPSHFILEOPSTRUCTW FileOp)
{
  if ((FileOp == nullptr) || (FileOp->wFunc != FO_DELETE) || (FileOp->pFrom == nullptr))
  {
    return ERROR_NOT_SUPPORTED;
  }
  FileOp->fAnyOperationsAborted = FALSE;
  // pFrom is a list of names, each null-terminated, ending with an empty one.
  for (LPCWSTR Name = FileOp->pFrom; *Name != 0; Name += linscp_WideLength(Name) + 1)
  {
    const std::string Path = linscp::WideToUtf8(Name);
    struct stat St{};
    if (lstat(Path.c_str(), &St) != 0)
    {
      return static_cast<int>(Win32ErrorFromErrno(errno));
    }
    const bool Done = ((FileOp->fFlags & FOF_ALLOWUNDO) != 0) ? MoveToTrash(Path) : RemoveTree(Path);
    if (!Done)
    {
      return static_cast<int>((errno != 0) ? Win32ErrorFromErrno(errno) : ERROR_ACCESS_DENIED);
    }
  }
  return 0;
}

// --- registry ------------------------------------------------------------------------------

LONG RegQueryValueEx(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD)
{
  return ERROR_INVALID_HANDLE;
}

LONG RegSetValueEx(HKEY, LPCWSTR, DWORD, DWORD, const BYTE *, DWORD)
{
  return ERROR_INVALID_HANDLE;
}

// --- user name ------------------------------------------------------------------------------

BOOLEAN GetUserNameEx(EXTENDED_NAME_FORMAT Format, LPWSTR Name, ULONG * Size)
{
  if ((Format != NameSamCompatible) || (Size == nullptr))
  {
    SetLastError(ERROR_NONE_MAPPED);
    return 0;
  }
  char Host[256] = {};
  gethostname(Host, sizeof(Host) - 1);
  std::string User;
  if (const passwd * Pw = getpwuid(geteuid()))
  {
    User = Pw->pw_name;
  }
  else
  {
    User = std::to_string(geteuid());
  }
  // The short host name, as Windows' NetBIOS computer name is.
  std::string HostName = Host;
  const std::size_t Dot = HostName.find('.');
  if (Dot != std::string::npos)
  {
    HostName.resize(Dot);
  }
  const std::u16string Full = FromUtf8(HostName + "\\" + User);
  if ((Name == nullptr) || (*Size < Full.size() + 1))
  {
    *Size = static_cast<ULONG>(Full.size() + 1);
    SetLastError(ERROR_MORE_DATA);
    return 0;
  }
  CopyOut(Full, Name, *Size);
  *Size = static_cast<ULONG>(Full.size());
  return 1;
}

// --- memory regions ---------------------------------------------------------------------------

SIZE_T VirtualQuery(LPCVOID Address, MEMORY_BASIC_INFORMATION * Info, SIZE_T Length)
{
  if ((Info == nullptr) || (Length < sizeof(MEMORY_BASIC_INFORMATION)))
  {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return 0;
  }
  const auto At = reinterpret_cast<std::uintptr_t>(Address);
  std::ifstream Maps("/proc/self/maps");
  std::string Line;
  std::uintptr_t PrevEnd = 0;
  while (std::getline(Maps, Line))
  {
    // "start-end perms offset dev inode [path]"
    char * End = nullptr;
    const std::uintptr_t Start = std::strtoull(Line.c_str(), &End, 16);
    const std::uintptr_t Stop = std::strtoull(End + 1, &End, 16);
    if (At < Start)
    {
      // In the gap before this mapping: free.
      std::memset(Info, 0, sizeof(*Info));
      Info->BaseAddress = reinterpret_cast<PVOID>(std::max(PrevEnd, At));
      Info->RegionSize = Start - std::max(PrevEnd, At);
      Info->State = MEM_FREE;
      return sizeof(*Info);
    }
    if (At < Stop)
    {
      std::istringstream Fields(Line);
      std::string Range, Perms, Offset, Device, Inode, Path;
      Fields >> Range >> Perms >> Offset >> Device >> Inode;
      std::getline(Fields, Path);
      const bool Anonymous = (Inode == "0");
      std::memset(Info, 0, sizeof(*Info));
      Info->BaseAddress = reinterpret_cast<PVOID>(Start);
      Info->AllocationBase = Info->BaseAddress;
      Info->RegionSize = Stop - Start;
      Info->State = MEM_COMMIT;
      Info->Type = Anonymous ? MEM_PRIVATE : MEM_MAPPED;
      return sizeof(*Info);
    }
    PrevEnd = Stop;
  }
  return 0;
}

// --- version resources ---------------------------------------------------------------------

extern "C" __attribute__((weak)) const linscp_VersionInfoData * linscp_OwnVersionInfo(void)
{
  return nullptr;
}

namespace {

// The block GetFileVersionInfo hands out: a header, the fixed information,
// the translation, then each string as key and value, null-terminated UTF-16.
// Only VerQueryValue reads it, and finds everything by walking it.
struct TVersionBlockHead
{
  DWORD Magic;
  DWORD Size;
  VS_FIXEDFILEINFO Fixed;
  WORD Translation[2];
  DWORD StringCount;
};
const DWORD VersionBlockMagic = 0x4C565349;   // "ISVL"

std::vector<char> BuildVersionBlock(const linscp_VersionInfoData & D)
{
  std::vector<char> B(sizeof(TVersionBlockHead));
  auto Append = [&B](const WCHAR * S)
  {
    const std::size_t N = (linscp_WideLength(S) + 1) * sizeof(WCHAR);
    const char * P = reinterpret_cast<const char *>(S);
    B.insert(B.end(), P, P + N);
  };
  for (int I = 0; I < D.StringCount; ++I)
  {
    Append(D.Strings[I].Key);
    Append(D.Strings[I].Value);
  }
  TVersionBlockHead Head{};
  Head.Magic = VersionBlockMagic;
  Head.Size = static_cast<DWORD>(B.size());
  Head.Fixed = D.Fixed;
  Head.Translation[0] = D.Language;
  Head.Translation[1] = D.CodePage;
  Head.StringCount = static_cast<DWORD>(D.StringCount);
  std::memcpy(B.data(), &Head, sizeof(Head));
  return B;
}

bool IsOwnExecutable(LPCWSTR FileName)
{
  char Self[4096], Given[4096];
  return (realpath("/proc/self/exe", Self) != nullptr) &&
         (realpath(linscp::WideToUtf8(FileName).c_str(), Given) != nullptr) &&
         (std::strcmp(Self, Given) == 0);
}

}  // namespace

DWORD GetFileVersionInfoSize(LPCWSTR FileName, LPDWORD Handle)
{
  if (Handle != nullptr)
  {
    *Handle = 0;
  }
  const linscp_VersionInfoData * D = linscp_OwnVersionInfo();
  if ((D == nullptr) || !IsOwnExecutable(FileName))
  {
    SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);
    return 0;
  }
  return static_cast<DWORD>(BuildVersionBlock(*D).size());
}

BOOL GetFileVersionInfo(LPCWSTR FileName, DWORD, DWORD Size, LPVOID Data)
{
  const linscp_VersionInfoData * D = linscp_OwnVersionInfo();
  if ((D == nullptr) || (Data == nullptr) || !IsOwnExecutable(FileName))
  {
    SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);
    return FALSE;
  }
  const std::vector<char> B = BuildVersionBlock(*D);
  if (Size < B.size())
  {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
  }
  std::memcpy(Data, B.data(), B.size());
  return TRUE;
}

BOOL VerQueryValue(LPCVOID Block, LPCWSTR SubBlock, LPVOID * Buffer, UINT * Length)
{
  const auto * Head = static_cast<const TVersionBlockHead *>(Block);
  if ((Head == nullptr) || (Head->Magic != VersionBlockMagic) || (SubBlock == nullptr) ||
      (Buffer == nullptr) || (Length == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  const std::u16string Path = Wide(SubBlock);
  if (Path == u"\\")
  {
    *Buffer = const_cast<VS_FIXEDFILEINFO *>(&Head->Fixed);
    *Length = sizeof(VS_FIXEDFILEINFO);
    return TRUE;
  }
  if (SameIgnoringCase(Path, "\\VarFileInfo\\Translation"))
  {
    *Buffer = const_cast<WORD *>(Head->Translation);
    *Length = sizeof(Head->Translation);
    return TRUE;
  }
  // \StringFileInfo\LLLLCCCC\Key, the block name in hex as core formats it.
  const std::u16string Prefix = u"\\StringFileInfo\\";
  if ((Path.size() > Prefix.size() + 9) &&
      SameIgnoringCase(Path.substr(0, Prefix.size()), "\\StringFileInfo\\") &&
      (Path[Prefix.size() + 8] == u'\\'))
  {
    char Expected[9];
    std::snprintf(Expected, sizeof(Expected), "%04X%04X", Head->Translation[0], Head->Translation[1]);
    if (SameIgnoringCase(Path.substr(Prefix.size(), 8), Expected))
    {
      const std::u16string Key = Path.substr(Prefix.size() + 9);
      const auto * P = reinterpret_cast<const WCHAR *>(Head + 1);
      for (DWORD I = 0; I < Head->StringCount; ++I)
      {
        const WCHAR * K = P;
        P += linscp_WideLength(P) + 1;
        const WCHAR * V = P;
        P += linscp_WideLength(P) + 1;
        if (Wide(K) == Key)
        {
          *Buffer = const_cast<WCHAR *>(V);
          *Length = static_cast<UINT>(linscp_WideLength(V) + 1);
          return TRUE;
        }
      }
    }
  }
  SetLastError(ERROR_RESOURCE_TYPE_NOT_FOUND);
  *Buffer = nullptr;
  *Length = 0;
  return FALSE;
}

// --- file type names ---------------------------------------------------------------------------

namespace {

// The untranslated <comment> of a type's shared-mime-info entry.
std::string MimeComment(const std::string & Type)
{
  for (const char * Root : { "/usr/share/mime/", "/usr/local/share/mime/" })
  {
    std::ifstream Xml(std::string(Root) + Type + ".xml");
    std::stringstream All;
    All << Xml.rdbuf();
    const std::string Text = All.str();
    const std::size_t Open = Text.find("<comment>");
    if (Open != std::string::npos)
    {
      const std::size_t Start = Open + 9;
      const std::size_t Close = Text.find("</comment>", Start);
      if (Close != std::string::npos)
      {
        return Text.substr(Start, Close - Start);
      }
    }
  }
  return std::string();
}

}  // namespace

DWORD_PTR SHGetFileInfo(LPCWSTR Path, DWORD FileAttributes, SHFILEINFOW * Info, UINT Size, UINT Flags)
{
  if ((Info == nullptr) || (Size < sizeof(SHFILEINFOW)) || ((Flags & SHGFI_USEFILEATTRIBUTES) == 0))
  {
    return 0;
  }
  std::memset(Info, 0, sizeof(*Info));
  std::string Name = linscp::WideToUtf8(Path);
  const std::size_t Slash = Name.find_last_of("/\\");
  if (Slash != std::string::npos)
  {
    Name = Name.substr(Slash + 1);
  }
  if ((Flags & SHGFI_TYPENAME) != 0)
  {
    std::string Type;
    if ((FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
      Type = "File folder";
    }
    else
    {
      const std::string Mime = MimeForName(Name);
      Type = Mime.empty() ? std::string() : MimeComment(Mime);
      if (Type.empty())
      {
        const std::size_t Dot = Name.rfind('.');
        if ((Dot != std::string::npos) && (Dot + 1 < Name.size()))
        {
          std::string Ext = Name.substr(Dot + 1);
          for (char & C : Ext)
          {
            if ((C >= 'a') && (C <= 'z')) { C = static_cast<char>(C - 'a' + 'A'); }
          }
          Type = Ext + " File";
        }
        else
        {
          Type = "File";
        }
      }
    }
    CopyOut(FromUtf8(Type).substr(0, 79), Info->szTypeName, 80);
  }
  if ((Flags & SHGFI_DISPLAYNAME) != 0)
  {
    CopyOut(FromUtf8(Name).substr(0, MAX_PATH - 1), Info->szDisplayName, MAX_PATH);
  }
  Info->dwAttributes = FileAttributes;
  // Nonzero: success. Windows returns the system image list's handle when
  // SHGFI_SYSICONINDEX is asked for.
  return 1;
}

BOOLEAN GetUserNameExA(EXTENDED_NAME_FORMAT Format, LPSTR Name, ULONG * Size)
{
  if (Size == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  wchar_t Wide[512];
  ULONG WideSize = 512;
  if (!GetUserNameEx(Format, Wide, &WideSize))
  {
    return 0;
  }
  const std::string N = linscp::WideToUtf8(Wide);
  if ((Name == nullptr) || (*Size < N.size() + 1))
  {
    *Size = static_cast<ULONG>(N.size() + 1);
    SetLastError(ERROR_MORE_DATA);
    return 0;
  }
  std::memcpy(Name, N.c_str(), N.size() + 1);
  *Size = static_cast<ULONG>(N.size());
  return 1;
}
