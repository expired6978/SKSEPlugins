#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/SafeWrite.h"

#include "skse/GameAPI.h"
#include "skse/GameObjects.h"
#include "skse/GameRTTI.h"
#include "skse/GameData.h"
#include "skse/GameEvents.h"

#include "skse/PapyrusVM.h"

#include "skse/NiRTTI.h"
#include "skse/NiNodes.h"
#include "skse/NiMaterial.h"
#include "skse/NiProperties.h"

#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"

#include "interfaces/OverrideInterface.h"
#include "interfaces/OverlayInterface.h"
#include "interfaces/BodyMorphInterface.h"
#include "interfaces/ItemDataInterface.h"
#include "interfaces/TintMaskInterface.h"
#include "interfaces/NiTransformInterface.h"

#include "ShaderUtilities.h"
#include "ScaleformFunctions.h"
#include "StringTable.h"

#include <shlobj.h>
#include <string>
#include <chrono>

#include "PapyrusNiOverride.h"
#include "Hooks.h"

IDebugLog	gLog;

PluginHandle					g_pluginHandle = kPluginHandle_Invalid;
const UInt32					kSerializationDataVersion = 1;

// Interfaces
SKSESerializationInterface		* g_serialization = NULL;
SKSEScaleformInterface			* g_scaleform = NULL;
SKSETaskInterface				* g_task = NULL;
SKSEMessagingInterface			* g_messaging = NULL;

// Handlers
IInterfaceMap				g_interfaceMap;
DyeMap						g_dyeMap;
OverrideInterface			g_overrideInterface;
TintMaskInterface			g_tintMaskInterface;
OverlayInterface			g_overlayInterface;
BodyMorphInterface			g_morphInterface;
ItemDataInterface			g_itemDataInterface;
NiTransformInterface		g_transformInterface;

StringTable g_stringTable;

UInt32	g_numBodyOverlays = 3;
UInt32	g_numHandOverlays = 3;
UInt32	g_numFeetOverlays = 3;
UInt32	g_numFaceOverlays = 3;

bool	g_playerOnly = true;
UInt32	g_numSpellBodyOverlays = 1;
UInt32	g_numSpellHandOverlays = 1;
UInt32	g_numSpellFeetOverlays = 1;
UInt32	g_numSpellFaceOverlays = 1;

bool	g_alphaOverride = true;
UInt16	g_alphaFlags = 4845;
UInt16	g_alphaThreshold = 0;

UInt16	g_loadMode = 0;
bool	g_enableAutoTransforms = true;
bool	g_enableBodyGen = true;
bool	g_enableBodyInit = true;
bool	g_firstLoad = false;
bool	g_immediateArmor = true;
bool	g_enableFaceOverlays = true;
bool	g_immediateFace = false;
bool	g_enableEquippableTransforms = true;
bool	g_parallelMorphing = true;
UInt16	g_scaleMode = 0;
UInt16	g_bodyMorphMode = 0;

#define MIN_SERIALIZATION_VERSION	2
#define MIN_TASK_VERSION			1
#define MIN_SCALEFORM_VERSION		1


const std::string & GetRuntimeDirectory(void)
{
	static std::string s_runtimeDirectory;

	if(s_runtimeDirectory.empty())
	{
		// can't determine how many bytes we'll need, hope it's not more than MAX_PATH
		char	runtimePathBuf[MAX_PATH];
		UInt32	runtimePathLength = GetModuleFileName(GetModuleHandle(NULL), runtimePathBuf, sizeof(runtimePathBuf));

		if(runtimePathLength && (runtimePathLength < sizeof(runtimePathBuf)))
		{
			std::string	runtimePath(runtimePathBuf, runtimePathLength);

			// truncate at last slash
			std::string::size_type	lastSlash = runtimePath.rfind('\\');
			if(lastSlash != std::string::npos)	// if we don't find a slash something is VERY WRONG
			{
				s_runtimeDirectory = runtimePath.substr(0, lastSlash + 1);

				_DMESSAGE("runtime root = %s", s_runtimeDirectory.c_str());
			}
			else
			{
				_WARNING("no slash in runtime path? (%s)", runtimePath.c_str());
			}
		}
		else
		{
			_WARNING("couldn't find runtime path (len = %d, err = %08X)", runtimePathLength, GetLastError());
		}
	}

	return s_runtimeDirectory;
}

