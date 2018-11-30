#include "main.h"


std::string mName = "test";
UInt32 mVer = 1;

PluginHandle			    g_pluginHandle = kPluginHandle_Invalid;

#include "xbyak/xbyak.h"
#include "f4se_common/BranchTrampoline.h"
#include "gfxfunctions.h"

F4SEScaleformInterface		*g_scaleform = NULL;
F4SEPapyrusInterface		*g_papyrus = NULL;
F4SEMessagingInterface		*g_messaging = NULL;

typedef void(*_ContainerMenuInvoke)(ContainerMenuBase* menu, GFxFunctionHandler::Args* args);

RelocAddr <_ContainerMenuInvoke> ContainerMenuInvoke(0x00B0A280);
_ContainerMenuInvoke ContainerMenuInvoke_Original;

class PipboyMenu;
typedef void(*_PipboyMenuInvoke)(PipboyMenu* menu, GFxFunctionHandler::Args* args);

RelocAddr <_PipboyMenuInvoke> PipboyMenuInvoke(0x00B93F60);
_PipboyMenuInvoke PipboyMenuInvoke_Original;


void DumpWorkshopEntry(WorkshopEntry* entry)
{
	gLog.Indent();
	for (size_t i = 0; i < entry->entries.count; i++)
	{
		if (entry->entries[i]->recipe)
		{
			_MESSAGE("recipe: %s", entry->entries[i]->object->GetFullName());
		}
		else if (entry->entries[i]->entries.entries)
		{
			_MESSAGE("category: %s", entry->entries[i]->kwd ? entry->entries[i]->kwd->GetFullName() : "<none>");
			DumpWorkshopEntry(entry->entries[i]);
		}
	}
	gLog.Outdent();
}

WorkshopEntry* SetSelectedWorkshopEntryInRow(WorkshopEntry* entry, UInt32 index)
{
	WorkshopEntry* result = nullptr;
	for (size_t i = 0; i < entry->entries.count; i++)
	{
		if (i == index)
		{
			entry->entries[i]->selected = 1;
			result = entry->entries[i];
		}
		else
		{
			entry->entries[i]->selected = 0;
		}
	}
	if (result)
	{
		if (result->recipe)
		{
			_MESSAGE("select recipe: %s", result->object->GetFullName());
		}
		else if (result->entries.entries)
		{
			_MESSAGE("select category: %s", result->kwd ? result->kwd->GetFullName():"<none>");
		}
	}
	return result;
}

UInt16 SetSelectedWorkshopEntry(UInt32 row1index, UInt32 row2index, UInt32 row3index, UInt32 row4index)
{
	WorkshopEntry* tempEntry = nullptr;
	UInt16 topDepth = 0;
	_MESSAGE("set up 1st entry");
	tempEntry = SetSelectedWorkshopEntryInRow(g_rootWorkshopEntry, row1index);
	if (tempEntry->entries.entries)
	{
		topDepth = 1;
		_MESSAGE("set up 2nd entry");
		tempEntry = SetSelectedWorkshopEntryInRow(tempEntry, row2index);
		if (tempEntry->entries.entries)
		{
			topDepth = 2;
			_MESSAGE("set up 3rd entry");
			tempEntry = SetSelectedWorkshopEntryInRow(tempEntry, row3index);
			if (tempEntry->entries.entries)
			{
				topDepth = 3;
				_MESSAGE("set up 4th entry");
				tempEntry = SetSelectedWorkshopEntryInRow(tempEntry, row4index);
			}
		}
	}
	return topDepth;
}

WorkshopEntry* myReplacingFunction(UInt16 depth, UInt16 *resultIndex)
{
	_MESSAGE("myReplacingFunction called");
	*g_workshopDepth = SetSelectedWorkshopEntry(3, 2, 1, 3);
	return GetSelectedWorkshopEntry(*g_workshopDepth, resultIndex);
}







