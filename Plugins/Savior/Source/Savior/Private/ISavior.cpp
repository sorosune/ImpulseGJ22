//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2022 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ISavior.h"

#include "Savior.h"
#include "Savior_Shared.h"
#include "LoadScreen/HUD_LoadScreenStyle.h"

#if WITH_EDITORONLY_DATA
 #include "ISettingsModule.h"
 #include "ISettingsSection.h"
 #include "ISettingsContainer.h"
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "Synaptech"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FSavior : public ISavior {
private:
	bool HandleSettingsSaved() {
	  #if WITH_EDITORONLY_DATA
		const auto &Settings = GetMutableDefault<USaviorSettings>();
		Settings->SaveConfig(); return true;
	  #endif
	return false;}
	//
	void RegisterSettings() {
	  #if WITH_EDITORONLY_DATA
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings")) {
			ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Project");
			SettingsContainer->DescribeCategory("Synaptech",LOCTEXT("SynaptechCategoryName","Synaptech"),
			LOCTEXT("SynaptechCategoryDescription","Configuration of Synaptech Systems."));
			//
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project","Synaptech","SaviorSettings",
				LOCTEXT("SaviorSettingsName","Savior Settings"),
				LOCTEXT("SaviorSettingsDescription","General settings for the Savior Plugin"),
			GetMutableDefault<USaviorSettings>());
			//
			if (SettingsSection.IsValid()) {SettingsSection->OnModified().BindRaw(this,&FSavior::HandleSettingsSaved);}
		}
	  #endif
	}
	//
	void UnregisterSettings() {
	  #if WITH_EDITORONLY_DATA
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings")) {
			SettingsModule->UnregisterSettings("Project","Synaptech","SaviorSettings");
		}
	  #endif
	}
public:
	virtual void StartupModule() override {
		RegisterSettings();
		FSaviorLoadScreenStyle::Initialize();
		//
		const auto VER = FString(TEXT("{ Savior } Initializing Plugin:  v"))+PLUGIN_VERSION;
		LOG_SV(true,ESeverity::Info,VER);
	}
	//
	virtual void ShutdownModule() override {
		FSaviorLoadScreenStyle::Shutdown();
		if (UObjectInitialized()) {UnregisterSettings();}
	}
	//
    virtual bool SupportsDynamicReloading() override {return true;}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
IMPLEMENT_GAME_MODULE(FSavior,Savior);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////