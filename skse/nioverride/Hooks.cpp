#include "skse/SafeWrite.h"
#include "skse/PluginAPI.h"
#include "skse/PapyrusVM.h"

#include "skse/GameData.h"
#include "skse/GameRTTI.h"

#include "skse/NiExtraData.h"
#include "skse/NiNodes.h"
#include "skse/NiRTTI.h"
#include "skse/NiNodes.h"
#include "skse/NiMaterial.h"
#include "skse/NiProperties.h"

#include "Hooks.h"
#include "PapyrusNiOverride.h"

#include "interfaces/OverlayInterface.h"
#include "interfaces/OverrideInterface.h"
#include "interfaces/BodyMorphInterface.h"
#include "interfaces/TintMaskInterface.h"
#include "interfaces/ItemDataInterface.h"

#include "SkeletonExtender.h"
#include "ShaderUtilities.h"

#include <vector>

extern SKSETaskInterface	* g_task;

extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern BodyMorphInterface	g_morphInterface;
extern OverlayInterface		g_overlayInterface;
extern OverrideInterface	g_overrideInterface;

extern bool					g_enableFaceOverlays;
extern UInt32				g_numFaceOverlays;

extern bool					g_playerOnly;
extern UInt32				g_numSpellFaceOverlays;

extern bool					g_immediateArmor;
extern bool					g_immediateFace;

extern bool					g_enableEquippableTransforms;

Actor						* g_weaponHookActor = NULL;
TESObjectWEAP				* g_weaponHookWeapon = NULL;
UInt32						g_firstPerson = 0;

typedef NiAVObject * (*_CreateWeaponNode)(UInt32 * unk1, UInt32 unk2, Actor * actor, UInt32 ** unk4, UInt32 * unk5);
extern const _CreateWeaponNode CreateWeaponNode = (_CreateWeaponNode)0x0046F530;

void __stdcall InstallWeaponHook(Actor * actor, TESObjectWEAP * weapon, NiAVObject * resultNode1, NiAVObject * resultNode2, UInt32 firstPerson)
{
	if (!actor) {
#ifdef _DEBUG
		_MESSAGE("%s - Error no reference found skipping overrides.", __FUNCTION__);
#endif
		return;
	}
	if (!weapon) {
#ifdef _DEBUG
		_MESSAGE("%s - Error no weapon found skipping overrides.", __FUNCTION__);
#endif
		return;
	}

	std::vector<TESObjectWEAP*> flattenedWeapons;
	flattenedWeapons.push_back(weapon);
	TESObjectWEAP * templateWeapon = weapon->templateForm;
	while (templateWeapon) {
		flattenedWeapons.push_back(templateWeapon);
		templateWeapon = templateWeapon->templateForm;
	}

	// Apply top-most parent properties first
	for (std::vector<TESObjectWEAP*>::reverse_iterator it = flattenedWeapons.rbegin(); it != flattenedWeapons.rend(); ++it)
	{
		if (resultNode1)
			g_overrideInterface.ApplyWeaponOverrides(actor, firstPerson == 1 ? true : false, weapon, resultNode1, true);
		if (resultNode2)
			g_overrideInterface.ApplyWeaponOverrides(actor, firstPerson == 1 ? true : false, weapon, resultNode2, true);
	}
}

// Store stack values here, they would otherwise be lost
enum
{
	kWeaponHook_EntryStackOffset1 = 0x40,
	kWeaponHook_EntryStackOffset2 = 0x20,
	kWeaponHook_VarObj = 0x04
};

static const UInt32 kInstallWeaponFPHook_Base = 0x0046F870 + 0x143;
static const UInt32 kInstallWeaponFPHook_Entry_retn = kInstallWeaponFPHook_Base + 0x5;

__declspec(naked) void CreateWeaponNodeFPHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, [esp + kWeaponHook_EntryStackOffset1 + kWeaponHook_VarObj + kWeaponHook_EntryStackOffset2]
		mov		g_weaponHookWeapon, eax
		mov		g_weaponHookActor, edi
		mov		g_firstPerson, 1
		popad

		call[CreateWeaponNode]

		mov		g_weaponHookWeapon, NULL
		mov		g_weaponHookActor, NULL

		jmp[kInstallWeaponFPHook_Entry_retn]
	}
}

