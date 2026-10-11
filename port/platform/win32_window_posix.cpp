// Windows and message queues over threads: what FileZilla's socket layer and
// its worker thread use of user32.
//
// FileZilla drives every socket through a hidden helper window per thread --
// WSAAsyncSelect posts to it, the layers post to it, a timer fires on it --
// and its worker runs a GetMessage loop fed with PostThreadMessage. No
// desktop is involved, so a window here is only what makes it a message
// target: a procedure, an owning thread and a user-data slot.
//
// Win32's rules, which FileZilla relies on:
//  - A thread has a queue from its first message call (or window, or timer);
//    PostThreadMessage to a thread without one fails with
//    ERROR_INVALID_THREAD_ID. The queue goes when the thread exits.
//  - Posted messages are retrieved in order, before any WM_TIMER; WM_TIMER is
//    synthesized when a timer is due, at most one per timer at a time.
//  - Filters: window NULL is any message of the thread, (HWND)-1 posted
//    thread messages only (not a thread timer's WM_TIMER, as Wine shows),
//    otherwise that window's; a range of 0..0 is every message, and WM_QUIT
//    is retrieved whatever the range.
//  - CreateWindowEx sends WM_NCCREATE and WM_CREATE to the procedure, and
//    DestroyWindow WM_DESTROY and WM_NCDESTROY; messages and timers of a
//    destroyed window go with it.
//  - A queue holds at most 10,000 posted messages (USERPostMessageLimit).
#include "win32_on_posix.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

using TClock = std::chrono::steady_clock;

const std::size_t PostedMessageLimit = 10000;

struct TTimer
{
  HWND Window;
  UINT_PTR Id;
  UINT Interval;
  TIMERPROC Proc;
  TClock::time_point Due;
};

struct TQueue
{
  std::deque<MSG> Posted;
  std::vector<TTimer> Timers;
  std::condition_variable Changed;
};

struct TWindow
{
  WNDPROC Proc;
  DWORD Thread;
  LONG_PTR UserData;
};

std::mutex GMutex;
std::map<DWORD, std::shared_ptr<TQueue>> GQueues;
std::map<HWND, TWindow> GWindows;
std::map<std::u16string, WNDPROC> GClasses;
std::map<std::u16string, UINT> GWindowMessages;
std::uintptr_t GNextWindow = 0x10000;
UINT_PTR GNextThreadTimer = 0x7F00;
ATOM GNextAtom = 0xC000;
UINT GNextWindowMessage = 0xC000;

// Class and message names compare case-insensitively, as Win32's atoms.
std::u16string Folded(LPCWSTR Name)
{
  std::u16string R;
  for (; (Name != nullptr) && (*Name != L'\0'); ++Name)
  {
    R += static_cast<char16_t>(linscp_ToLower(*Name));
  }
  return R;
}

DWORD Now()
{
  return GetTickCount();
}

// Requires GMutex.
std::shared_ptr<TQueue> QueueOf(DWORD Thread)
{
  const auto It = GQueues.find(Thread);
  return (It == GQueues.end()) ? nullptr : It->second;
}

// Removes the thread's queue, and its windows, when the thread exits.
struct TQueueOwner
{
  DWORD Thread = 0;
  ~TQueueOwner()
  {
    if (Thread == 0)
    {
      return;
    }
    std::lock_guard<std::mutex> Lock(GMutex);
    GQueues.erase(Thread);
    for (auto It = GWindows.begin(); It != GWindows.end();)
    {
      It = (It->second.Thread == Thread) ? GWindows.erase(It) : std::next(It);
    }
  }
};

thread_local TQueueOwner GQueueOwner;

// Requires GMutex: the calling thread's queue, created on first use.
std::shared_ptr<TQueue> OwnQueue()
{
  const DWORD Thread = GetCurrentThreadId();
  std::shared_ptr<TQueue> & Queue = GQueues[Thread];
  if (!Queue)
  {
    Queue = std::make_shared<TQueue>();
    GQueueOwner.Thread = Thread;
  }
  return Queue;
}

