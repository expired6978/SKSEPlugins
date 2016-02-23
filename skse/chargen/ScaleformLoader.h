#pragma once

#include "skse/ScaleformState.h"

class NiTexture;

class GFxImageLoader : public GFxState
{
public:
};

class BSScaleformImageLoader : public GFxImageLoader
{
public:
	virtual ~BSScaleformImageLoader();
	virtual UInt32	Unk_01(UInt32 unk1);

	MEMBER_FN_PREFIX(BSScaleformImageLoader);
	DEFINE_MEMBER_FN(AddVirtualImage, UInt8, 0x00A65710, NiTexture ** texture);
	DEFINE_MEMBER_FN(ReleaseVirtualImage, UInt8, 0x00A65A40, NiTexture ** texture);
};

class GFxLoader
{
public:
	UInt32			unk00;					// 00
	GFxStateBag		* stateBag;				// 04
	UInt32			unk08;					// 08
	UInt32			unk0C;					// 0C
	BSScaleformImageLoader	* imageLoader;	// 10

	static GFxLoader * GetSingleton();	
};