static const UInt32 kInstallWeapon3PHook_Base = 0x0046F870 + 0x17E;
static const UInt32 kInstallWeapon3PHook_Entry_retn = kInstallWeapon3PHook_Base + 0x5;

__declspec(naked) void CreateWeaponNode3PHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, [esp + kWeaponHook_EntryStackOffset1 + kWeaponHook_VarObj + kWeaponHook_EntryStackOffset2]
		mov		g_weaponHookWeapon, eax
		mov		g_weaponHookActor, edi
		mov		g_firstPerson, 0
		popad

		call[CreateWeaponNode]

		mov		g_weaponHookWeapon, NULL
		mov		g_weaponHookActor, NULL

		jmp[kInstallWeapon3PHook_Entry_retn]
	}
}

// Recall stack values here
static const UInt32 kInstallWeaponHook_Base = 0x0046F530 + 0x28A;
static const UInt32 kInstallWeaponHook_Entry_retn = kInstallWeaponHook_Base + 0x5;

__declspec(naked) void InstallWeaponNodeHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, g_firstPerson
		push	eax
		push	ebp
		push	edx
		mov		eax, g_weaponHookWeapon
		push	eax
		mov		eax, g_weaponHookActor
		push	eax
		call	InstallWeaponHook
		popad

		push	ebx
		push	ecx
		push	edx
		push	ebp
		push	esi

		jmp[kInstallWeaponHook_Entry_retn]
	}
}

UInt8 * g_unk1 = (UInt8*)0x001240DE8;
UInt32 * g_unk2 = (UInt32*)0x001310588;

NiAVObject * ArmorAddonTree::CreateArmorNode(AddonTreeParameters * params, NiNode * unk1, UInt32 unk2, UInt32 unk3, UInt32 unk4, UInt32 unk5)
{
	NiNode * boneTree = this->unk04;
	NiAVObject * retVal = CALL_MEMBER_FN(this, CreateArmorNode)(unk1, unk2, unk3, unk4, unk5);

	try {
		TESObjectREFR * reference = nullptr;
		UInt32 handle = this->unkA88;
		LookupREFRByHandle(&handle, &reference);
		if(reference)
			InstallArmorAddonHook(reference, params, boneTree, retVal);
	}
	catch (...) {
		_MESSAGE("%s - unexpected error occured when installing body overlays.", __FUNCTION__);
	}

	return retVal;
}

static const UInt32 kCreateArmorNode = GetFnAddr(&ArmorAddonTree::CreateArmorNode);
static const UInt32 kInstallArmorHook_Base = 0x00470050 + 0x902;
static const UInt32 kInstallArmorHook_Entry_retn = kInstallArmorHook_Base + 0x5;

enum
{
	kArmorHook_EntryStackOffset = 0x04,
	kArmorHook_VarObj = 0x14
};

__declspec(naked) void InstallArmorNodeHook_Entry(void)
{
	__asm
	{
		lea		eax, [ebx]
		push	eax
		call[kCreateArmorNode]

		jmp[kInstallArmorHook_Entry_retn]
	}
}