bool InRange(UINT Message, UINT Min, UINT Max)
{
  return ((Min == 0) && (Max == 0)) || (Message == WM_QUIT) || ((Message >= Min) && (Message <= Max));
}

const HWND ThreadMessages = reinterpret_cast<HWND>(static_cast<LONG_PTR>(-1));

bool WindowMatches(HWND MessageWindow, HWND Filter)
{
  if (Filter == ThreadMessages)
  {
    return MessageWindow == nullptr;
  }
  return (Filter == nullptr) || (MessageWindow == Filter);
}

// Requires GMutex. Takes (or, without Remove, copies) the first message the
// filter accepts: posted messages first, then a due timer.
bool Take(TQueue & Queue, LPMSG Msg, HWND Window, UINT Min, UINT Max, bool Remove)
{
  for (auto It = Queue.Posted.begin(); It != Queue.Posted.end(); ++It)
  {
    if (WindowMatches(It->hwnd, Window) && InRange(It->message, Min, Max))
    {
      *Msg = *It;
      if (Remove)
      {
        Queue.Posted.erase(It);
      }
      return true;
    }
  }
  if (!InRange(WM_TIMER, Min, Max) || (Window == ThreadMessages))
  {
    return false;
  }
  const TClock::time_point Current = TClock::now();
  for (TTimer & Timer : Queue.Timers)
  {
    if ((Timer.Due <= Current) && WindowMatches(Timer.Window, Window))
    {
      Msg->hwnd = Timer.Window;
      Msg->message = WM_TIMER;
      Msg->wParam = Timer.Id;
      Msg->lParam = reinterpret_cast<LPARAM>(Timer.Proc);
      Msg->time = Now();
      Msg->pt = POINT{ 0, 0 };
      if (Remove)
      {
        Timer.Due = Current + std::chrono::milliseconds(Timer.Interval);
      }
      return true;
    }
  }
  return false;
}

// Requires GMutex: when the next timer the filter accepts falls due.
bool NextDue(const TQueue & Queue, HWND Window, UINT Min, UINT Max, TClock::time_point & Due)
{
  if (!InRange(WM_TIMER, Min, Max) || (Window == ThreadMessages))
  {
    return false;
  }
  bool Any = false;
  for (const TTimer & Timer : Queue.Timers)
  {
    if (WindowMatches(Timer.Window, Window) && (!Any || (Timer.Due < Due)))
    {
      Due = Timer.Due;
      Any = true;
    }
  }
  return Any;
}

// Requires GMutex.
BOOL Post(TQueue & Queue, HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
  if (Queue.Posted.size() >= PostedMessageLimit)
  {
    SetLastError(ERROR_NOT_ENOUGH_QUOTA);
    return FALSE;
  }
  MSG M{};
  M.hwnd = Window;
  M.message = Message;
  M.wParam = WParam;
  M.lParam = LParam;
  M.time = Now();
  Queue.Posted.push_back(M);
  Queue.Changed.notify_all();
  return TRUE;
}

// Requires GMutex: the window's procedure, or null if there is no window.
WNDPROC ProcOf(HWND Window)
{
  const auto It = GWindows.find(Window);
  return (It == GWindows.end()) ? nullptr : It->second.Proc;
}

// SetTimer's result: a window timer's id (nonzero, as Win32 promises), or
// the new thread timer's.
UINT_PTR TimerResult(HWND Window, UINT_PTR Id)
{
  return ((Window != nullptr) && (Id == 0)) ? 1 : Id;
}

bool ValidFilter(HWND Window)
{
  return (Window == nullptr) || (Window == ThreadMessages) ||
    (GWindows.find(Window) != GWindows.end());
}

}  // namespace