void PipboyMenuInvoke_Hook(PipboyMenu * menu, GFxFunctionHandler::Args * args) {


	if (args->optionID == 0xD && args->numArgs == 4 && args->args[0].GetType() == GFxValue::kType_Int \
		&& args->args[1].GetType() == GFxValue::kType_Array && args->args[2].GetType() == GFxValue::kType_Array)
	{
		SInt32 selectedIndex = args->args[0].GetInt();
		_MESSAGE("%i selected index", selectedIndex);
		if (selectedIndex>-1)
		{
			BSFixedString str = BSFixedString("HandleID");
			PipboyObject::PipboyTableItem *ti = (*g_PipboyDataManager)->inventoryData.inventoryObjects[selectedIndex]->table.Find(&str);
			if (ti)
			{
				UInt32 val = ((PipboyPrimitiveValue<UInt32>*)(ti->value))->value;
				_MESSAGE("handleID: %u", val);
			}
			BSFixedString str2 = BSFixedString("StackID");
			PipboyObject::PipboyTableItem *ti2 = (*g_PipboyDataManager)->inventoryData.inventoryObjects[selectedIndex]->table.Find(&str2);
			if (ti2)
			{
				tracePipboyArray((PipboyArray*)ti2->value);
			}
		}
	}
	return PipboyMenuInvoke_Original(menu, args);
}


void ContainerMenuInvoke_Hook(ContainerMenuBase * menu, GFxFunctionHandler::Args * args) {
	ContainerMenuInvoke_Original(menu, args);
	if (args->optionID != 3) return;
	int itemIndex = args->args[0].GetInt();
	if (itemIndex < 0) return;
	int isContainer = args->args[1].GetInt();
	auto movie = menu->movie;
	auto root = movie->movieRoot;
	GFxValue ListArray, ListArrayItem, ListArrayItemCardInfoList;
	BGSInventoryItem* ii = nullptr;
	if (isContainer == 0)
	{
		root->GetVariable(&ListArray, "root1.FilterHolder_mc.Menu_mc.playerListArray");
	}
	else
	{
		root->GetVariable(&ListArray, "root1.FilterHolder_mc.Menu_mc.containerListArray");
	}
	ListArray.GetElement(itemIndex, &ListArrayItem);
	if (ListArrayItem.HasMember("haveExtraData")) return;
	UInt32 handleID = 0;
	UInt32 stackID = 0;
	if (DYNAMIC_CAST(menu, GameMenuBase, ContainerMenu))
	{
		//_MESSAGE("is Container menu");
		if (isContainer)
		{
			handleID = menu->contItems[itemIndex].HandleID;
			stackID = menu->contItems[itemIndex].stackid;
		}
		else
		{
			handleID = menu->playerItems[itemIndex].HandleID;
			stackID = menu->playerItems[itemIndex].stackid;
		}
	}
	else if (DYNAMIC_CAST(menu, GameMenuBase, BarterMenu))
	{
		//_MESSAGE("is Barter menu");
		GFxValue tempVal;
		ListArrayItem.GetMember("handle", &tempVal);
		if (tempVal.type == GFxValue::kType_Undefined) return;
		handleID = tempVal.GetUInt();
		ListArrayItem.GetMember("stackArray", &tempVal);
		tempVal.GetElement(0, &tempVal);
		stackID = tempVal.GetUInt();
	}
	else
	{
		return;
	}

	ii = getInventoryItemByHandleID_int(handleID);
	if (!ii) return;
	ListArrayItem.GetMember("ItemCardInfoList", &ListArrayItemCardInfoList);
	// put ur code there. use ii and stackID variables

	TESForm* form = ii->form;
	BGSInventoryItem::Stack* currentStack = ii->stack;
	while (stackID != 0)
	{
		currentStack = currentStack->next;
		stackID--;
	}
	if (form->formType == kFormType_WEAP)
	{
		TESObjectWEAP* weap = (TESObjectWEAP*)form;
		float APCost = weap->weapData.actionCost;
		ExtraDataList * stackDataList = currentStack->extraData;
		if (stackDataList) {
			ExtraInstanceData * eid = DYNAMIC_CAST(stackDataList->GetByType(kExtraData_InstanceData), BSExtraData, ExtraInstanceData);
			if (eid)
			{
				((TESObjectWEAP::InstanceData*)eid->instanceData)->baseDamage = 1000;
				APCost = ((TESObjectWEAP::InstanceData*)eid->instanceData)->actionCost;
			}
		}
		GFxValue extraData;
		root->CreateObject(&extraData);
		RegisterString(&extraData, root, "text", "APCost");
		RegisterString(&extraData, root, "value", std::to_string(int(round(APCost))).c_str());
		RegisterInt(&extraData, "difference", 0);
		ListArrayItemCardInfoList.PushBack(&extraData);
	}

	// end of ur code

	GFxValue haveExtraData;
	haveExtraData.SetBool(true);
	ListArrayItem.SetMember("haveExtraData", &haveExtraData);
	return;
}