void __stdcall InstallArmorAddonHook(TESObjectREFR * refr, AddonTreeParameters * params, NiNode * boneTree, NiAVObject * resultNode)
{
	if (!refr) {
#ifdef _DEBUG
		_ERROR("%s - Error no reference found skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!params) {
#ifdef _DEBUG
		_ERROR("%s - Error no armor parameters found, skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!params->armor || !params->addon) {
#ifdef _DEBUG
		_ERROR("%s - Armor or ArmorAddon found, skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!boneTree) {
#ifdef _DEBUG
		_ERROR("%s - Error no bone tree found, skipping overlays.", __FUNCTION__);
#endif
		return;
	}
	if (!resultNode) {
#ifdef _DEBUG
		UInt32 addonFormid = params->addon ? params->addon->formID : 0;
		UInt32 armorFormid = params->armor ? params->armor->formID : 0;
		_ERROR("%s - Error no node found on Reference (%08X) while attaching ArmorAddon (%08X) of Armor (%08X)", __FUNCTION__, refr->formID, addonFormid, armorFormid);
#endif
		return;
	}

	NiNode * node3P = refr->GetNiRootNode(0);
	NiNode * node1P = refr->GetNiRootNode(1);

	// Go up to the root and see which one it is
	NiNode * rootNode = nullptr;
	NiNode * parent = boneTree->m_parent;
	do
	{
		if (parent == node1P)
			rootNode = node1P;
		if (parent == node3P)
			rootNode = node3P;
		parent = parent->m_parent;
	} while (parent);

	bool isFirstPerson = (rootNode == node1P);
	if (node1P == node3P) { // Theres only one node, theyre the same, no 1st person
		isFirstPerson = false;
	}

	if (rootNode != node1P && rootNode != node3P) {
#ifdef _DEBUG
		_DMESSAGE("%s - Mismatching root nodes, bone tree not for this reference (%08X)", __FUNCTION__, refr->formID);
#endif
		return;
	}

	SkeletonExtender::Attach(refr, boneTree, resultNode);

#ifdef _DEBUG
	_ERROR("%s - Applying Vertex Diffs on Reference (%08X) ArmorAddon (%08X) of Armor (%08X)", __FUNCTION__, refr->formID, params->addon->formID, params->armor->formID);
#endif

	// Apply no v-diffs if theres no morphs at all
	if (g_morphInterface.HasMorphs(refr)) {
		NiAutoRefCounter rf(resultNode);
		g_morphInterface.ApplyVertexDiff(refr, resultNode, true);
	}

	
	if (g_enableEquippableTransforms)
	{
		NiAutoRefCounter rf(resultNode);
		SkeletonExtender::AddTransforms(refr, isFirstPerson, isFirstPerson ? node1P : node3P, resultNode);
	}

	if ((refr == (*g_thePlayer) && g_playerOnly) || !g_playerOnly || g_overlayInterface.HasOverlays(refr))
	{
		UInt32 armorMask = params->armor->bipedObject.GetSlotMask();
		UInt32 addonMask = params->addon->biped.GetSlotMask();
		g_overlayInterface.BuildOverlays(armorMask, addonMask, refr, boneTree, resultNode);
	}

	{
		NiAutoRefCounter rf(resultNode);
		g_overrideInterface.ApplyOverrides(refr, params->armor, params->addon, resultNode, g_immediateArmor);
	}

	{
		UInt32 armorMask = params->armor->bipedObject.GetSlotMask();
		UInt32 addonMask = params->addon->biped.GetSlotMask();
		NiAutoRefCounter rf(resultNode);
		g_overrideInterface.ApplySkinOverrides(refr, isFirstPerson, params->armor, params->addon, armorMask & addonMask, resultNode, g_immediateArmor);
	}

	UInt32 armorMask = params->armor->bipedObject.GetSlotMask();
	std::function<void(ColorMap*)> overrideFunc = [=](ColorMap* colorMap)
	{
		Actor * actor = DYNAMIC_CAST(refr, TESForm, Actor);
		if (actor) {
			ModifiedItemIdentifier identifier;
			identifier.SetSlotMask(armorMask);
			ItemAttributeData * data = g_itemDataInterface.GetExistingData(actor, identifier);
			if (data) {
				if (data->m_tintData) {
					*colorMap = data->m_tintData->m_colorMap;
				}
			}
		}
	};

	g_task->AddTask(new NIOVTaskDeferredMask(refr, isFirstPerson, params->armor, params->addon, resultNode, overrideFunc));
}

void __stdcall InstallFaceOverlayHook(TESObjectREFR* refr, bool attemptUninstall, bool immediate)
{
	if (!refr) {
#ifdef _DEBUG
		_DMESSAGE("%s - Warning no reference found skipping overlay", __FUNCTION__);
#endif
		return;
	}

	if (!refr->GetFaceGenNiNode()) {
#ifdef _DEBUG
		_DMESSAGE("%s - Warning no head node for %08X skipping overlay", __FUNCTION__, refr->formID);
#endif
		return;
	}

#ifdef _DEBUG
	_DMESSAGE("%s - Attempting to install face overlay to %08X - Flags %08X", __FUNCTION__, refr->formID, refr->GetFaceGenNiNode()->m_flags);
#endif

	if ((refr == (*g_thePlayer) && g_playerOnly) || !g_playerOnly || g_overlayInterface.HasOverlays(refr))
	{
		char buff[MAX_PATH];
		// Face
		for (UInt32 i = 0; i < g_numFaceOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FACE_NODE, i);
			if (attemptUninstall) {
				SKSETaskUninstallOverlay * task = new SKSETaskUninstallOverlay(refr, buff);
				if (immediate) {
					task->Run();
					task->Dispose();
				}
				else {
					g_task->AddTask(task);
				}
			}
			SKSETaskInstallFaceOverlay * task = new SKSETaskInstallFaceOverlay(refr, buff, FACE_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen);
			if (immediate) {
				task->Run();
				task->Dispose();
			}
			else {
				g_task->AddTask(task);
			}
		}
		for (UInt32 i = 0; i < g_numSpellFaceOverlays; i++)
		{
			memset(buff, 0, MAX_PATH);
			sprintf_s(buff, MAX_PATH, FACE_NODE_SPELL, i);
			if (attemptUninstall) {
				SKSETaskUninstallOverlay * task = new SKSETaskUninstallOverlay(refr, buff);
				if (immediate) {
					task->Run();
					task->Dispose();
				}
				else {
					g_task->AddTask(task);
				}
			}
			SKSETaskInstallFaceOverlay * task = new SKSETaskInstallFaceOverlay(refr, buff, FACE_MAGIC_MESH, BGSHeadPart::kTypeFace, BSShaderMaterial::kShaderType_FaceGen);
			if (immediate) {
				task->Run();
				task->Dispose();
			}
			else {
				g_task->AddTask(task);
			}
		}
	}
}

void ExtraContainerChangesData_Hooked::TransferItemUID_Hooked(BaseExtraList * extraList, TESForm * oldForm, TESForm * newForm, UInt32 unk1)
{
	CALL_MEMBER_FN(this, TransferItemUID)(extraList, oldForm, newForm, unk1);

	if (extraList) {
		if (extraList->HasType(kExtraData_Rank) && !extraList->HasType(kExtraData_UniqueID)) {
			CALL_MEMBER_FN(this, SetUniqueID)(extraList, oldForm, newForm);
			ExtraRank * rank = static_cast<ExtraRank*>(extraList->GetByType(kExtraData_Rank));
			ExtraUniqueID * uniqueId = static_cast<ExtraUniqueID*>(extraList->GetByType(kExtraData_UniqueID));
			if (rank && uniqueId) {
				// Re-assign mapping
				g_itemDataInterface.UpdateUIDByRank(rank->rank, uniqueId->uniqueId, uniqueId->ownerFormId);
			}
		}
	}
}

bool BSLightingShaderProperty_Hooked::HasFlags_Hooked(UInt32 flags)
{
	bool ret = CALL_MEMBER_FN(this, HasFlags)(flags);
	if (material) {
		if (material->GetShaderType() == BSShaderMaterial::kShaderType_FaceGen) {
			NiExtraData * tintData = GetExtraData("TINT");
			if (tintData) {
				NiBooleanExtraData * boolData = ni_cast(tintData, NiBooleanExtraData);
				if (boolData) {
					return boolData->m_data;
				}
			}

			return false;
		}
	}

	return ret;
}

SInt32 TESNPC_Hooked::CreateHeadState_Hooked(Actor * actor, UInt32 unk1)
{
	SInt32 ret = CALL_MEMBER_FN(this, UpdateHeadState)(actor, unk1);
	try {
		__asm {
			pushad
			mov		al, g_immediateFace
			push	eax
			push	1
			mov		eax, actor
			push	eax
			call	InstallFaceOverlayHook
			popad
		}
	}
	catch (...) {
		if (actor)
			_MESSAGE("%s - unexpected error while updating face overlay for actor %08X", __FUNCTION__, actor->formID);
	}
	return ret;
}

SInt32 TESNPC_Hooked::UpdateHeadState_Hooked(Actor * actor, UInt32 unk1)
{
	SInt32 ret = CALL_MEMBER_FN(this, UpdateHeadState)(actor, unk1);
	try {
		__asm {
			pushad
			mov		al, g_immediateFace
			push	eax
			push	0
			mov		eax, actor
			push	eax
			call	InstallFaceOverlayHook
			popad
		}
	}
	catch (...) {
		if (actor)
			_MESSAGE("%s - unexpected error while updating face overlay for actor %08X", __FUNCTION__, actor->formID);
	}
	return ret;
}

class UniqueIDEventHandler : public BSTEventSink <TESUniqueIDChangeEvent>
{
public:
	virtual	EventResult		ReceiveEvent(TESUniqueIDChangeEvent * evn, EventDispatcher<TESUniqueIDChangeEvent> * dispatcher)
	{
		if (evn->oldOwnerFormId != 0) {
			g_itemDataInterface.UpdateUID(evn->oldUniqueId, evn->oldOwnerFormId, evn->newUniqueId, evn->newOwnerFormId);
		}
		if (evn->newOwnerFormId == 0) {
			g_itemDataInterface.EraseByUID(evn->oldUniqueId, evn->oldOwnerFormId);
		}
		return EventResult::kEvent_Continue;
	}
};

UniqueIDEventHandler			g_uniqueIdEventSink;

void RegisterPapyrusFunctions(VMClassRegistry * registry)
{
	papyrusNiOverride::RegisterFuncs(registry);
}

void SkyrimVM_Hooked::RegisterEventSinks_Hooked()
{
	RegisterPapyrusFunctions(GetClassRegistry());

	CALL_MEMBER_FN(this, RegisterEventSinks)();

	if (g_changeUniqueIDEventDispatcher)
		g_changeUniqueIDEventDispatcher->AddEventSink(&g_uniqueIdEventSink);

	g_tintMaskInterface.ReadTintData("Data\\SKSE\\Plugins\\NiOverride\\TintData\\", "*.xml");
}

void InstallHooks()
{
	WriteRelJump(kInstallArmorHook_Base, (UInt32)&InstallArmorNodeHook_Entry);

	if (g_enableFaceOverlays) {
		WriteRelJump(0x00569990 + 0x16E, GetFnAddr(&TESNPC_Hooked::CreateHeadState_Hooked)); // Creates new head
		WriteRelCall(0x0056AFC0 + 0x2E5, GetFnAddr(&TESNPC_Hooked::UpdateHeadState_Hooked)); // Updates head state
	}

	WriteRelCall(0x0046C310 + 0x110, GetFnAddr(&BSLightingShaderProperty_Hooked::HasFlags_Hooked));

	// Make the ExtraRank item unique
	WriteRelCall(0x00482120 + 0x5DD, GetFnAddr(&ExtraContainerChangesData_Hooked::TransferItemUID_Hooked));

	// Closest and easiest hook to Payprus Native Function Registry without a detour
	WriteRelCall(0x008D7A40 + 0x995, GetFnAddr(&SkyrimVM_Hooked::RegisterEventSinks_Hooked));

	WriteRelJump(kInstallWeaponFPHook_Base, (UInt32)&CreateWeaponNodeFPHook_Entry);
	WriteRelJump(kInstallWeapon3PHook_Base, (UInt32)&CreateWeaponNode3PHook_Entry);
	WriteRelJump(kInstallWeaponHook_Base, (UInt32)&InstallWeaponNodeHook_Entry);
}