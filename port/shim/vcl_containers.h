//---------------------------------------------------------------------------
// TList and TObjectList.
//
// Core derives from both -- TNamedObjectList extends TObjectList -- and
// overrides Notify, so that hook must be virtual and must fire at the moments
// Delphi fires it. A Notify that is never called would leave core's
// bookkeeping silently out of step with the list.
//
// Indexing is 0-based, like TStrings.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_CONTAINERS_H
#define LINSCP_VCL_CONTAINERS_H

#include <vector>

#include "vcl_object.h"
#include "vcl_property.h"

// Why Notify is being called. Delphi's names.
enum TListNotification { lnAdded, lnExtracted, lnDeleted };

// Delphi's comparison callback: negative, zero or positive, like strcmp.
using TListSortCompare = int (*)(void * Item1, void * Item2);

class TList : public TObject
{
public:
  ~TList() override = default;

  int GetCount() const { return static_cast<int>(FItems.size()); }
  PROP_R_G(int, Count, GetCount);

  void * GetItem(int Index) const;
  void PutItem(int Index, void * Item);
  PROP_IDX_RW_GS(void *, Items, int, GetItem, PutItem);

  int GetCapacity() const { return static_cast<int>(FItems.capacity()); }
  void SetCapacity(int Value);
  PROP_RW_GS(int, Capacity, GetCapacity, SetCapacity);

  virtual int Add(void * Item);
  virtual void Insert(int Index, void * Item);
  virtual void Delete(int Index);
  virtual void Clear();
  int Remove(void * Item);
  int IndexOf(void * Item) const;
  void * Extract(void * Item);
  void Sort(TListSortCompare Compare);
  void Move(int CurIndex, int NewIndex);
  void Exchange(int Index1, int Index2);
  void * First() const;
  void * Last() const;

protected:
  // Overridden by core. Called after an addition and before a removal, which is
  // Delphi's ordering: a handler for lnDeleted can still see the item.
  virtual void Notify(void * Ptr, TListNotification Action);
  void CheckIndex(int Index) const;

  std::vector<void *> FItems;
};

//---------------------------------------------------------------------------
// A TList of objects which, unlike TStrings, owns them by default.
//---------------------------------------------------------------------------
class TObjectList : public TList
{
public:
  TObjectList() = default;
  explicit TObjectList(bool AOwnsObjects) : FOwnsObjects(AOwnsObjects) {}
  ~TObjectList() override;

  TObject * GetObject(int Index) const
    { return static_cast<TObject *>(GetItem(Index)); }
  void PutObject(int Index, TObject * Item) { PutItem(Index, Item); }
  PROP_IDX_RW_GS(TObject *, Objects, int, GetObject, PutObject);

  PROP_RW_FF(bool, OwnsObjects, FOwnsObjects);

  int Add(TObject * Item) { return TList::Add(Item); }
  int IndexOf(TObject * Item) const { return TList::IndexOf(Item); }
  int Remove(TObject * Item) { return TList::Remove(Item); }
  TObject * Extract(TObject * Item)
    { return static_cast<TObject *>(TList::Extract(Item)); }

protected:
  // Frees the object on removal when OwnsObjects is set. Extract deliberately
  // reports lnExtracted instead, which is how Delphi lets a caller take
  // ownership back without the list freeing it.
  void Notify(void * Ptr, TListNotification Action) override;

private:
  bool FOwnsObjects = true;
};

#endif // LINSCP_VCL_CONTAINERS_H