class ViewCasterUpdateEventSink : public BSTEventSink<ViewCasterUpdateEvent>
{
public:
	virtual    EventResult    ReceiveEvent(ViewCasterUpdateEvent * evn, void * dispatcher) override
	{
		TESObjectREFR* ref = nullptr;
		LookupREFRByHandle(&evn->handle, &ref);
		if (ref)
		{
			_MESSAGE("ViewCasterUpdateEvent handle %d name %s formid %u", evn->handle, ref->baseForm->GetFullName(), ref->formID);
		}
		return kEvent_Continue;
	}
};

ViewCasterUpdateEventSink viewCasterUpdateEventSink;

bool RegisterScaleform(GFxMovieView * view, GFxValue * f4se_root)
{

	GFxMovieRoot	*movieRoot = view->movieRoot;

	GFxValue currentSWFPath;
	std::string currentSWFPathString = "";
	if (movieRoot->GetVariable(&currentSWFPath, "root.loaderInfo.url")) {
		currentSWFPathString = currentSWFPath.GetString();
		//_MESSAGE("hooking %s", currentSWFPathString.c_str());
		if (currentSWFPathString.find("HUDMenu.swf") != std::string::npos)
		{
		//	_DMESSAGE("hooking HUDmenu");

		}
	}
	return true;
}

#include ".\TestS.h"

//-------------------------
// Input Handler
//-------------------------
class F4SEInputHandler : public BSInputEventUser
{
public:
	F4SEInputHandler() : BSInputEventUser(true) { }

	virtual void OnButtonEvent(ButtonEvent * inputEvent)
	{
		UInt32	keyCode;
		UInt32	deviceType = inputEvent->deviceType;
		UInt32	keyMask = inputEvent->keyMask;
		keyCode = keyMask;

		float timer = inputEvent->timer;
		bool  isDown = inputEvent->isDown == 1.0f && timer == 0.0f;
		bool  isUp = inputEvent->isDown == 0.0f && timer != 0.0f;

		BSFixedString* control = inputEvent->GetControlID();
		if (isDown) {
			_MESSAGE("isDown keycode: %i, control name: %s", keyMask, control->c_str());
		}
		else if (isUp) {
			_MESSAGE("isUp keycode: %i, control name: %s", keyMask, control->c_str());
		}
	}
};

F4SEInputHandler g_DebugInputHandler;


void RegisterForInput(bool bRegister) {
	if (bRegister) {
		g_DebugInputHandler.enabled = true;
		tArray<BSInputEventUser*>* inputEvents = &((*g_menuControls)->inputEvents);
		BSInputEventUser* inputHandler = &g_DebugInputHandler;
		int idx = inputEvents->GetItemIndex(inputHandler);
		if (idx == -1) {
			inputEvents->Push(&g_DebugInputHandler);
			_MESSAGE("Registered for input events.");
		}
	}
	else {
		g_DebugInputHandler.enabled = false;
	}
}

EventResult	TESLoadGameHandler::ReceiveEvent(TESLoadGameEvent * evn, void * dispatcher)
{
	_DMESSAGE("TESLoadGameEvent recieved");
	auto eventDispatcher = GET_EVENT_DISPATCHER(ViewCasterUpdateEvent);
	if (eventDispatcher) {
		_MESSAGE("GET_EVENT_DISPATCHER");
		eventDispatcher->AddEventSink(&viewCasterUpdateEventSink);
	}
	return kEvent_Continue;
}

void OnF4SEMessage(F4SEMessagingInterface::Message* msg) {
	switch (msg->type) {
	case F4SEMessagingInterface::kMessage_GameDataReady:
		_MESSAGE("kMessage_GameDataReady");
		static auto pLoadGameHandler = new TESLoadGameHandler();
		GetEventDispatcher<TESLoadGameEvent>()->AddEventSink(pLoadGameHandler);
		//testreg();
		//RegisterForInput(true);
		break;
	case F4SEMessagingInterface::kMessage_GameLoaded:
		_MESSAGE("kMessage_GameLoaded");
		break;
	}
}


