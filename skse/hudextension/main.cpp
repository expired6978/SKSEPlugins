#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/SafeWrite.h"
#include "skse/GameAPI.h"
#include <shlobj.h>

#include "skse/GameRTTI.h"
#include "skse/GameReferences.h"
#include "skse/GameEvents.h"

#include "HUDExtension.h"
#include "Hooks.h"

IDebugLog	gLog;

PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

SKSEMessagingInterface	* g_messagingInterface = NULL;
SKSETaskInterface		* g_task = NULL;
SKSEScaleformInterface	* g_scaleform = NULL;


HUDExtension * g_hudExtension = NULL;


class CombatEventHandler : public BSTEventSink < TESCombatEvent >
{
public:
	virtual	EventResult ReceiveEvent(TESCombatEvent * evn, EventDispatcher<TESCombatEvent> * dispatcher)
	{
		float current = 0.0, max = 0.0;

		UInt32 contextFlags = ObjectWidget::kContext_None;
		if (evn->state == 0)
			contextFlags |= ObjectWidget::kContext_LeaveCombat;

		Actor * actor = DYNAMIC_CAST(evn->source, TESObjectREFR, Actor);
		if (actor) {
			current = actor->actorValueOwner.GetCurrent(24);
			max = actor->actorValueOwner.GetMaximum(24);

			if (!CALL_MEMBER_FN(*g_thePlayer, IsHostileToActor)(actor))
				contextFlags |= ObjectWidget::kContext_Friendly;
		}

		if (evn->source != (*g_thePlayer))
			g_task->AddUITask(new AddRemoveWidgetTask(evn->source->formID, current, max, evn->state, contextFlags));
		return kEvent_Continue;
	}
};

class DeathEventHandler : public BSTEventSink < TESDeathEvent >
{
public:
	virtual	EventResult ReceiveEvent(TESDeathEvent * evn, EventDispatcher<TESDeathEvent> * dispatcher)
	{
		if (evn->source != (*g_thePlayer))
			g_task->AddUITask(new AddRemoveWidgetTask(evn->source->formID, 0.0, 0.0, 0, ObjectWidget::kContext_Death));
		return kEvent_Continue;
	}
};

CombatEventHandler	g_combatEventHandler;
DeathEventHandler	g_deathEventHandler;

void MessageHandler(SKSEMessagingInterface::Message * msg)
{
	switch (msg->type)
	{
		case SKSEMessagingInterface::kMessage_InputLoaded:
		{
			g_combatEventDispatcher->AddEventSink(&g_combatEventHandler);
			g_deathEventDispatcher->AddEventSink(&g_deathEventHandler);
		}
		break;
		case SKSEMessagingInterface::kMessage_PostLoadGame:
		{
			if (g_hudExtension)
				g_hudExtension->RemoveAllHealthbars();
		}
		break;
	}
}

class SKSEScaleform_SetHUDFlags : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

		g_hudExtension->hudFlags = args->args[0].GetNumber();
	}
};

class SKSEScaleform_GetHUDFlags : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args)
	{
		args->result->SetNumber(g_hudExtension->hudFlags);
	}
};

bool RegisterScaleform(GFxMovieView * view, GFxValue * root)
{
	RegisterFunction<SKSEScaleform_GetHUDFlags>(root, view, "GetHUDFlags");
	RegisterFunction<SKSEScaleform_SetHUDFlags>(root, view, "SetHUDFlags");
	return true;
}

extern "C"
{

bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
{
	gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim\\SKSE\\skse_hudextension.log");
	_MESSAGE("skse_hudextension");

	// populate info structure
	info->infoVersion =	PluginInfo::kInfoVersion;
	info->name =		"hudextension";
	info->version =		1;

	// store plugin handle so we can identify ourselves later
	g_pluginHandle = skse->GetPluginHandle();

	if(skse->isEditor)
	{
		_MESSAGE("loaded in editor, marking as incompatible");

		return false;
	}
	else if(skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0)
	{
		_MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);

		return false;
	}

	g_scaleform = (SKSEScaleformInterface *)skse->QueryInterface(kInterface_Scaleform);
	if (!g_scaleform)
	{
		_MESSAGE("couldn't get scaleform interface");
		return false;
	}

	// get the messaging interface and query its version
	g_messagingInterface = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
	if(!g_messagingInterface)
	{
		_MESSAGE("couldn't get messaging interface");
		return false;
	}
	if(g_messagingInterface->interfaceVersion < 1)
	{
		_MESSAGE("messaging interface too old (%d expected %d)", g_messagingInterface->interfaceVersion, 1);
		return false;
	}

	// get the messaging interface and query its version
	g_task = (SKSETaskInterface *)skse->QueryInterface(kInterface_Task);
	if(!g_task)
	{
		_MESSAGE("couldn't get task interface");
		return false;
	}
	if(g_task->interfaceVersion < 2)
	{
		_MESSAGE("task interface too old (%d expected %d)", g_task->interfaceVersion, 1);
		return false;
	}

	// supported runtime version
	return true;
}

bool SKSEPlugin_Load(const SKSEInterface * skse)
{
	_MESSAGE("Hud Extension Loaded");

	g_messagingInterface->RegisterListener(0, "SKSE", MessageHandler);
	g_scaleform->Register("HudExtension", RegisterScaleform);

	WriteRelJump(kInstallRaceMenuHook_Base, (UInt32)InstallRaceMenuHook_Entry);
	return true;
}

};
