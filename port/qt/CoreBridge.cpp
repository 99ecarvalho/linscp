#include <vcl.h>
#pragma hdrstop

#include "CoreBridge.h"

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <HeadlessMain.h>

#include <stdexcept>

namespace {

std::string Utf8(const UnicodeString & Str)
{
  const UTF8String Result(Str);
  return std::string(Result.c_str(), Result.Length());
}

}  // namespace

namespace CoreBridge
{

void Initialize()
{
  try
  {
    HeadlessInitialize();
  }
  catch (Exception & E)
  {
    throw std::runtime_error(Utf8(E.Message));
  }
}

void Finalize()
{
  HeadlessFinalize();
}

TInfo Info()
{
  TInfo Result;
  Result.AppName = Utf8(AppNameString());
  Result.Version = Utf8(Configuration->Version);
  Result.ConfigurationStorage = Utf8((Configuration->Storage == stRegistry) ?
    (L"HKCU\\" + Configuration->RegistryStorageKey) : Configuration->IniFileStorageName);
  return Result;
}

}