bool GetScriptVariableValue(VMIdentifier* id, const char* varName, VMValue * outVal)
{
	if (id->m_typeInfo->memberData.unk00 == 3)
	{
		for (UInt32 i = 0; i < id->m_typeInfo->memberData.numMembers; ++i)
		{
			if (!_stricmp(id->m_typeInfo->properties->defs[i].propertyName.c_str(), varName))
			{
				return (*g_gameVM)->m_virtualMachine->GetPropertyValueByIndex(&id, i, outVal);
			}
		}
	}
	return false;
}

#include "f4se/PapyrusNativeFunctions.h"

class BGSSaveLoadManager
{
public:
	void*	unk00;
	void*	unk08;
	void*	unk10;
	void*	unk18;
	void*	unk20;
	void*	unk28;
	void*	unk30;
	void*	unk38;
	void*	unk40;
	void*	unk48;
	void*	unk50;
	void*	unk58;
	void*	unk60;
	void*	unk68;
	void*	unk70;
	UInt32	unk78;
	UInt32	numCharacters; // 7C
	void*	unk80;
	void*	unk88;
	void*	unk90;
	void*	characterList; // 98
	void*	unkA0;
	void*	unkA8;
	void*	unkB0;
	void*	unkB8;
	void*	unkC0[280];
};
STATIC_ASSERT(sizeof(BGSSaveLoadManager) == 0x980);

