#include "vcl_containers.h"

#include <algorithm>

void TList::CheckIndex(int Index) const
{
  if (Index < 0 || Index >= GetCount())
  {
    throw EListError(UnicodeString(L"List index out of bounds"));
  }
}

void * TList::GetItem(int Index) const
{
  CheckIndex(Index);
  return FItems[static_cast<std::size_t>(Index)];
}

void TList::PutItem(int Index, void * Item)
{
  CheckIndex(Index);
  FItems[static_cast<std::size_t>(Index)] = Item;
}

void TList::SetCapacity(int Value)
{
  if (Value > GetCount())
  {
    FItems.reserve(static_cast<std::size_t>(Value));
  }
}

void TList::Notify(void * Ptr, TListNotification Action)
{
  (void)Ptr;
  (void)Action;
}

int TList::Add(void * Item)
{
  FItems.push_back(Item);
  const int Index = GetCount() - 1;
  Notify(Item, lnAdded);
  return Index;
}

void TList::Insert(int Index, void * Item)
{
  if (Index < 0 || Index > GetCount())
  {
    throw EListError(UnicodeString(L"List index out of bounds"));
  }
  FItems.insert(FItems.begin() + Index, Item);
  Notify(Item, lnAdded);
}

void TList::Delete(int Index)
{
  CheckIndex(Index);
  void * Item = FItems[static_cast<std::size_t>(Index)];
  // Notify before erasing, so a handler can still inspect the item.
  Notify(Item, lnDeleted);
  FItems.erase(FItems.begin() + Index);
}

void TList::Clear()
{
  // Back to front, so a handler that itself removes items cannot invalidate
  // the index we are about to visit.
  while (!FItems.empty())
  {
    Delete(GetCount() - 1);
  }
  FItems.clear();
}

int TList::IndexOf(void * Item) const
{
  const auto It = std::find(FItems.begin(), FItems.end(), Item);
  return It == FItems.end()
         ? -1 : static_cast<int>(std::distance(FItems.begin(), It));
}

int TList::Remove(void * Item)
{
  const int Index = IndexOf(Item);
  if (Index >= 0)
  {
    Delete(Index);
  }
  return Index;
}

void * TList::Extract(void * Item)
{
  const int Index = IndexOf(Item);
  if (Index < 0)
  {
    return nullptr;
  }
  // lnExtracted rather than lnDeleted: the caller is taking ownership, so
  // TObjectList must not free it.
  Notify(Item, lnExtracted);
  FItems.erase(FItems.begin() + Index);
  return Item;
}

void TList::SortList(TListSortCompareFunc * Compare)
{
  if (Compare == nullptr)
  {
    return;
  }
  Compare->AddRef();
  std::stable_sort(FItems.begin(), FItems.end(),
    [Compare](void * A, void * B) { return Compare->Invoke(A, B) < 0; });
  Compare->Release();
}

void TList::Sort(TListSortCompare Compare)
{
  if (Compare == nullptr)
  {
    return;
  }
  // Stable, so that items the comparison calls equal keep their relative
  // order. Delphi's quicksort does not guarantee that, but relying on an
  // unstable order would be a latent bug in the caller either way, and stable
  // is the safer of the two to provide.
  std::stable_sort(FItems.begin(), FItems.end(),
    [Compare](void * A, void * B) { return Compare(A, B) < 0; });
}

void TList::Move(int CurIndex, int NewIndex)
{
  if (CurIndex == NewIndex)
  {
    return;
  }
  CheckIndex(CurIndex);
  CheckIndex(NewIndex);
  void * Item = FItems[static_cast<std::size_t>(CurIndex)];
  FItems.erase(FItems.begin() + CurIndex);
  FItems.insert(FItems.begin() + NewIndex, Item);
}

void TList::Exchange(int Index1, int Index2)
{
  CheckIndex(Index1);
  CheckIndex(Index2);
  std::swap(FItems[static_cast<std::size_t>(Index1)],
            FItems[static_cast<std::size_t>(Index2)]);
}

void * TList::First() const
{
  return GetItem(0);
}

void * TList::Last() const
{
  return GetItem(GetCount() - 1);
}

// --- TObjectList -----------------------------------------------------------

TObjectList::~TObjectList()
{
  // Clear() runs Notify, which frees the objects when owned.
  Clear();
}

void TObjectList::Notify(void * Ptr, TListNotification Action)
{
  if (FOwnsObjects && Action == lnDeleted && Ptr != nullptr)
  {
    delete static_cast<TObject *>(Ptr);
  }
  TList::Notify(Ptr, Action);
}
