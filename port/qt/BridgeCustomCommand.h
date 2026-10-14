// TWinInteractiveCustomCommand's part in the bridge: a custom command's
// prompt patterns asked through the frontend (TCustomCommandPrompts), and its
// local commands' output. Shared by the bridge's custom commands
// (Session.cpp) and the PuTTY slice (PuttySlice.h). VCL side only.
#ifndef LINSCP_QT_BRIDGE_CUSTOM_COMMAND_H
#define LINSCP_QT_BRIDGE_CUSTOM_COMMAND_H

#include "BridgeVcl.h"

#include <FileMasks.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace CoreBridge
{

// TWinInteractiveCustomCommand, its questions the frontend's.
class TBridgeInteractiveCustomCommand : public TInteractiveCustomCommand
{
public:
  TBridgeInteractiveCustomCommand(TCustomCommand * ChildCustomCommand, const UnicodeString & Name,
      const TCustomCommandPrompts & Prompts) :
    TInteractiveCustomCommand(ChildCustomCommand),
    FName(StripEllipsis(StripHotkey(Name))),
    FPrompts(Prompts)
  {
  }

protected:
  virtual void __fastcall PatternHint(int Index, const UnicodeString & Pattern)
  {
    if (IsPromptPattern(Pattern))
    {
      UnicodeString Prompt;
      UnicodeString Default;
      bool Delimit = false;
      ParsePromptPattern(Pattern, Prompt, Default, Delimit);
      FIndexes.insert(std::make_pair(Index, FAsked.size()));
      FAsked.emplace_back(Utf8(Prompt), Utf8(Default));
    }
  }

  virtual void __fastcall Prompt(int Index, const UnicodeString & /*Prompt*/, UnicodeString & Value)
  {
    auto I = FIndexes.find(Index);
    if (I == FIndexes.end())
    {
      return;
    }
    if (FValues.empty())
    {
      if (!FPrompts.Prompt || !FPrompts.Prompt(Utf8(FName), FAsked, FValues) || (FValues.size() != FAsked.size()))
      {
        Abort();
      }
    }
    Value = FromUtf8(FValues[I->second]);
  }

  virtual void __fastcall Execute(const UnicodeString & Command, UnicodeString & Value)
  {
    Value = FPrompts.Execute ? TrimRight(FromUtf8(FPrompts.Execute(Utf8(Command)))) : UnicodeString();
  }

private:
  UnicodeString FName;
  TCustomCommandPrompts FPrompts;
  std::map<int, size_t> FIndexes;
  std::vector<std::pair<std::string, std::string>> FAsked;
  std::vector<std::string> FValues;
};

}  // namespace CoreBridge

#endif