bool testfunk(StaticFunctionTag *base) {
	_MESSAGE("testfunk");
	GFxValue;
	tArray<void*>;
	//------------------- SAVE GAMES -------------

	RelocPtr <void*> g_UISaveLoadManager(0x5A13258); // 0x60?
	DumpClass(*g_UISaveLoadManager, 0x60/8);

	RelocPtr <BGSSaveLoadManager*> g_saveLoadManager(0x5AAC618); // 0x980
	DumpClass(*g_saveLoadManager, 0x980 / 8);
	_MESSAGE("numChar %i", (*g_saveLoadManager)->numCharacters);
	DumpClass((*g_saveLoadManager)->characterList, 50);

	//------------------- EO SAVE GAMES -------------


	/*struct xxy
	{
		TESForm* form;
		BSFixedString str;
		void Dump(void)
		{
			_MESSAGE("\t\tForm: %08X", form ? form->formID : 0);
			_MESSAGE("\t\tunk08: %s", str.c_str());
		}
	};
	RelocPtr <tHashSet<xxy, BSFixedString>> xxx(0x370F888);
	xxx->Dump();
	*/
	/*struct yyx
	{
		UInt32* formid;
		TESForm* form;

		void Dump(void)
		{
			DumpClass(this,2);
		}
	};
	RelocPtr <tHashSet<yyx, UInt32>*> yyy(0x590C748);
	(*yyy)->Dump();*/
	/*
	WorkshopMenu * menu = (WorkshopMenu *)(*g_ui)->GetMenu(BSFixedString("WorkshopMenu"));
	if (!menu)
	{
		_DMESSAGE("menu closed");
		return false;
	}
	else
	{
		_DMESSAGE("menu opened");
	}

	auto movie = menu->movie;
	if (!movie)
		return false;

	auto root = movie->movieRoot;
	if (!root)
		return false;
	NiTArray <NiAVObject *>* xarray = &menu->workshopMenuGeometry->unk28->m_children;
	for (size_t i = 0; i < xarray->m_size; i++)
	{
		_MESSAGE("name %s", xarray->m_data[i]->m_name.c_str());
	}
	*/
	//DumpClass(&menu->inventory3DManager, 0x140/8);
	//DumpClass(menu->workshopMenuGeometry, 0x30/8);
	//DumpWorkshopEntry(g_rootWorkshopEntry);
	//WM_Up(menu);
	//UInt16 workshopItemIndex = 0;
	//DumpClass(GetSelectedWorkshopEntry(*g_workshopDepth, &workshopItemIndex), 10);

	//*g_workshopDepth = 3;
	//static BSFixedString menuName("WorkshopMenu");
	//CALL_MEMBER_FN((*g_uiMessageManager), SendUIMessage)(menuName, kMessage_Message);

	//DumpClass(xxx->Dump(), 1000);
	return true;
	DumpClass(*g_PipboyDataManager, 500);
	_MESSAGE("----------------------------------------------------------------------------------");
	(*g_PipboyDataManager)->mapData.unkhs1.Dump();
	//DumpClass((*g_PipboyDataManager)->mapData.unkxx[5], 50);
	_MESSAGE("----------------------------------------------------------------------------------");

	(*g_PipboyDataManager)->mapData.unkhs2.Dump();
	//DumpClass((*g_PipboyDataManager)->mapData.unkxx[11], 50);
	
	return true;

	TESQuest* hc = DYNAMIC_CAST(LookupFormByID(0x80E), TESForm, TESQuest);
	IObjectHandlePolicy * hp = (*g_gameVM)->m_virtualMachine->GetHandlePolicy();
	UInt64 hdl = hp->Create(hc->kTypeID, hc);

	VMIdentifier* hcScriptIdentifier = nullptr;

	VirtualMachine::IdentifierItem * ii = (*g_gameVM)->m_virtualMachine->m_attachedScripts.Find(&hdl);

	if (ii->count == 1)
	{
		if (_stricmp(ii->GetScriptObject(ii->identifier.one)->m_typeInfo->m_typeName.c_str(), "Hardcore:HC_ManagerScript") == 0)
		{
			hcScriptIdentifier = ii->GetScriptObject(ii->identifier.one);
		}		
	}
	else
	{
		for (UInt32 i = 0; i < ii->count; ++i)
		{
			
			if (_stricmp(ii->GetScriptObject(ii->identifier.many[i])->m_typeInfo->m_typeName.c_str(), "Hardcore:HC_ManagerScript") == 0)
			{
				hcScriptIdentifier = ii->GetScriptObject(ii->identifier.many[i]);
				break;
			}
				
		}
	}

	VMValue FoodPool, DrinkPool;

	if (hcScriptIdentifier)
	{
		if (GetScriptVariableValue(hcScriptIdentifier, "FoodPool", &FoodPool))
		{
			_MESSAGE("FoodPool %d", FoodPool.data.i);
		}
		if (GetScriptVariableValue(hcScriptIdentifier, "DrinkPool", &DrinkPool))
		{
			_MESSAGE("DrinkPool %d", DrinkPool.data.i);
		}
	}

	/*class xxxVisitor : public VirtualMachine::IdentifierItem::IScriptVisitor
	{
	public:
		virtual bool Visit(VMIdentifier* obj)
		{
			_MESSAGE("\t\t\t%s %i", obj->m_typeInfo->m_typeName.c_str(), _stricmp(obj->m_typeInfo->m_typeName.c_str(), "Hardcore:HC_ManagerScript"));
			if(obj->m_typeInfo->memberData.unk00 == 3)
			{
			_MESSAGE("Members:");
			gLog.Indent();
			for(UInt32 i = 0; i < obj->m_typeInfo->memberData.numMembers; ++i)
			{
			_MESSAGE("%s", obj->m_typeInfo->properties->defs[i].propertyName.c_str());
			DumpClass((void*)((uintptr_t)obj+0x10*(i+3)), 2);
			}
			gLog.Outdent();
			}
			
			return true;
		}
	};

	if (xxx)
	{
		xxxVisitor visitor;
		xxx->ForEachScript(&visitor);
	}*/

	return true;

}

bool RegisterFuncs(VirtualMachine* vm)
{
	vm->RegisterFunction(
		new NativeFunction0 <StaticFunctionTag, bool>("testfunk", "MyDebug:debugS", testfunk, vm));
	return true;
}

typedef bool(*_GetPropertyValue)(void* param1, void* param2, void* param3, void* param4);

RelocAddr <_GetPropertyValue> GetPropertyValue(0x002759FB0);
_GetPropertyValue GetPropertyValue_Original;

bool GetPropertyValue_Hook(void* param1, void* param2, void* param3, void* param4) {
	_MESSAGE("GetPropertyValue_Hook");
	return GetPropertyValue_Original(param1, param2, param3, param4);

}

typedef UInt32(*_ScriptFunction_Invoke)(void* param1, void* param2, void* param3, void* param4, void* param5);

RelocAddr <_ScriptFunction_Invoke> ScriptFunction_Invoke(0x0027D1C60);
_ScriptFunction_Invoke ScriptFunction_Invoke_Original;