const std::string & GetConfigPath(void)
{
	static std::string s_configPath;

	if(s_configPath.empty())
	{
		std::string	runtimePath = GetRuntimeDirectory();
		if(!runtimePath.empty())
		{
			s_configPath = runtimePath + "Data\\SKSE\\Plugins\\nioverride.ini";

			_MESSAGE("config path = %s", s_configPath.c_str());
		}
	}

	return s_configPath;
}

std::string GetConfigOption(const char * section, const char * key)
{
	std::string	result;

	const std::string & configPath = GetConfigPath();
	if(!configPath.empty())
	{
		char	resultBuf[256];
		resultBuf[0] = 0;

		UInt32	resultLen = GetPrivateProfileString(section, key, NULL, resultBuf, sizeof(resultBuf), configPath.c_str());

		result = resultBuf;
	}

	return result;
}

bool GetConfigOption_SInt32(const char * section, const char * key, SInt32 * dataOut)
{
	std::string	data = GetConfigOption(section, key);
	if (data.empty())
		return false;

	return (sscanf_s(data.c_str(), "%d", dataOut) == 1);
}

bool GetConfigOption_UInt32(const char * section, const char * key, UInt32 * dataOut)
{
	std::string	data = GetConfigOption(section, key);
	if(data.empty())
		return false;

	return (sscanf_s(data.c_str(), "%d", dataOut) == 1);
}

#include "skse/GameExtraData.h"

// This event isnt hooked up SKSE end
void NIOVSerialization_FormDelete(UInt64 handle)
{
	g_overrideInterface.RemoveAllReferenceOverrides(handle);
	g_overrideInterface.RemoveAllReferenceNodeOverrides(handle);
}

void NIOVSerialization_Revert(SKSESerializationInterface * intfc)
{
//#ifndef _DEBUG
	_MESSAGE("Reverting...");

	g_overlayInterface.Revert();
	g_overrideInterface.Revert();
	g_morphInterface.Revert();
	g_itemDataInterface.Revert();
	g_dyeMap.Revert();
	g_transformInterface.Revert();
//#endif
}

class StopWatch
{
public:
	StopWatch()
	{
		
	}

	void Start()
	{
		start = std::chrono::system_clock::now();
	}

	long long Stop()
	{
		end = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	}

private:
	std::chrono::system_clock::time_point start;
	std::chrono::system_clock::time_point end;
};

