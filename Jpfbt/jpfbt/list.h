#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Linked list manipulation functions
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Note: The following functions are taken from winddk.h of the
 * ReactOS project.
 *
 */

static __inline VOID
InitializeListHead(
  IN PLIST_ENTRY  ListHead)
{
  ListHead->Flink = ListHead->Blink = ListHead;
}

static __inline VOID
InsertHeadList(
  IN PLIST_ENTRY  ListHead,
  IN PLIST_ENTRY  Entry)
{
  PLIST_ENTRY OldFlink;
  OldFlink = ListHead->Flink;
  Entry->Flink = OldFlink;
  Entry->Blink = ListHead;
  OldFlink->Blink = Entry;
  ListHead->Flink = Entry;
}

static __inline VOID
InsertTailList(
  IN PLIST_ENTRY  ListHead,
  IN PLIST_ENTRY  Entry)
{
  PLIST_ENTRY OldBlink;
  OldBlink = ListHead->Blink;
  Entry->Flink = ListHead;
  Entry->Blink = OldBlink;
  OldBlink->Flink = Entry;
  ListHead->Blink = Entry;
}

/*
 * BOOLEAN
 * IsListEmpty(
 *   IN PLIST_ENTRY  ListHead)
 */
#define IsListEmpty(_ListHead) \
  ((_ListHead)->Flink == (_ListHead))

/*
 * PSINGLE_LIST_ENTRY
 * PopEntryList(
 *   IN PSINGLE_LIST_ENTRY  ListHead)
 */
#define PopEntryList(ListHead) \
  (ListHead)->Next; \
  { \
    PSINGLE_LIST_ENTRY _FirstEntry; \
    _FirstEntry = (ListHead)->Next; \
    if (_FirstEntry != NULL) \
      (ListHead)->Next = _FirstEntry->Next; \
  }

/*
 * VOID
 * PushEntryList(
 *   IN PSINGLE_LIST_ENTRY  ListHead,
 *   IN PSINGLE_LIST_ENTRY  Entry)
 */
#define PushEntryList(_ListHead, _Entry) \
	(_Entry)->Next = (_ListHead)->Next; \
	(_ListHead)->Next = (_Entry); \


static __inline BOOL
RemoveEntryList(
  IN PLIST_ENTRY  Entry)
{
  PLIST_ENTRY OldFlink;
  PLIST_ENTRY OldBlink;

  OldFlink = Entry->Flink;
  OldBlink = Entry->Blink;
  OldFlink->Blink = OldBlink;
  OldBlink->Flink = OldFlink;
  return (OldFlink == OldBlink);
}

static __inline PLIST_ENTRY
RemoveHeadList(
  IN PLIST_ENTRY  ListHead)
{
  PLIST_ENTRY Flink;
  PLIST_ENTRY Entry;

  Entry = ListHead->Flink;
  Flink = Entry->Flink;
  ListHead->Flink = Flink;
  Flink->Blink = ListHead;
  return Entry;
}

static __inline PLIST_ENTRY
RemoveTailList(
  IN PLIST_ENTRY  ListHead)
{
  PLIST_ENTRY Blink;
  PLIST_ENTRY Entry;

  Entry = ListHead->Blink;
  Blink = Entry->Blink;
  ListHead->Blink = Blink;
  Blink->Flink = ListHead;
  return Entry;
}