struct unkxxx
{
	void* unk1;
	void* unk2;
};
UInt32 ScriptFunction_Invoke_Hook(IFunction* param1, unkxxx* param2, void* param3, VirtualMachine* param4, VMState* param5) {
	if (!_strcmpi("ModDrinkPoolAndUpdateThirstEffects", param1->GetName()->c_str()))
	{
		_MESSAGE("ModDrinkPoolAndUpdateThirstEffects");
		// get drinkpool variable value
	}
	else if (!_strcmpi("ModFoodPoolAndUpdateHungerEffects", param1->GetName()->c_str()))
	{
		_MESSAGE("ModFoodPoolAndUpdateHungerEffects");
		// get foodpool variable value
	}
	else if (!_strcmpi("ApplyEffect", param1->GetName()->c_str()))
	{
		_MESSAGE("ApplyEffect");

		DumpClass(param2, 15);

		DumpClass(param3, 10);

		DumpClass(&param5, 10);

	}

	UInt32 result =  ScriptFunction_Invoke_Original(param1, param2, param3, param4, param5);
	return result;
}

_WorkshopMenuProcessMessage WorkshopMenuProcessMessage_Original;

void WorkshopMenuProcessMessage_Hook(WorkshopMenu* menu, UIMessage * message) {
	if (message->type != 0)
	{
		_MESSAGE("WorkshopMenuProcessMessage_Hook %i", message->type);
		//DumpClass(message, 10);
	}

	WorkshopMenuProcessMessage_Original(menu, message);
}

_OnWorkshopMenuButtonEvent OnWorkshopMenuButtonEvent_Original;

bool OnWorkshopMenuButtonEvent_Hook(BSInputEventUser* eu, ButtonEvent * inputEvent) {
	//_MESSAGE("OnWorkshopMenuButtonEvent_Hook %s", inputEvent->controlID.c_str());
	WorkshopMenu* wm = (WorkshopMenu*)((uintptr_t)eu - 0x10);
	//DumpClass((void*)((uintptr_t)eu-0x10), 0x500/8);


	bool result = OnWorkshopMenuButtonEvent_Original(eu, inputEvent);


	//_MESSAGE("============================ WORKSHOP MENU DUMP ================================");
	//UInt16 workshopItemIndex = 0;
	//DumpClass(GetSelectedWorkshopEntry(*g_workshopDepth, &workshopItemIndex),10);
	//_MESSAGE("%i %i", *g_workshopDepth, workshopItemIndex);
	/*
	RelocPtr <void*> x_parent(0x59198D0);
	DumpClass(x_parent,10);





	gLog.Indent();
	for (size_t i = 0; i < x_entries->count; i++)
	{
		_MESSAGE("%s", x_entries->entries[i]->kwd? x_entries->entries[i]->kwd->GetFullName():"<none>");
		DumpClass(x_entries->entries[i], 10);
		if (x_entries->entries[i]->entries.count>0)
		{
			gLog.Indent();
			for (size_t j = 0; j < x_entries->entries[i]->entries.count; j++)
			{
				_MESSAGE("%s", x_entries->entries[i]->entries[j]->kwd ? x_entries->entries[i]->entries[j]->kwd->GetFullName() : "<none>");
				DumpClass(x_entries->entries[i]->entries[j], 10);
				if (x_entries->entries[i]->entries[j]->entries.count > 0)
				{
					gLog.Indent();
					for (size_t k = 0; k < x_entries->entries[i]->entries[j]->entries.count; k++)
					{
						_MESSAGE("%s", x_entries->entries[i]->entries[j]->entries[k]->kwd ? x_entries->entries[i]->entries[j]->entries[k]->kwd->GetFullName() : "<none>");
						DumpClass(x_entries->entries[i]->entries[j]->entries[k], 10);
					}
					gLog.Outdent();
				}
			}
			gLog.Outdent();
		}
	}
	gLog.Outdent();*/

	//

	//_MESSAGE("x_row %i", *x_row);
	return result;
}