void NIOVSerialization_Save(SKSESerializationInterface * intfc)
{
	_MESSAGE("Saving...");

	StopWatch sw;
	UInt32 strCount = 0;
	sw.Start();
	g_transformInterface.VisitStrings([&](BSFixedString str)
	{
		g_stringTable.StringToId(str.data, strCount);
		strCount++;
	});
	g_overrideInterface.VisitStrings([&](BSFixedString str)
	{
		g_stringTable.StringToId(str.data, strCount);
		strCount++;
	});
	g_morphInterface.VisitStrings([&](BSFixedString str)
	{
		g_stringTable.StringToId(str.data, strCount);
		strCount++;
	});

	_DMESSAGE("%s - Pooled strings %dms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_stringTable.Save(intfc, StringTable::kSerializationVersion);
	_DMESSAGE("%s - Serialized string table %dms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_transformInterface.Save(intfc, NiTransformInterface::kSerializationVersion);
	_DMESSAGE("%s - Serialized transforms %dms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_overlayInterface.Save(intfc, OverlayInterface::kSerializationVersion);
	_DMESSAGE("%s - Serialized overlays %dms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_overrideInterface.Save(intfc, OverrideInterface::kSerializationVersion);
	_DMESSAGE("%s - Serialized overrides %dms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_morphInterface.Save(intfc, BodyMorphInterface::kSerializationVersion);
	_DMESSAGE("%s - Serialized body morphs %dms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_itemDataInterface.Save(intfc, ItemDataInterface::kSerializationVersion);
	_DMESSAGE("%s - Serialized item data %dms", __FUNCTION__, sw.Stop());

	g_stringTable.Clear();
}

void NIOVSerialization_Load(SKSESerializationInterface * intfc)
{
	_MESSAGE("Loading...");

	UInt32 type, length, version;
	bool error = false;

	StopWatch sw;
	sw.Start();
	while (intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch (type)
		{
			case 'STTB':	g_stringTable.Load(intfc, version);							break;
			case 'AOVL':	g_overlayInterface.Load(intfc, version);					break;
			case 'ACEN':	g_overrideInterface.LoadOverrides(intfc, version);			break;
			case 'NDEN':	g_overrideInterface.LoadNodeOverrides(intfc, version);		break;
			case 'WPEN':	g_overrideInterface.LoadWeaponOverrides(intfc, version);	break;
			case 'SKNR':	g_overrideInterface.LoadSkinOverrides(intfc, version);		break;
			case 'MRPH':	g_morphInterface.Load(intfc, version);						break;
			case 'ITEE':	g_itemDataInterface.Load(intfc, version);					break;
			case 'ACTM':	g_transformInterface.Load(intfc, version);					break;
			default:
				_MESSAGE("unhandled type %08X (%.4s)", type, &type);
				error = true;
				break;
		}
	}
	_DMESSAGE("%s - Loaded %dms", __FUNCTION__, sw.Stop());

	g_stringTable.Clear();
	g_firstLoad = true;

#ifdef _DEBUG
	g_overrideInterface.DumpMap();
	g_overlayInterface.DumpMap();
#endif
}

bool RegisterScaleform(GFxMovieView * view, GFxValue * root)
{
	GFxValue obj;
	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numBodyOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellBodyOverlays);
	root->SetMember("body", &obj);

	obj.SetNull();
	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numHandOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellHandOverlays);
	root->SetMember("hand", &obj);

	obj.SetNull();
	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numFeetOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellFeetOverlays);
	root->SetMember("feet", &obj);

	obj.SetNull();
	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numFaceOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellFaceOverlays);
	root->SetMember("face", &obj);

	RegisterBool(root, "bPlayerOnly", g_playerOnly);

	RegisterFunction <SKSEScaleform_GetDyeItems>(root, view, "GetDyeItems");
	RegisterFunction <SKSEScaleform_GetDyeableItems>(root, view, "GetDyeableItems");
	RegisterFunction <SKSEScaleform_SetItemDyeColor>(root, view, "SetItemDyeColor");
	return true;
}

class SKSEObjectLoadSink : public BSTEventSink<TESObjectLoadedEvent>
{
public:
	virtual ~SKSEObjectLoadSink() {}	// todo?
	virtual	EventResult ReceiveEvent(TESObjectLoadedEvent * evn, EventDispatcher<TESObjectLoadedEvent> * dispatcher)
	{
		if (evn) {
			TESForm * form = LookupFormByID(evn->formId);
			if (form) {
				if (g_enableBodyGen && form->formType == Character::kTypeID) {
					TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
					if (reference) {
						if (!g_morphInterface.HasMorphs(reference)) {
							UInt32 total = g_morphInterface.EvaluateBodyMorphs(reference);
							if (total) {
								_DMESSAGE("%s - Applied %d morph(s) to %s", __FUNCTION__, total, CALL_MEMBER_FN(reference, GetReferenceName)());
								g_morphInterface.UpdateModelWeight(reference);
							}
						}
					}
				}

				if (g_enableAutoTransforms) {
					UInt64 handle = g_overrideInterface.GetHandle(form, TESObjectREFR::kTypeID);
					g_transformInterface.SetHandleNodeTransforms(handle);
				}
			}
		}
		return kEvent_Continue;
	};
};

SKSEObjectLoadSink	g_objectLoadSink;

class SKSEInitScriptSink : public BSTEventSink < TESInitScriptEvent >
{
public:
	virtual ~SKSEInitScriptSink() {}	// todo?
	virtual	EventResult ReceiveEvent(TESInitScriptEvent * evn, EventDispatcher<TESInitScriptEvent> * dispatcher)
	{
		if (evn) {
			TESObjectREFR * reference = evn->reference;
			if (reference && g_enableBodyInit) {
				if (reference->formType == Character::kTypeID) {
					if (!g_morphInterface.HasMorphs(reference)) {
						UInt32 total = g_morphInterface.EvaluateBodyMorphs(reference);
						if (total) {
							_DMESSAGE("%s - Applied %d morph(s) to %s", __FUNCTION__, total, CALL_MEMBER_FN(reference, GetReferenceName)());
						}
					}
				}
			}
		}
		return kEvent_Continue;
	};
};

SKSEInitScriptSink g_initScriptSink;

void SKSEMessageHandler(SKSEMessagingInterface::Message * message)
{
	switch (message->type)
	{
		case SKSEMessagingInterface::kMessage_InputLoaded:
		{
			if (g_enableAutoTransforms || g_enableBodyGen) {
				if (g_objectLoadedEventDispatcher)
					g_objectLoadedEventDispatcher->AddEventSink(&g_objectLoadSink);
			}
		}
		break;
		case SKSEMessagingInterface::kMessage_PreLoadGame:
			g_enableBodyInit = false;
			g_tintMaskInterface.ManageTints();
			break;
		case SKSEMessagingInterface::kMessage_PostLoadGame:
			g_enableBodyInit = true;
			g_tintMaskInterface.ReleaseTints();
			break;
		case SKSEMessagingInterface::kMessage_DataLoaded:
		{
			if (g_enableBodyGen) {
				// Add data sink after everything has loaded
				if (g_initScriptEventDispatcher)
					g_initScriptEventDispatcher->AddEventSink(&g_initScriptSink);

				DataHandler * dataHandler = DataHandler::GetSingleton();
				if (dataHandler)
				{
					std::string fixedPath = "Meshes\\" + std::string(MORPH_MOD_DIRECTORY);
					UInt8 modCount = dataHandler->modList.loadedModCount;
					for (UInt32 i = 0; i < modCount; i++)
					{
						ModInfo * modInfo = dataHandler->modList.loadedMods[i];
						std::string templatesPath = fixedPath + std::string(modInfo->name) + "\\templates.ini";
						g_morphInterface.ReadBodyMorphTemplates(templatesPath.c_str());
					}

					for (UInt32 i = 0; i < modCount; i++)
					{
						ModInfo * modInfo = dataHandler->modList.loadedMods[i];
						std::string morphsPath = fixedPath + std::string(modInfo->name) + "\\morphs.ini";
						g_morphInterface.ReadBodyMorphs(morphsPath.c_str());
					}
				}
			}
		}
		break;
	}
}



void InterfaceExchangeMessageHandler(SKSEMessagingInterface::Message * message)
{
	switch (message->type)
	{
		case InterfaceExchangeMessage::kMessage_ExchangeInterface:
		{
			InterfaceExchangeMessage * exchangeMessage = (InterfaceExchangeMessage*)message->data;
			exchangeMessage->interfaceMap = &g_interfaceMap;
		}
		break;
	}
}

extern "C"
{

bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
{
	SInt32	logLevel = IDebugLog::kLevel_DebugMessage;
	if (GetConfigOption_SInt32("Debug", "iLogLevel", &logLevel))
		gLog.SetLogLevel((IDebugLog::LogLevel)logLevel);

	if (logLevel >= 0)
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim\\SKSE\\skse_nioverride.log");

	_DMESSAGE("skse_nioverride");

	// populate info structure
	info->infoVersion =	PluginInfo::kInfoVersion;
	info->name =		"nioverride";
	info->version =		6;

	// store plugin handle so we can identify ourselves later
	g_pluginHandle = skse->GetPluginHandle();

	if(skse->isEditor)
	{
		_MESSAGE("loaded in editor, marking as incompatible");
		return false;
	}
	else if(skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0)
	{
		_FATALERROR("unsupported runtime version %08X", skse->runtimeVersion);
		return false;
	}

	// get the serialization interface and query its version
	g_serialization = (SKSESerializationInterface *)skse->QueryInterface(kInterface_Serialization);
	if(!g_serialization)
	{
		_FATALERROR("couldn't get serialization interface");
		return false;
	}
	if(g_serialization->version < MIN_SERIALIZATION_VERSION)//SKSESerializationInterface::kVersion)
	{
		_FATALERROR("serialization interface too old (%d expected %d)", g_serialization->version, MIN_SERIALIZATION_VERSION);
		return false;
	}

	// get the scaleform interface and query its version
	g_scaleform = (SKSEScaleformInterface *)skse->QueryInterface(kInterface_Scaleform);
	if(!g_scaleform)
	{
		_FATALERROR("couldn't get scaleform interface");
		return false;
	}
	if(g_scaleform->interfaceVersion < MIN_SCALEFORM_VERSION)
	{
		_FATALERROR("scaleform interface too old (%d expected %d)", g_scaleform->interfaceVersion, MIN_SCALEFORM_VERSION);
		return false;
	}

	// get the task interface and query its version
	g_task = (SKSETaskInterface *)skse->QueryInterface(kInterface_Task);
	if(!g_task)
	{
		_FATALERROR("couldn't get task interface");
		return false;
	}
	if(g_task->interfaceVersion < MIN_TASK_VERSION)//SKSETaskInterface::kInterfaceVersion)
	{
		_FATALERROR("task interface too old (%d expected %d)", g_task->interfaceVersion, MIN_TASK_VERSION);
		return false;
	}

	g_messaging = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
	if (!g_messaging) {
		_ERROR("couldn't get messaging interface");
	}

	// supported runtime version
	return true;
}

bool SKSEPlugin_Load(const SKSEInterface * skse)
{
	g_interfaceMap.AddInterface("Override", &g_overrideInterface);
	g_interfaceMap.AddInterface("Overlay", &g_overlayInterface);
	g_interfaceMap.AddInterface("NiTransform", &g_transformInterface);
	g_interfaceMap.AddInterface("BodyMorph", &g_morphInterface);
	g_interfaceMap.AddInterface("ItemData", &g_itemDataInterface);
	g_interfaceMap.AddInterface("TintMask", &g_tintMaskInterface);

	_DMESSAGE("NetImmerse Override Enabled");

	UInt32	playerOnly = 1;

	UInt32	numBodyOverlays = 3;
	UInt32	numHandOverlays = 3;
	UInt32	numFeetOverlays = 3;
	UInt32	numFaceOverlays = 3;
	
	UInt32	spellBodyOverlays = 1;
	UInt32	spellHandOverlays = 1;
	UInt32	spellFeetOverlays = 1;
	UInt32	spellFaceOverlays = 1;

	UInt32	alphaOverride = 0;
	UInt32	alphaFlags = 4845;
	UInt32	alphaThreshold = 0;

	UInt32	loadMode = 0;
	UInt32	immediateArmor = 1;
	UInt32	enableFaceOverlays = 1;
	UInt32	immediateFace = 0;
	UInt32	enableAutoTransforms = 1;
	UInt32	enableBodyGen = 1;
	UInt32	enableEquippableTransforms = 1;
	UInt32	scaleMode = 0;
	UInt32	parallelMorphing = 1;
	UInt32	bodyMorphMode = 0;

	if(GetConfigOption_UInt32("Overlays", "bPlayerOnly", &playerOnly))
	{
		g_playerOnly = (playerOnly > 0);
	}
	if(GetConfigOption_UInt32("Overlays", "bEnableFaceOverlays", &enableFaceOverlays))
	{
		g_enableFaceOverlays = (enableFaceOverlays > 0);
	}
	if(GetConfigOption_UInt32("Overlays", "bImmediateArmor", &immediateArmor))
	{
		g_immediateArmor = (immediateArmor > 0);
	}
	if(GetConfigOption_UInt32("Overlays", "bImmediateFace", &immediateFace))
	{
		g_immediateFace = (immediateFace > 0);
	}

	if(GetConfigOption_UInt32("Overlays/Body", "iNumOverlays", &numBodyOverlays))
	{
		g_numBodyOverlays = numBodyOverlays;
		if(g_numBodyOverlays > 0x7F)
			g_numBodyOverlays = 0x7F;
	}
	if(GetConfigOption_UInt32("Overlays/Body", "iSpellOverlays", &spellBodyOverlays))
	{
		g_numSpellBodyOverlays = spellBodyOverlays;
		if(g_numSpellBodyOverlays > 0x7F)
			g_numSpellBodyOverlays = 0x7F;
	}
	if(GetConfigOption_UInt32("Overlays/Hands", "iNumOverlays", &numHandOverlays))
	{
		g_numHandOverlays = numHandOverlays;
		if(g_numHandOverlays > 0x7F)
			g_numHandOverlays = 0x7F;
	}
	if(GetConfigOption_UInt32("Overlays/Hands", "iSpellOverlays", &spellHandOverlays))
	{
		g_numSpellHandOverlays = spellHandOverlays;
		if(g_numSpellHandOverlays > 0x7F)
			g_numSpellHandOverlays = 0x7F;
	}
	if(GetConfigOption_UInt32("Overlays/Feet", "iNumOverlays", &numFeetOverlays))
	{
		g_numFeetOverlays = numFeetOverlays;
		if(g_numFeetOverlays > 0x7F)
			g_numFeetOverlays = 0x7F;
	}
	if(GetConfigOption_UInt32("Overlays/Feet", "iSpellOverlays", &spellFeetOverlays))
	{
		g_numSpellFeetOverlays = spellFeetOverlays;
		if(g_numSpellFeetOverlays > 0x7F)
			g_numSpellFeetOverlays = 0x7F;
	}

	if(GetConfigOption_UInt32("Overlays/Face", "iNumOverlays", &numFaceOverlays))
	{
		g_numFaceOverlays = numFaceOverlays;
		if(g_numFaceOverlays > 0x7F)
			g_numFaceOverlays = 0x7F;
	}
	if(GetConfigOption_UInt32("Overlays/Face", "iSpellOverlays", &spellFaceOverlays))
	{
		g_numSpellFaceOverlays = spellFaceOverlays;
		if(g_numSpellFaceOverlays > 0x7F)
			g_numSpellFaceOverlays = 0x7F;
	}

	if(GetConfigOption_UInt32("Overlays/Data", "bAlphaOverride", &alphaOverride))
	{
		g_alphaOverride = (alphaThreshold > 0);
	}

	if(GetConfigOption_UInt32("Overlays/Data", "iAlphaFlags", &alphaFlags))
	{
		g_alphaFlags = alphaFlags;
		if(g_alphaFlags > 0xFFFF)
			g_alphaFlags = 0xFFFF;
	}

	if(GetConfigOption_UInt32("Overlays/Data", "iAlphaThreshold", &alphaThreshold))
	{
		g_alphaThreshold = alphaThreshold;
		if(g_alphaThreshold > 0xFF)
			g_alphaThreshold = 0xFF;
	}

	std::string defaultTexture = GetConfigOption("Overlays/Data", "sDefaultTexture");
	if (defaultTexture.empty()) {
		defaultTexture = "textures\\actors\\character\\overlays\\default.dds";
	}
	g_overlayInterface.SetDefaultTexture(defaultTexture.c_str());

	if(GetConfigOption_UInt32("General", "iLoadMode", &loadMode))
	{
		g_loadMode = loadMode;
	}
	if (GetConfigOption_UInt32("General", "bEnableAutoTransforms", &enableAutoTransforms))
	{
		g_enableAutoTransforms = (enableAutoTransforms > 0);
	}
	if (GetConfigOption_UInt32("General", "bEnableEquippableTransforms", &enableEquippableTransforms))
	{
		g_enableEquippableTransforms = (enableEquippableTransforms > 0);
	}
	if (GetConfigOption_UInt32("General", "bEnableBodyGen", &enableBodyGen))
	{
		g_enableBodyGen = (enableBodyGen > 0);
	}
	if (GetConfigOption_UInt32("General", "iScaleMode", &scaleMode))
	{
		g_scaleMode = scaleMode;
	}

	if (GetConfigOption_UInt32("General", "iBodyMorphMode", &bodyMorphMode))
	{
		g_bodyMorphMode = bodyMorphMode;
	}

	if (GetConfigOption_UInt32("General", "bParallelMorphing", &parallelMorphing))
	{
		g_parallelMorphing = (parallelMorphing > 0);
	}

	UInt32 bodyMorphMemoryLimit = 256000000;
	if (GetConfigOption_UInt32("General", "uBodyMorphMemoryLimit", &bodyMorphMemoryLimit))
	{
		g_morphInterface.SetCacheLimit(bodyMorphMemoryLimit);
	}

	if(!g_enableFaceOverlays) {
		g_numFaceOverlays = 0;
		g_numSpellFaceOverlays = 0;
	}

	if (g_serialization) {
		g_serialization->SetUniqueID(g_pluginHandle, 'NIOV');
		g_serialization->SetRevertCallback(g_pluginHandle, NIOVSerialization_Revert);
		g_serialization->SetSaveCallback(g_pluginHandle, NIOVSerialization_Save);
		g_serialization->SetLoadCallback(g_pluginHandle, NIOVSerialization_Load);
		g_serialization->SetFormDeleteCallback(g_pluginHandle, NIOVSerialization_FormDelete);
	}

	// register scaleform callbacks
	if (g_scaleform) {
		g_scaleform->Register("NiOverride", RegisterScaleform);
	}

	if (g_messaging) {
		g_messaging->RegisterListener(g_pluginHandle, "SKSE", SKSEMessageHandler);
		g_messaging->RegisterListener(g_pluginHandle, nullptr, InterfaceExchangeMessageHandler);
	}

	InstallHooks();
	return true;
}

};
