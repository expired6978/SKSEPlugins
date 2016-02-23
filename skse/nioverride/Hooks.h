#pragma once

class TESObjectARMO;
class TESObjectARMA;
class TESModelTextureSwap;
class BGSTextureSet;

#include "common/IErrors.h"
#include "skse/GameTypes.h"
#include "skse/GameExtraData.h"

class AddonTreeParameters
{
public:
	UInt32	unk00;					// 00 - ?
	UInt32	unk04;					// 04 - inited to zero
	TESObjectARMO * armor;			// 08
	TESObjectARMA * addon;			// 0C
	TESModelTextureSwap * model;	// 10
	BGSTextureSet * textureSet;		// 14
};

class ArmorAddonTree
{
public:
	MEMBER_FN_PREFIX(ArmorAddonTree);
	DEFINE_MEMBER_FN(CreateArmorNode, NiAVObject *, 0x0046F0B0, NiNode * unk1, UInt32 unk2, UInt8 unk3, UInt32 unk4, UInt32 unk5);

	UInt32	unk00;			// 00 refcount?
	NiNode * unk04;			// 04 FlattenedBoneTree
	TESObjectARMO * skin;	// 08
	UInt32	unk0C[(0xA88 - 0x0C) >> 2];	// 0C
	UInt32	unkA88;			// A88

	//NiAVObject * CreateArmorNode_Hooked(NiNode * unk1, UInt32 unk2, UInt32 unk3, UInt32 unk4, UInt32 unk5);
	NiAVObject * CreateArmorNode(AddonTreeParameters * params, NiNode * unk1, UInt32 unk2, UInt32 unk3, UInt32 unk4, UInt32 unk5);
};
STATIC_ASSERT(offsetof(ArmorAddonTree, unkA88) == 0xA88);

class SkyrimVM_Hooked : public SkyrimVM
{
public:
	MEMBER_FN_PREFIX(SkyrimVM_Hooked);
	DEFINE_MEMBER_FN(RegisterEventSinks, void, 0x008D2120);

	void RegisterEventSinks_Hooked();
};

class TESNPC_Hooked : public TESNPC
{
public:
	MEMBER_FN_PREFIX(TESNPC_Hooked);
	DEFINE_MEMBER_FN(BindHead, void, 0x00569990, Actor*, BSFaceGenNiNode**);
	//DEFINE_MEMBER_FN(GetFaceGenHead, UInt32, 0x0056AEB0, UInt32 unk1, UInt32 unk2);
	DEFINE_MEMBER_FN(UpdateHeadState, SInt32, 0x00566490, Actor *, UInt32 unk1);

	void BindHead_Hooked(Actor* actor, BSFaceGenNiNode** headNode);
	//UInt32 GetFaceGenHead_Hooked(TESObjectREFR* refr, UInt32 unk1, UInt32 unk2);
	SInt32 CreateHeadState_Hooked(Actor *, UInt32 unk1);
	SInt32 UpdateHeadState_Hooked(Actor *, UInt32 unk1);
};

class ExtraContainerChangesData_Hooked : public ExtraContainerChanges::Data
{
public:
	MEMBER_FN_PREFIX(ExtraContainerChangesData_Hooked);
	DEFINE_MEMBER_FN(TransferItemUID, void, 0x004819C0, BaseExtraList * extraList, TESForm * oldForm, TESForm * newForm, UInt32 unk1);

	void TransferItemUID_Hooked(BaseExtraList * extraList, TESForm * oldForm, TESForm * newForm, UInt32 unk1);
};

class BSLightingShaderProperty_Hooked : public BSLightingShaderProperty
{
public:
	MEMBER_FN_PREFIX(BSLightingShaderProperty_Hooked);

	bool HasFlags_Hooked(UInt32 flags);
};

void __stdcall InstallArmorAddonHook(TESObjectREFR * refr, AddonTreeParameters * params, NiNode * boneTree, NiAVObject * resultNode);
void __stdcall InstallFaceOverlayHook(TESObjectREFR* refr, bool attemptUninstall, bool immediate);

void InstallHooks();