ATOM RegisterClassEx(const WNDCLASSEXW * Class)
{
  if ((Class == nullptr) || (Class->cbSize != sizeof(WNDCLASSEXW)) || (Class->lpszClassName == nullptr) ||
      (Class->lpfnWndProc == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  std::lock_guard<std::mutex> Lock(GMutex);
  const std::u16string Name = Folded(Class->lpszClassName);
  if (GClasses.find(Name) != GClasses.end())
  {
    SetLastError(ERROR_CLASS_ALREADY_EXISTS);
    return 0;
  }
  GClasses[Name] = Class->lpfnWndProc;
  return GNextAtom++;
}

HWND CreateWindowEx(DWORD ExStyle, LPCWSTR ClassName, LPCWSTR WindowName, DWORD Style,
  int X, int Y, int Width, int Height, HWND Parent, HMENU Menu, HINSTANCE Instance, LPVOID Param)
{
  HWND Window;
  WNDPROC Proc;
  {
    std::lock_guard<std::mutex> Lock(GMutex);
    const auto Class = GClasses.find(Folded(ClassName));
    if (Class == GClasses.end())
    {
      SetLastError(ERROR_CANNOT_FIND_WND_CLASS);
      return nullptr;
    }
    Proc = Class->second;
    OwnQueue();
    Window = reinterpret_cast<HWND>(GNextWindow);
    GNextWindow += 4;
    GWindows[Window] = TWindow{ Proc, GetCurrentThreadId(), 0 };
  }
  CREATESTRUCTW Create{};
  Create.lpCreateParams = Param;
  Create.hInstance = Instance;
  Create.hMenu = Menu;
  Create.hwndParent = Parent;
  Create.cy = Height;
  Create.cx = Width;
  Create.y = Y;
  Create.x = X;
  Create.style = static_cast<LONG>(Style);
  Create.lpszName = WindowName;
  Create.lpszClass = ClassName;
  Create.dwExStyle = ExStyle;
  // As Win32: a procedure refusing WM_NCCREATE (FALSE) or WM_CREATE (-1)
  // fails the creation, and the window is destroyed.
  if ((Proc(Window, WM_NCCREATE, 0, reinterpret_cast<LPARAM>(&Create)) == 0) ||
      (Proc(Window, WM_CREATE, 0, reinterpret_cast<LPARAM>(&Create)) == -1))
  {
    DestroyWindow(Window);
    SetLastError(ERROR_INVALID_PARAMETER);
    return nullptr;
  }
  return Window;
}

BOOL DestroyWindow(HWND Window)
{
  WNDPROC Proc;
  {
    std::lock_guard<std::mutex> Lock(GMutex);
    const auto It = GWindows.find(Window);
    if (It == GWindows.end())
    {
      SetLastError(ERROR_INVALID_WINDOW_HANDLE);
      return FALSE;
    }
    if (It->second.Thread != GetCurrentThreadId())
    {
      // Win32 destroys a window only from the thread that owns it.
      SetLastError(ERROR_ACCESS_DENIED);
      return FALSE;
    }
    Proc = It->second.Proc;
  }
  Proc(Window, WM_DESTROY, 0, 0);
  Proc(Window, WM_NCDESTROY, 0, 0);
  std::lock_guard<std::mutex> Lock(GMutex);
  const auto It = GWindows.find(Window);
  if (It != GWindows.end())
  {
    const std::shared_ptr<TQueue> Queue = QueueOf(It->second.Thread);
    if (Queue)
    {
      for (auto M = Queue->Posted.begin(); M != Queue->Posted.end();)
      {
        M = (M->hwnd == Window) ? Queue->Posted.erase(M) : std::next(M);
      }
      for (auto T = Queue->Timers.begin(); T != Queue->Timers.end();)
      {
        T = (T->Window == Window) ? Queue->Timers.erase(T) : std::next(T);
      }
    }
    GWindows.erase(It);
  }
  return TRUE;
}

BOOL IsWindow(HWND Window)
{
  std::lock_guard<std::mutex> Lock(GMutex);
  return (GWindows.find(Window) != GWindows.end()) ? TRUE : FALSE;
}

LRESULT DefWindowProc(HWND, UINT Message, WPARAM, LPARAM)
{
  // WM_NCCREATE must be accepted for creation to go ahead; nothing else has a
  // default action without a desktop.
  return (Message == WM_NCCREATE) ? TRUE : 0;
}

LONG_PTR GetWindowLongPtr(HWND Window, int Index)
{
  std::lock_guard<std::mutex> Lock(GMutex);
  const auto It = GWindows.find(Window);
  if (It == GWindows.end())
  {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    return 0;
  }
  switch (Index)
  {
    case GWLP_USERDATA: return It->second.UserData;
    case GWLP_WNDPROC: return reinterpret_cast<LONG_PTR>(It->second.Proc);
    default: SetLastError(ERROR_INVALID_INDEX); return 0;
  }
}

LONG_PTR SetWindowLongPtr(HWND Window, int Index, LONG_PTR Value)
{
  std::lock_guard<std::mutex> Lock(GMutex);
  const auto It = GWindows.find(Window);
  if (It == GWindows.end())
  {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    return 0;
  }
  LONG_PTR Previous;
  switch (Index)
  {
    case GWLP_USERDATA:
      Previous = It->second.UserData;
      It->second.UserData = Value;
      break;
    case GWLP_WNDPROC:
      Previous = reinterpret_cast<LONG_PTR>(It->second.Proc);
      It->second.Proc = reinterpret_cast<WNDPROC>(Value);
      break;
    default:
      SetLastError(ERROR_INVALID_INDEX);
      return 0;
  }
  // A previous value of 0 is told apart from failure by GetLastError.
  SetLastError(NO_ERROR);
  return Previous;
}

UINT RegisterWindowMessage(LPCWSTR Name)
{
  if ((Name == nullptr) || (*Name == L'\0'))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  std::lock_guard<std::mutex> Lock(GMutex);
  UINT & Message = GWindowMessages[Folded(Name)];
  if (Message == 0)
  {
    Message = GNextWindowMessage++;
  }
  return Message;
}

BOOL PostMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
  std::lock_guard<std::mutex> Lock(GMutex);
  if (Window == nullptr)
  {
    // As Win32: a thread message to the calling thread.
    return Post(*OwnQueue(), nullptr, Message, WParam, LParam);
  }
  const auto It = GWindows.find(Window);
  const std::shared_ptr<TQueue> Queue = (It == GWindows.end()) ? nullptr : QueueOf(It->second.Thread);
  if (!Queue)
  {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    return FALSE;
  }
  return Post(*Queue, Window, Message, WParam, LParam);
}

BOOL PostThreadMessage(DWORD ThreadId, UINT Message, WPARAM WParam, LPARAM LParam)
{
  std::lock_guard<std::mutex> Lock(GMutex);
  const std::shared_ptr<TQueue> Queue = QueueOf(ThreadId);
  if (!Queue)
  {
    SetLastError(ERROR_INVALID_THREAD_ID);
    return FALSE;
  }
  return Post(*Queue, nullptr, Message, WParam, LParam);
}

BOOL GetMessage(LPMSG Msg, HWND Window, UINT FilterMin, UINT FilterMax)
{
  std::unique_lock<std::mutex> Lock(GMutex);
  if (!ValidFilter(Window))
  {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    return -1;
  }
  const std::shared_ptr<TQueue> Queue = OwnQueue();
  while (!Take(*Queue, Msg, Window, FilterMin, FilterMax, true))
  {
    TClock::time_point Due;
    if (NextDue(*Queue, Window, FilterMin, FilterMax, Due))
    {
      Queue->Changed.wait_until(Lock, Due);
    }
    else
    {
      Queue->Changed.wait(Lock);
    }
  }
  return (Msg->message == WM_QUIT) ? FALSE : TRUE;
}

BOOL PeekMessage(LPMSG Msg, HWND Window, UINT FilterMin, UINT FilterMax, UINT RemoveMsg)
{
  std::lock_guard<std::mutex> Lock(GMutex);
  if (!ValidFilter(Window))
  {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    return FALSE;
  }
  return Take(*OwnQueue(), Msg, Window, FilterMin, FilterMax, (RemoveMsg & PM_REMOVE) != 0) ? TRUE : FALSE;
}

BOOL TranslateMessage(const MSG *)
{
  // Only keyboard messages are translated, and there is no keyboard.
  return FALSE;
}

LRESULT DispatchMessage(const MSG * Msg)
{
  if (Msg == nullptr)
  {
    return 0;
  }
  if ((Msg->message == WM_TIMER) && (Msg->lParam != 0))
  {
    // A timer with a callback calls it instead of the window procedure.
    reinterpret_cast<TIMERPROC>(Msg->lParam)(Msg->hwnd, WM_TIMER, Msg->wParam, Now());
    return 0;
  }
  if (Msg->hwnd == nullptr)
  {
    return 0;
  }
  WNDPROC Proc;
  {
    std::lock_guard<std::mutex> Lock(GMutex);
    Proc = ProcOf(Msg->hwnd);
  }
  if (Proc == nullptr)
  {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    return 0;
  }
  return Proc(Msg->hwnd, Msg->message, Msg->wParam, Msg->lParam);
}

UINT_PTR SetTimer(HWND Window, UINT_PTR IdEvent, UINT Elapse, TIMERPROC TimerFunc)
{
  if (Elapse < USER_TIMER_MINIMUM)
  {
    Elapse = USER_TIMER_MINIMUM;
  }
  if (Elapse > USER_TIMER_MAXIMUM)
  {
    Elapse = USER_TIMER_MAXIMUM;
  }
  std::lock_guard<std::mutex> Lock(GMutex);
  std::shared_ptr<TQueue> Queue;
  if (Window != nullptr)
  {
    // The timer is the window's, and fires in the thread that owns it.
    const auto It = GWindows.find(Window);
    Queue = (It == GWindows.end()) ? nullptr : QueueOf(It->second.Thread);
    if (!Queue)
    {
      SetLastError(ERROR_INVALID_WINDOW_HANDLE);
      return 0;
    }
  }
  else
  {
    Queue = OwnQueue();
  }
  const TClock::time_point Due = TClock::now() + std::chrono::milliseconds(Elapse);
  // An existing timer with the same identity is replaced, as Win32 does; a
  // thread timer is identified by the id SetTimer returned for it.
  for (TTimer & Timer : Queue->Timers)
  {
    if ((Timer.Window == Window) && (Timer.Id == IdEvent) && ((Window != nullptr) || (IdEvent != 0)))
    {
      Timer.Interval = Elapse;
      Timer.Proc = TimerFunc;
      Timer.Due = Due;
      Queue->Changed.notify_all();
      return TimerResult(Window, IdEvent);
    }
  }
  const UINT_PTR Id = (Window != nullptr) ? IdEvent : GNextThreadTimer++;
  Queue->Timers.push_back(TTimer{ Window, Id, Elapse, TimerFunc, Due });
  Queue->Changed.notify_all();
  return TimerResult(Window, Id);
}

BOOL KillTimer(HWND Window, UINT_PTR IdEvent)
{
  std::lock_guard<std::mutex> Lock(GMutex);
  std::shared_ptr<TQueue> Queue;
  if (Window != nullptr)
  {
    const auto It = GWindows.find(Window);
    Queue = (It == GWindows.end()) ? nullptr : QueueOf(It->second.Thread);
  }
  else
  {
    Queue = QueueOf(GetCurrentThreadId());
  }
  if (Queue)
  {
    for (auto T = Queue->Timers.begin(); T != Queue->Timers.end(); ++T)
    {
      if ((T->Window == Window) && (T->Id == IdEvent))
      {
        Queue->Timers.erase(T);
        return TRUE;
      }
    }
  }
  SetLastError(ERROR_INVALID_PARAMETER);
  return FALSE;
}
