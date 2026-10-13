// The shell's configuration: upstream's TGUIConfiguration (compiled as it is,
// port/gui), with what the shell keeps of TWinConfiguration's settings under
// TWinConfiguration's own keys, so a WinSCP.ini carries over both ways
// (ADR 0014). VCL side.
#ifndef LINSCP_QT_SHELL_CONFIGURATION_H
#define LINSCP_QT_SHELL_CONFIGURATION_H

#include <Common.h>
#include <GUIConfiguration.h>

class TShellConfiguration : public TGUIConfiguration
{
public:
  // As TWinConfiguration's constructor: the defaults, then the translation
  // of the user interface's language, if there is one.
  TShellConfiguration();

  virtual UnicodeString TemporaryDir(bool Mask);
  // A locale for this run only (linscp's --lang): applied, but what is
  // saved stays the locale chosen before (or after, through Locale).
  void OverrideLocale(LCID Locale);

protected:
  virtual void __fastcall SaveData(THierarchicalStorage * Storage, bool All);
  virtual void __fastcall LoadData(THierarchicalStorage * Storage);

private:
  bool FOverridden = false;
  LCID FSavedLocale = 0;
};

#endif