#include "f4se/ObScript.h"
#include "f4se_common/SafeWrite.h"
extern "C"
{

	bool F4SEPlugin_Query(const F4SEInterface * f4se, PluginInfo * info)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, (const char*)("\\My Games\\Fallout4\\F4SE\\"+ mName +".log").c_str());

		logMessage("query");

		// populate info structure
		info->infoVersion =	PluginInfo::kInfoVersion;
		info->name =		mName.c_str();
		info->version =		mVer;

		// store plugin handle so we can identify ourselves later
		g_pluginHandle = f4se->GetPluginHandle();

		// Check game version
		if (f4se->runtimeVersion != CURRENT_RUNTIME_VERSION) {
			char str[512];
			sprintf_s(str, sizeof(str), "Your game version: v%d.%d.%d.%d\nExpected version: v%d.%d.%d.%d\n%s will be disabled.",
				GET_EXE_VERSION_MAJOR(f4se->runtimeVersion),
				GET_EXE_VERSION_MINOR(f4se->runtimeVersion),
				GET_EXE_VERSION_BUILD(f4se->runtimeVersion),
				GET_EXE_VERSION_SUB(f4se->runtimeVersion),
				GET_EXE_VERSION_MAJOR(CURRENT_RUNTIME_VERSION),
				GET_EXE_VERSION_MINOR(CURRENT_RUNTIME_VERSION),
				GET_EXE_VERSION_BUILD(CURRENT_RUNTIME_VERSION),
				GET_EXE_VERSION_SUB(CURRENT_RUNTIME_VERSION),
				mName.c_str()
			);

			MessageBox(NULL, str, mName.c_str(), MB_OK | MB_ICONEXCLAMATION);
			return false;
		}
		// Get the messaging interface
		g_messaging = (F4SEMessagingInterface *)f4se->QueryInterface(kInterface_Messaging);
		if (!g_messaging) {
			_MESSAGE("couldn't get messaging interface");
			return false;
		}
		g_scaleform = (F4SEScaleformInterface *)f4se->QueryInterface(kInterface_Scaleform);
		if (!g_scaleform)
		{
			_MESSAGE("couldn't get scaleform interface");
			return false;
		}
		// get the papyrus interface and query its version
		g_papyrus = (F4SEPapyrusInterface *)f4se->QueryInterface(kInterface_Papyrus);
		if (!g_papyrus)
		{
			_MESSAGE("couldn't get papyrus interface");
			return false;
		}
		else {
			_MESSAGE("got papyrus interface");
		}

		return true;
	}

	bool F4SEPlugin_Load(const F4SEInterface *f4se)
	{
		InitAddresses();
		InitWSMAddresses();
		RVAManager::UpdateAddresses(f4se->runtimeVersion);
		g_rootWorkshopEntry.SetEffective(g_rootWorkshopEntry.GetUIntPtr()-0x10);
		if (false)
		{
			_MESSAGE("ObScriptCommands");
			for (ObScriptCommand* iter = g_firstObScriptCommand; iter->opcode < (kObScript_NumObScriptCommands + kObScript_ScriptOpBase); iter++) {
				//if (strcmp(iter->longName, "AddItem") == 0) {
				//if (iter->params)
				//{
				//	_MESSAGE("%s: numParams: %i)", iter->longName, iter->numParams);
				//	DumpClass(iter->params, iter->numParams*2);
				//}
				
				_MESSAGE("%s (%s): execute 0x%08X parse 0x%08X eval 0x%08X", iter->longName, iter->shortName, (uintptr_t)iter->execute - RelocationManager::s_baseAddr, (uintptr_t)iter->parse - RelocationManager::s_baseAddr, (uintptr_t)iter->eval - RelocationManager::s_baseAddr);
				//}
			}
			_MESSAGE("ConsoleCommands");
			for (ObScriptCommand* iter = g_firstConsoleCommand; iter->opcode < (kObScript_NumConsoleCommands + kObScript_ConsoleOpBase); iter++) {
				_MESSAGE("%s (%s): execute 0x%08X parse 0x%08X eval 0x%08X", iter->longName, iter->shortName, (uintptr_t)iter->execute - RelocationManager::s_baseAddr, (uintptr_t)iter->parse - RelocationManager::s_baseAddr, (uintptr_t)iter->eval - RelocationManager::s_baseAddr);
			}
		}



		logMessage("load");
		g_messaging->RegisterListener(g_pluginHandle, "F4SE", OnF4SEMessage);

		if (!g_branchTrampoline.Create(1024 * 64))
		{
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}
		if (g_papyrus)
		{
			g_papyrus->Register(RegisterFuncs);
			_MESSAGE("Papyrus Register Succeeded");
		}
		if (g_scaleform)
		{
			g_scaleform->Register("test", RegisterScaleform);
			_MESSAGE("Scaleform Register Succeeded");
		}

		if (!g_localTrampoline.Create(1024 * 64, nullptr))
		{
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}
		{
			struct ContainerMenuInvoke_Code : Xbyak::CodeGenerator {
				ContainerMenuInvoke_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;

					mov(r11, rsp);
					mov(ptr[r11 + 0x18], rbx);
					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(ContainerMenuInvoke.GetUIntPtr() + 7);
				}
			};

			void * codeBuf = g_localTrampoline.StartAlloc();
			ContainerMenuInvoke_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());

			ContainerMenuInvoke_Original = (_ContainerMenuInvoke)codeBuf;

			g_branchTrampoline.Write6Branch(ContainerMenuInvoke.GetUIntPtr(), (uintptr_t)ContainerMenuInvoke_Hook);
		}

		{
			struct PipboyMenuInvoke_Code : Xbyak::CodeGenerator {
				PipboyMenuInvoke_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;

					mov(r11, rsp);
					push(rbp);
					push(rbx);
					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(PipboyMenuInvoke.GetUIntPtr() + 5);
				}
			};

			void * codeBuf = g_localTrampoline.StartAlloc();
			PipboyMenuInvoke_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());

			PipboyMenuInvoke_Original = (_PipboyMenuInvoke)codeBuf;

			g_branchTrampoline.Write5Branch(PipboyMenuInvoke.GetUIntPtr(), (uintptr_t)PipboyMenuInvoke_Hook);
		}

		{
			struct GetPropertyValue_Code : Xbyak::CodeGenerator {
				GetPropertyValue_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;

					mov(ptr[rsp + 8], rbx);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(GetPropertyValue.GetUIntPtr() + 5);
				}
			};

			void * codeBuf = g_localTrampoline.StartAlloc();
			GetPropertyValue_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());

			GetPropertyValue_Original = (_GetPropertyValue)codeBuf;

			g_branchTrampoline.Write5Branch(GetPropertyValue.GetUIntPtr(), (uintptr_t)GetPropertyValue_Hook);
		}


		{
			struct ScriptFunction_Invoke_Code : Xbyak::CodeGenerator {
				ScriptFunction_Invoke_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;

					sub(rsp, 0x28);
					mov(rdx, ptr[rdx]);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(ScriptFunction_Invoke.GetUIntPtr() + 8);
				}
			};

			void * codeBuf = g_localTrampoline.StartAlloc();
			ScriptFunction_Invoke_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());

			ScriptFunction_Invoke_Original = (_ScriptFunction_Invoke)codeBuf;

			g_branchTrampoline.Write6Branch(ScriptFunction_Invoke.GetUIntPtr(), (uintptr_t)ScriptFunction_Invoke_Hook);
		}

		{
			struct WorkshopMenuProcessMessage_Code : Xbyak::CodeGenerator {
				WorkshopMenuProcessMessage_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;

					mov(ptr[rsp + 8], rbx);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(WorkshopMenuProcessMessage.GetUIntPtr() + 5);
				}
			};

			void * codeBuf = g_localTrampoline.StartAlloc();
			WorkshopMenuProcessMessage_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());

			WorkshopMenuProcessMessage_Original = (_WorkshopMenuProcessMessage)codeBuf;

			g_branchTrampoline.Write5Branch(WorkshopMenuProcessMessage.GetUIntPtr(), (uintptr_t)WorkshopMenuProcessMessage_Hook);
		}

		{
			struct OnWorkshopMenuButtonEvent_Code : Xbyak::CodeGenerator {
				OnWorkshopMenuButtonEvent_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label retnLabel;

					mov(rax,rsp);
					push(rbp);
					push(r14);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(OnWorkshopMenuButtonEvent.GetUIntPtr() + 6);
				}
			};

			void * codeBuf = g_localTrampoline.StartAlloc();
			OnWorkshopMenuButtonEvent_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());

			OnWorkshopMenuButtonEvent_Original = (_OnWorkshopMenuButtonEvent)codeBuf;

			g_branchTrampoline.Write6Branch(OnWorkshopMenuButtonEvent.GetUIntPtr(), (uintptr_t)OnWorkshopMenuButtonEvent_Hook);
		}

		unsigned char data[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		//SafeWriteBuf(wsm_firstAddress.GetUIntPtr(), &data, sizeof(data));
		//g_branchTrampoline.Write5Call(wsm_secondAddress.GetUIntPtr(), (uintptr_t)myReplacingFunction);
		

		return true;
	}

};