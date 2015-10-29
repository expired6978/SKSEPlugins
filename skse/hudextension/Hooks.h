#pragma once

#include "skse/GameTypes.h"

class HUDMenu;

template<class T>
class BSTArrayFunctor
{
public:
	BSTArrayFunctor(tArray<T> * arr, UInt32 growSize = 10, UInt32 shrinkSize = 10) : m_array(arr)
	{
		m_growSize = growSize;
		if(m_growSize == 0)
			m_growSize = 1;

		m_shrinkSize = shrinkSize;
	}

	bool Allocate(UInt32 numEntries)
	{
		if(!m_array)
			return false;

		m_array->arr.entries = (T *)FormHeap_Allocate(sizeof(T) * numEntries);
		if(!m_array->arr.entries) return false;

		for(UInt32 i = 0; i < numEntries; i++)
			new (&m_array->arr.entries[i]) T;

		m_array->arr.capacity = numEntries;
		count = numEntries;
		return true;
	};

	bool Push(T & entry)
	{
		if(!m_array) return false;

		if(m_array->count + 1 > m_array->arr.capacity) {
			if(!Grow(m_growSize))
				return false;
		}

		m_array->arr.entries[m_array->count] = entry;
		m_array->count++;
		return true;
	};

	bool Insert(UInt32 index, T & entry)
	{
		if(!m_array->arr.entries || index < m_array->count)
			return false;

		m_array->arr.entries[index] = entry;
		return true;
	};

	bool Remove(UInt32 index)
	{
		if(!m_array->arr.entries || index < m_array->count)
			return false;

		m_array->arr.entries[index] = NULL;
		if(index + 1 < m_array->count) {
			UInt32 remaining = m_array->count - index;
			memmove_s(&m_array->arr.entries[index + 1], sizeof(T) * remaining, &m_array->arr.entries[index], sizeof(T) * remaining); // Move the rest up
		}
		m_array->count--;

		if(m_array->arr.capacity > m_array->count + m_shrinkSize)
			Shrink();

		return true;
	};

	bool Shrink()
	{
		if(!m_array || m_array->count == m_array->arr.capacity) return false;

		try {
			UInt32 newSize = m_array->count;
			T * oldArray = m_array->arr.entries;
			T * newArray = (T *)FormHeap_Allocate(sizeof(T) * newSize); // Allocate new block
			memmove_s(newArray, sizeof(T) * newSize, m_array->arr.entries, sizeof(T) * newSize); // Move the old block
			m_array->arr.entries = newArray;
			m_array->arr.capacity = m_array->count;
			FormHeap_Free(oldArray); // Free the old block
			return true;
		}
		catch(...) {
			return false;
		}

		return false;
	}

	bool Grow(UInt32 entries)
	{
		if(!m_array) return false;

		try {
			UInt32 oldSize = m_array->arr.capacity;
			UInt32 newSize = oldSize + entries;
			T * oldArray = m_array->arr.entries;
			T * newArray = (T *)FormHeap_Allocate(sizeof(T) * m_array->arr.capacity + entries); // Allocate new block
			memmove_s(newArray, sizeof(T) * newSize, m_array->arr.entries, sizeof(T) * m_array->arr.capacity); // Move the old block
			m_array->arr.entries = newArray;
			m_array->arr.capacity = newSize;
			FormHeap_Free(oldArray); // Free the old block

			for(UInt32 i = oldSize; i < newSize; i++) // Allocate the rest of the free blocks
				new (&m_array->arr.entries[i]) T;

			return true;
		}
		catch(...) {
			return false;
		}

		return false;
	};

private:
	tArray<T> * m_array;
	UInt32	m_growSize;
	UInt32	m_shrinkSize;
};

extern const UInt32 kInstallRaceMenuHook_Base;
extern const UInt32 kInstallRaceMenuHook_Entry_retn;

void __stdcall InstallHudComponents(HUDMenu * menu);
void InstallRaceMenuHook_Entry(void);

typedef void (* _Fn85FD10)();
extern const _Fn85FD10 Fn85FD10;