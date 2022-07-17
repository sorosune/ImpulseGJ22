//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2022 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Savior.h"
#include "Savior_Shared.h"
#include "Reflector.h"
#include "LoadScreen/HUD_SaveLoadScreen.h"

#include "Runtime/Core/Public/Misc/App.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"

#include "Runtime/Engine/Public/TimerManager.h"
#include "Runtime/Engine/Classes/Engine/Font.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

#include "Policies/CondensedJsonPrintPolicy.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SerializeEntity"),STAT_FSimpleDelegateGraphTask_SerializeEntity,STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DeserializeEntity"),STAT_FSimpleDelegateGraphTask_DeserializeEntity,STATGROUP_TaskGraphTasks);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

EThreadSafety USavior::LastThreadState = EThreadSafety::IsCurrentlyThreadSafe;
EThreadSafety USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;

FTimerHandle USavior::TH_LoadScreen;

float USavior::SS_Progress = 100.f;
float USavior::SL_Progress = 100.f;
float USavior::SS_Workload = 0.f;
float USavior::SL_Workload = 0.f;
float USavior::SS_Complete = 0.f;
float USavior::SL_Complete = 0.f;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Constructors:

USaviorSettings::USaviorSettings() {
	DefaultPlayerName = TEXT("Player One");
	DefaultChapter = TEXT("Chapter 1");
	DefaultLocation = TEXT("???");
	DefaultPlayerLevel = 1;
	DefaultPlayerID = 0;
	//
	RespawnDynamicComponents = true;
	RespawnDynamicActors = true;
	//
	//
	InstanceScope.Add(UAutoInstanced::StaticClass());
	RespawnScope.Add(UActorComponent::StaticClass());
	RespawnScope.Add(AActor::StaticClass());
	//
	//
	LoadConfig();
}

USavior::USavior() {
	if (HasAnyFlags(RF_ClassDefaultObject)) {
		SetFlagsTo(GetFlags()|RF_MarkAsRootSet);
	}
	//
	if (GetOuter()&&GetOuter()->GetWorld()) {World=GetOuter()->GetWorld();}
	//
	SlotThumbnail = FSoftObjectPath(TEXT("/Savior/Icons/ICO_Thumbnail.ICO_Thumbnail"));
	if ((!IsRunningDedicatedServer())&&(!FeedbackFont.HasValidFont())) {
		ConstructorHelpers::FObjectFinder<UFont>FeedFontOBJ(TEXT("/Engine/EngineFonts/Roboto"));
		if (FeedFontOBJ.Succeeded()){FeedbackFont=FSlateFontInfo(FeedFontOBJ.Object,34,FName("Bold"));}
	}
	//
	const auto &Settings = GetDefault<USaviorSettings>();
	FeedbackLOAD = FText::FromString(TEXT("LOADING..."));
	FeedbackSAVE = FText::FromString(TEXT("SAVING..."));
	LoadScreenMode = ELoadScreenMode::NoLoadScreen;
	RuntimeMode = ERuntimeMode::BackgroundTask;
	ProgressBarTint = FLinearColor::White;
	SplashStretch = EStretch::Fill;
	ProgressBarOnMovie = true;
	LoadScreenTimer = 1.f;
	BackBlurPower = 25.f;
	//
	ComponentScope.Add(UActorComponent::StaticClass());
	ObjectScope.Add(UAutoInstanced::StaticClass());
	ActorScope.Add(AActor::StaticClass());
	//
	WriteMetaOnSave = true;
	Instanced = false;
	DeepLogs = false;
	Debug = false;
	//
	SlotMeta = FSlotMeta{};
	SlotData = FSlotData{};
} USavior::~USavior(){}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Routines:

static void StaticRemoveLoadScreen() {
	AHUD_SaviorUI::StaticRemoveLoadScreen();
}

UWorld* USavior::GetWorld() const {
	const auto Owner = GetOuter();
	//
	if (Owner) {return Owner->GetWorld();}
	//
	return World;
}

FString USavior::GetSlotName() const {
	FString SlotName;
	//
	if (SlotFileName.ToString().TrimStartAndEnd().IsEmpty()) {
		SlotName = FString::Printf(TEXT("%s_%s"),FApp::GetProjectName(),*GetName());
	} else {SlotName=SlotFileName.ToString().TrimStartAndEnd();}
	//
	return *SlotName;
}

bool USavior::IsInstance() const {
	return Instanced;
}

bool USavior::CheckInstance() {
	Instanced = ((World)&&(World->IsGameWorld()));
	//
	if (World==nullptr||(!World->IsGameWorld())) {
		LOG_SV(true,ESeverity::Error,FString::Printf(TEXT("Slot isn't Instanced. Slots can't be referenced directly in runtime, create a new Slot Instance for:  %s"),*GetFullName()));
	} return Instanced;
}

void USavior::AbortCurrentTask() {
	USavior::LastThreadState = USavior::ThreadSafety;
	USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;
	//
	USavior::SS_Workload=0; USavior::SS_Complete=0; USavior::SS_Progress=100.f;
	USavior::SL_Workload=0; USavior::SL_Complete=0; USavior::SL_Progress=100.f;
	//
	RemoveLoadScreen();
}

void USavior::SetWorld(UWorld* InWorld) {
	World = InWorld;
	CheckInstance();
}

void USavior::PostLoad() {
	Super::PostLoad();
}

void USavior::PostInitProperties() {
	Super::PostInitProperties();
	//
	ESaviorResult Results;
	if (HasAnyFlags(RF_ClassDefaultObject)) {
		LoadSlotMetaData(Results);
	}
}

void USavior::BeginDestroy() {
	Super::BeginDestroy();
	//
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Serializer Interface:

void USavior::WriteSlotToFile(const int32 PlayerID, ESaviorResult &Result) {
	const FString MetaName = FString::Printf(TEXT("%s___Meta"),*GetSlotName());
	//
	auto Meta = (WriteMetaOnSave) ? UGameplayStatics::SaveGameToSlot(SlotMetaToOBJ(),MetaName,0) : true;
	auto Saved = UGameplayStatics::SaveGameToSlot(SlotDataToOBJ(),GetSlotName(),PlayerID);
	//
	if (Meta && Saved) {Result=ESaviorResult::Success; return;}
	//
	Result = ESaviorResult::Failed;
}

void USavior::ReadSlotFromFile(const int32 PlayerID, ESaviorResult &Result) {
	const FString MetaName = FString::Printf(TEXT("%s___Meta"),*GetSlotName());
	//
	auto LoadedMeta = Cast<UMetaSlot>(UGameplayStatics::LoadGameFromSlot(MetaName,0));
	auto LoadedData = Cast<UDataSlot>(UGameplayStatics::LoadGameFromSlot(GetSlotName(),PlayerID));
	//
	if ((LoadedMeta)&&(LoadedData)) {
		SlotMeta = LoadedMeta;
		SlotData = LoadedData;
		//
		if (GetWorld()) {
			if (UGameInstance*const&GI=GetWorld()->GetGameInstance()) {
				if (USaviorSubsystem*System=GI->GetSubsystem<USaviorSubsystem>()) {
					USavior::ShadowCopySlot(this,System->StaticSlot,Result);
				}
			}
		}
		//
		Result = ESaviorResult::Success;
	return;}
	//
	Result = ESaviorResult::Failed;
}

void USavior::DeleteSlotFile(const int32 PlayerID, ESaviorResult &Result) {
	const FString MetaName = FString::Printf(TEXT("%s___Meta"),*GetSlotName());
	//
	if (UGameplayStatics::DoesSaveGameExist(MetaName,0)) {
		UGameplayStatics::DeleteGameInSlot(MetaName,0);
	}
	//
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(),PlayerID)) {
		UGameplayStatics::DeleteGameInSlot(GetSlotName(),PlayerID);
		Result=ESaviorResult::Success;
	return;}
	//
	Result = ESaviorResult::Failed;
}

void USavior::FindSlotFile(ESaviorResult &Result) {
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(),0)) {
		Result = ESaviorResult::Success; return;
	}
	//
	Result = ESaviorResult::Failed;
}

void USavior::SaveSlotMetaData(ESaviorResult &Result) {
	const FString MetaName = FString::Printf(TEXT("%s___Meta"),*GetSlotName());
	//
	auto Meta = UGameplayStatics::SaveGameToSlot(SlotMetaToOBJ(),MetaName,0);
	//
	if (Meta) {Result=ESaviorResult::Success; return;}
	//
	Result = ESaviorResult::Failed;
}

void USavior::LoadSlotMetaData(ESaviorResult &Result) {
	const FString MetaName = FString::Printf(TEXT("%s___Meta"),*GetSlotName());
	//
	if (!UGameplayStatics::DoesSaveGameExist(MetaName,0)) {
		Result = ESaviorResult::Failed; return;
	}
	//
	auto LoadedMeta = Cast<UMetaSlot>(UGameplayStatics::LoadGameFromSlot(MetaName,0));
	//
	if (LoadedMeta) {
		SlotMeta = LoadedMeta;
		Result = ESaviorResult::Success;
	return;}
	//
	Result = ESaviorResult::Failed;
}

const ESaviorResult USavior::PrepareSlotToSave(const UObject* Context) {
	if (((GetSaveProgress()<100.f)&&(GetSavesDone()>0))||((GetLoadProgress()<100.f)&&(GetLoadsDone()>0))) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("Save or Load action already in progress... Cannot begin another Save action. Save aborted!"));
		EVENT_OnFinishDataSAVE.Broadcast(false);
	return ESaviorResult::Failed;}
	//
	if (Context==nullptr||Context->GetWorld()==nullptr) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("Context Object is invalid to start a Save process... Save aborted!"));
		EVENT_OnFinishDataSAVE.Broadcast(false);
	return ESaviorResult::Failed;}
	//
	//
	/*if (UGameInstance*const&GI=Context->GetWorld()->GetGameInstance()) {
		if (USaviorSubsystem*System=GI->GetSubsystem<USaviorSubsystem>()) {
			System->Reset();
		}
	}*///
	//
	//
	USavior::ThreadSafety = EThreadSafety::IsPreparingToSaveOrLoad;
	World = Context->GetWorld();
	//
	double TimeSpan = SlotMeta.PlayTime + FMath::Abs(World->RealTimeSeconds);
	SlotMeta.PlayTime = FTimespan::FromSeconds(TimeSpan).GetHours();
	SlotMeta.SaveLocation = World->GetName();
	SlotMeta.SaveDate = FDateTime::Now();
	//
	//
	if (LoadScreenTrigger!=ELoadScreenTrigger::WhileLoading) {
		switch (RuntimeMode) {
			case ERuntimeMode::SynchronousTask:
				LaunchLoadScreen(EThreadSafety::SynchronousSaving,FeedbackSAVE);
			break;
			case ERuntimeMode::BackgroundTask:
				LaunchLoadScreen(EThreadSafety::AsynchronousSaving,FeedbackSAVE);
			break;
	default: break;}}
	//
	//
	USavior::SS_Workload = CalculateWorkload();
	USavior::SS_Progress = 0.f;
	USavior::SS_Complete = 0.f;
	//
	EVENT_OnBeginDataSAVE.Broadcast();
	OnPreparedToSave();
	//
	if (PauseGameOnLoad) {
		if (auto PC=World->GetFirstPlayerController()){PC->Pause();}
	}
	//
	return ESaviorResult::Success;
}

const ESaviorResult USavior::PrepareSlotToLoad(const UObject* Context) {
	if (((GetSaveProgress()<100.f)&&(GetSavesDone()>0))||((GetLoadProgress()<100.f)&&(GetLoadsDone()>0))) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("Save or Load action already in progress... Cannot begin another Load action. Load aborted!"));
		EVENT_OnFinishDataLOAD.Broadcast(false);
	return ESaviorResult::Failed;}
	//
	if (Context==nullptr||Context->GetWorld()==nullptr) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("Context Object is invalid to start a Load process... Load aborted!"));
		EVENT_OnFinishDataLOAD.Broadcast(false);
	return ESaviorResult::Failed;}
	//
	//
	/*if (UGameInstance*const&GI=Context->GetWorld()->GetGameInstance()) {
		if (USaviorSubsystem*System=GI->GetSubsystem<USaviorSubsystem>()) {
			System->Reset();
		}
	}*///
	//
	//
	USavior::ThreadSafety = EThreadSafety::IsPreparingToSaveOrLoad;
	SlotMeta.SaveLocation = World->GetName();
	World = Context->GetWorld();
	//
	if (LoadScreenTrigger!=ELoadScreenTrigger::WhileSaving) {
		switch (RuntimeMode) {
			case ERuntimeMode::SynchronousTask:
				LaunchLoadScreen(EThreadSafety::SynchronousLoading,FeedbackLOAD);
			break;
			case ERuntimeMode::BackgroundTask:
				LaunchLoadScreen(EThreadSafety::AsynchronousLoading,FeedbackLOAD);
			break;
	default: break;}}
	//
	USavior::SL_Workload = CalculateWorkload();
	USavior::SL_Progress = 0.f;
	USavior::SL_Complete = 0.f;
	//
	EVENT_OnBeginDataLOAD.Broadcast();
	OnPreparedToLoad();
	//
	if (PauseGameOnLoad) {
		if (auto PC=World->GetFirstPlayerController()){PC->Pause();}
	}
	//
	return ESaviorResult::Success;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Object Serialization Interface:

void USavior::SaveObject(UObject* Object, ESaviorResult &Result) {
	if ((USavior::ThreadSafety==EThreadSafety::SynchronousSaving)||(USavior::ThreadSafety==EThreadSafety::AsynchronousSaving)) {
		USavior::SS_Complete++; USavior::SS_Progress = ((USavior::SS_Complete/USavior::SS_Workload)*100);
	}
	//
	Result = ESaviorResult::Success;
	if (Object==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Object->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (Compression==ERecordType::Complex) {
		const auto ObjectID = Reflector::MakeObjectID(Object);
		const auto Record = GenerateRecord_Object(Object);
		SlotData.Complex.Add(ObjectID,Record);
	} else {
		const auto ObjectID = Reflector::MakeObjectID(Object);
		const auto Record = GenerateMinimalRecord_Object(Object);
		SlotData.Minimal.Add(ObjectID,Record);
	}
	//
	if (IsInGameThread()) {OnObjectSaved(Object);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnObjectSaved,Object),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void USavior::LoadObject(UObject* Object, ESaviorResult &Result) {
	if ((USavior::ThreadSafety==EThreadSafety::SynchronousLoading)||(USavior::ThreadSafety==EThreadSafety::AsynchronousLoading)) {
		USavior::SL_Complete++; USavior::SL_Progress = ((USavior::SL_Complete/USavior::SL_Workload)*100);
	}
	//
	Result = ESaviorResult::Success;
	if (Object==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Object->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	ISAVIOR_Serializable* SerialObject = Cast<ISAVIOR_Serializable>(Object);
	if (SerialObject)
	{
		SerialObject->Execute_OnPrepareToLoad(Object);
	}
	const auto ObjectID = Reflector::MakeObjectID(Object);
	//
	if (Compression==ERecordType::Complex) {
		if (!SlotData.Complex.Contains(ObjectID)) {Result=ESaviorResult::Failed; return;}
	} else {
		if (!SlotData.Minimal.Contains(ObjectID)) {Result=ESaviorResult::Failed; return;}
	}
	//
	if (Compression==ERecordType::Complex) {
		const auto Record = SlotData.Complex.FindRef(ObjectID);
		//
		if (Record.Destroyed) {
			MarkObjectAutoDestroyed(Object);
		} else {UnpackRecord_Object(Record,Object,Result);}
	} else {
		const auto Record = SlotData.Minimal.FindRef(ObjectID);
		//
		if (Record.Destroyed) {
			MarkObjectAutoDestroyed(Object);
		} else {UnpackMinimalRecord_Object(Record,Object,Result);}
	}
	//
	if (IsInGameThread()) {OnObjectLoaded(SlotMeta,Object);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnObjectLoaded,SlotMeta,Object),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void OnSavedCall(UObject * OBJ)
{
	if (!OBJ->IsValidLowLevelFast()) {return;}
	if (OBJ->IsA(USavior::StaticClass())) {return;}
	if (OBJ->IsA(USaveGame::StaticClass())) {return;}
	if (OBJ->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {return;}
	//
	const auto &Interface = Cast<ISAVIOR_Serializable>(OBJ);
	//
	if (Interface) {Interface->Execute_OnPrepareToSave(OBJ);}
	else if ((OBJ)->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())) {
		ISAVIOR_Serializable::Execute_OnPrepareToSave(OBJ);
	}
}

void USavior::SaveObjectHierarchy(UObject* Object, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Object==nullptr||GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Object->IsA(UActorComponent::StaticClass())||Object->IsA(UActorComponent::StaticClass())) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("SaveObjectHierarchy() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed; return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	OnSavedCall(Object);
	SaveObject(Object,Result);
	if (Result==ESaviorResult::Failed) {return;}
	//
	TArray<UObject*>Children;
	ForEachObjectWithOuter(Object,[&Children](UObject*Chid){Children.Add(Chid);},true);
	//
	for (const auto &OBJ : Children) {
		
		OnSavedCall(OBJ);
		SaveObject(OBJ,HierarchyResult);
	}
}

void USavior::LoadObjectHierarchy(UObject* Object, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Object==nullptr||GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Object->IsA(UActorComponent::StaticClass())||Object->IsA(UActorComponent::StaticClass())) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("LoadObjectHierarchy() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed; return;
	}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	LoadObject(Object,Result);
	if (Result==ESaviorResult::Failed) {return;}
	//
	TArray<UObject*>Children;
	ForEachObjectWithOuter(Object,[&Children](UObject*Chid){Children.Add(Chid);},true);
	//
	for (const auto &OBJ : Children) {
		SaveObject(OBJ,HierarchyResult);
	}
}

void USavior::SaveObjectsOfClass(TSubclassOf<UObject>Class, const bool SaveHierarchy, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Class.Get()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Class.Get()->IsChildOf(UActorComponent::StaticClass())||Class.Get()->IsChildOf(UActorComponent::StaticClass())) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("SaveObjectsOfClass() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed; return;
	}
	//
	for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
		if (!OBJ->IsValidLowLevelFast()) {continue;}
		if (!OBJ->IsA(Class.Get())) {continue;}
		//
		if ((*OBJ)==GetWorld()) {continue;}
		if (!OBJ->GetOutermost()->ContainsMap()) {continue;}
		if (OBJ->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		//
		if (!SaveHierarchy) {SaveObject(*OBJ,Result);}
		else {SaveObjectHierarchy(*OBJ,Result,HierarchyResult);}
	}
}

void USavior::LoadObjectsOfClass(TSubclassOf<UObject>Class, const bool LoadHierarchy, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Class.Get()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Class.Get()->IsChildOf(UActorComponent::StaticClass())||Class.Get()->IsChildOf(UActorComponent::StaticClass())) {
		LOG_SV(Debug,ESeverity::Warning,TEXT("LoadObjectsOfClass() is not usable on Actors. Use 'Save Actor Hierarchy' function instead."));
		Result = ESaviorResult::Failed; return;
	}
	//
	for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
		if (!OBJ->IsValidLowLevelFast()) {continue;}
		if (!OBJ->IsA(Class.Get())) {continue;}
		//
		if ((*OBJ)==GetWorld()) {continue;}
		if (!OBJ->GetOutermost()->ContainsMap()) {continue;}
		if (OBJ->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		//
		if (!LoadHierarchy) {LoadObject(*OBJ,Result);}
		else {LoadObjectHierarchy(*OBJ,Result,HierarchyResult);}
	}
}

void USavior::SaveComponent(UObject* Context, UActorComponent* Component, ESaviorResult &Result) {
	if ((USavior::ThreadSafety==EThreadSafety::SynchronousSaving)||(USavior::ThreadSafety==EThreadSafety::AsynchronousSaving)) {
		USavior::SS_Complete++; USavior::SS_Progress = ((USavior::SS_Complete/USavior::SS_Workload)*100);
	}
	//
	Result = ESaviorResult::Success;
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	World = Context->GetWorld();
	//
	if (Component==nullptr) {Result=ESaviorResult::Failed; return;}
	if (TAG_CNOSAVE(Component)) {Result=ESaviorResult::Failed; return;}
	if (Component->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (Compression==ERecordType::Complex) {
		const auto ComponentID = Reflector::MakeComponentID(Component,true);
		const auto Record = GenerateRecord_Component(Component);
		SlotData.Complex.Add(ComponentID,Record);
	} else {
		const auto ComponentID = Reflector::MakeComponentID(Component);
		const auto Record = GenerateMinimalRecord_Component(Component);
		SlotData.Minimal.Add(ComponentID,Record);
	}
	//
	if (IsInGameThread()) {OnComponentSaved(Component);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnComponentSaved,Component),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void USavior::LoadComponent(UObject* Context, UActorComponent* Component, ESaviorResult &Result) {
	if ((USavior::ThreadSafety==EThreadSafety::SynchronousLoading)||(USavior::ThreadSafety==EThreadSafety::AsynchronousLoading)) {
		USavior::SL_Complete++; USavior::SL_Progress = ((USavior::SL_Complete/USavior::SL_Workload)*100);
	}
	//
	Result = ESaviorResult::Success;
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	World = Context->GetWorld();
	//
	if (Component==nullptr) {Result=ESaviorResult::Failed; return;}
	if (TAG_CNOLOAD(Component)) {Result=ESaviorResult::Failed; return;}
	if (Component->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	FName ComponentID = NAME_None;
	if (Compression==ERecordType::Complex) {
		ComponentID = Reflector::MakeComponentID(Component,true);
		if (!SlotData.Complex.Contains(ComponentID)) {Result=ESaviorResult::Failed; return;}
	} else {
		ComponentID = Reflector::MakeComponentID(Component);
		if (!SlotData.Minimal.Contains(ComponentID)) {Result=ESaviorResult::Failed; return;}
	}
	//
	if (Compression==ERecordType::Complex) {
		const auto Record = SlotData.Complex.FindRef(ComponentID);
		if (!TAG_CNOKILL(Component)&&(Record.Destroyed)) {
			MarkComponentAutoDestroyed(Component);
		} else {
			UnpackRecord_Component(Record,Component,Result);
		}
	} else {
		const auto Record = SlotData.Minimal.FindRef(ComponentID);
		if (!TAG_CNOKILL(Component)&&(Record.Destroyed)) {
			MarkComponentAutoDestroyed(Component);
		} else {UnpackMinimalRecord_Component(Record,Component,Result);}
	}
	//
	if (IsInGameThread()) {OnComponentLoaded(SlotMeta,Component);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnComponentLoaded,SlotMeta,Component),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void USavior::LoadComponentWithGUID(UObject* Context, const FGuid &SGUID, ESaviorResult &Result) {
	if (!SGUID.IsValid()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	World = Context->GetWorld();
	//
	if (World==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	UActorComponent* Component = nullptr;
	for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
		if (!OBJ->IsA(UActorComponent::StaticClass())) {continue;}
		//
		if ((*OBJ)==World) {continue;}
		if (OBJ->GetWorld()!=World) {continue;}
		if (!OBJ->GetOutermost()->ContainsMap()) {continue;}
		if (OBJ->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		//
		const FGuid UID = Reflector::FindGUID(*OBJ);
		if (UID==SGUID) {Component=(CastChecked<UActorComponent>(*OBJ)); break;}
	}
	//
	if (Component==nullptr) {
		Result = ESaviorResult::Failed;
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Load Component by GUID]: Couldn't find in World any Component owner of this SGUID:  %s"),*SGUID.ToString()));
	return;}
	//
	if (Compression==ERecordType::Complex) {
		for (auto IT = SlotData.Complex.CreateConstIterator(); IT; ++IT) {
			const FSaviorRecord &Record = IT->Value;
			//
			if (Record.GUID==SGUID) {
				if (TAG_CNOLOAD(Component)) {Result=ESaviorResult::Failed; return;}
				if (!TAG_CNOKILL(Component)&&(Record.Destroyed)) {
					MarkComponentAutoDestroyed(Component);
				} else {UnpackRecord_Component(Record,Component,Result);}
			}
		}
	} else {Result=ESaviorResult::Failed; return;}
	//
	if (IsInGameThread()) {OnComponentLoaded(SlotMeta,Component);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnComponentLoaded,SlotMeta,Component),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void USavior::SaveAnimation(UObject* Context, ACharacter* Character, ESaviorResult &Result) {
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (const auto &Skel=Character->GetMesh()) {
		if (const auto &Anim=Skel->GetAnimInstance()) {
			const auto &Interface = Cast<ISAVIOR_Serializable>(Anim);
			if (Interface||Anim->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())) {
				const auto ActorAnimID = Reflector::MakeObjectID(Anim);
				const auto AnimRecord = GenerateRecord_Object(Anim);
				//
				SlotData.Complex.Add(ActorAnimID,AnimRecord);
				if (IsInGameThread()) {OnAnimationSaved(Anim);} else {
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
						FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnAnimationSaved,Anim),
						GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity),
						nullptr, ENamedThreads::GameThread
					);
				}
			} else {Result=ESaviorResult::Failed; return;}
		} else {Result=ESaviorResult::Failed; return;}
	} else {Result=ESaviorResult::Failed; return;}
}

void USavior::LoadAnimation(UObject* Context, ACharacter* Character, ESaviorResult &Result) {
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	Result = ESaviorResult::Success;
	//
	if (const auto &Skel=Character->GetMesh()) {
		if (const auto &Anim=Skel->GetAnimInstance()) {
			const auto &Interface = Cast<ISAVIOR_Serializable>(Anim);
			if (Interface||Anim->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())) {
				const auto ActorAnimID = Reflector::MakeObjectID(Anim);
				//
				if (!SlotData.Complex.Contains(ActorAnimID)) {Result=ESaviorResult::Failed; return;}
				const auto AnimRecord = SlotData.Complex.FindRef(ActorAnimID);
				UnpackRecord_Object(AnimRecord,Anim,Result);
				//
				if (IsInGameThread()) {OnAnimationLoaded(SlotMeta,Anim);} else {
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
						FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnAnimationLoaded,SlotMeta,Anim),
						GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
						nullptr, ENamedThreads::GameThread
					);
				}
			} else {Result=ESaviorResult::Failed; return;}
		} else {Result=ESaviorResult::Failed; return;}
	} else {Result=ESaviorResult::Failed; return;}
}

void USavior::SaveActorMaterials(UObject* Context, AActor* Actor, ESaviorResult &Result) {
	if (Actor==nullptr||Actor->GetWorld()==nullptr||!Actor->GetWorld()->IsGameWorld()) {Result=ESaviorResult::Failed; return;}
	if (Context==nullptr||!Context->IsValidLowLevel()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	const auto &Interface = Cast<ISAVIOR_Serializable>(Actor);
	if ((Interface==nullptr)&&(!Actor->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass()))) {
		Result = ESaviorResult::Failed; return;
	}
	//
	const auto ActorID = Reflector::MakeActorID(Actor,true);
	if (!SlotData.Complex.Contains(ActorID)) {Result=ESaviorResult::Failed; return;}
	if (ActorID.IsEqual(TEXT("ERROR:ID"))) {Result=ESaviorResult::Failed; return;}
	//
	FSaviorRecord &ActorRecord = SlotData.Complex.FindChecked(ActorID);
	TArray<USkeletalMeshComponent*>Skins;
	TArray<UStaticMeshComponent*>Meshes;
	//
	Actor->GetComponents<UStaticMeshComponent>(Meshes);
	Actor->GetComponents<USkeletalMeshComponent>(Skins);
	//
	for (auto CMP : Meshes) {
		const auto Mesh = CastChecked<UStaticMeshComponent>(CMP);
		TArray<UMaterialInterface*>Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats) {
			if (!Mat->IsValidLowLevel()) {continue;}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass())) {continue;}
			//
			FMaterialRecord MatInfo = CaptureMaterialSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat));
			const FName MatID = *FString::Printf(TEXT("%s.%s"),*ActorID.ToString(),*Mat->GetName());
			ActorRecord.Materials.Add(MatID,MatInfo);
		}
	}
	//
	for (auto CMP : Skins) {
		const auto Mesh = CastChecked<USkeletalMeshComponent>(CMP);
		TArray<UMaterialInterface*>Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats) {
			if (!Mat->IsValidLowLevel()) {continue;}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass())) {continue;}
			//
			FMaterialRecord MatInfo = CaptureMaterialSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat));
			const FName MatID = *FString::Printf(TEXT("%s.%s"),*ActorID.ToString(),*Mat->GetName());
			ActorRecord.Materials.Add(MatID,MatInfo);
		}
	}
}

void USavior::LoadActorMaterials(UObject* Context, AActor* Actor, ESaviorResult &Result) {
	if (Actor==nullptr||Actor->GetWorld()==nullptr||!Actor->GetWorld()->IsGameWorld()) {Result=ESaviorResult::Failed; return;}
	if (Context==nullptr||!Context->IsValidLowLevel()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	const auto &Interface = Cast<ISAVIOR_Serializable>(Actor);
	if ((Interface==nullptr)&&(!Actor->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass()))) {
		Result=ESaviorResult::Failed; return;
	}
	//
	const auto ActorID = Reflector::MakeActorID(Actor,true);
	if (!SlotData.Complex.Contains(ActorID)) {Result=ESaviorResult::Failed; return;}
	if (ActorID.IsEqual(TEXT("ERROR:ID"))) {Result=ESaviorResult::Failed; return;}
	//
	FSaviorRecord &ActorRecord = SlotData.Complex.FindChecked(ActorID);
	TArray<UStaticMeshComponent*>Meshes;
	TArray<USkeletalMeshComponent*>Skins;
	//
	Actor->GetComponents<UStaticMeshComponent>(Meshes);
	Actor->GetComponents<USkeletalMeshComponent>(Skins);
	//
	for (auto CMP : Meshes) {
		const auto Mesh = CastChecked<UStaticMeshComponent>(CMP);
		TArray<UMaterialInterface*>Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats) {
			if (!Mat->IsValidLowLevel()) {continue;}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass())) {continue;}
			RestoreMaterialFromSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat),ActorID.ToString(),ActorRecord);
		}
	}
	//
	for (auto CMP : Skins) {
		const auto Mesh = CastChecked<USkeletalMeshComponent>(CMP);
		TArray<UMaterialInterface*>Mats = Mesh->GetMaterials();
		//
		for (auto Mat : Mats) {
			if (!Mat->IsValidLowLevel()) {continue;}
			if (!Mat->IsA(UMaterialInstanceDynamic::StaticClass())) {continue;}
			RestoreMaterialFromSnapshot(CastChecked<UMaterialInstanceDynamic>(Mat),ActorID.ToString(),ActorRecord);
		}
	}
}

void USavior::SaveActor(UObject* Context, AActor* Actor, ESaviorResult &Result) {
	if ((USavior::ThreadSafety==EThreadSafety::SynchronousSaving)||(USavior::ThreadSafety==EThreadSafety::AsynchronousSaving)) {
		USavior::SS_Complete++; USavior::SS_Progress = ((USavior::SS_Complete/USavior::SS_Workload)*100);
	}	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	World = Context->GetWorld();
	//
	if (Actor==nullptr) {Result=ESaviorResult::Failed; return;}
	if (TAG_ANOSAVE(Actor)) {Result=ESaviorResult::Failed; return;}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (Compression==ERecordType::Complex) {
		const auto ActorID = Reflector::MakeActorID(Actor,true);
		const auto Record = GenerateRecord_Actor(Actor);
		SlotData.Complex.Add(ActorID,Record);
		//
		if (IsInGameThread()) {SerializeActorMaterials(this,Actor);} else {
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SerializeActorMaterials,this,Actor),
				GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity),
				nullptr, ENamedThreads::GameThread
			);
		}
		//
		if (Actor->IsA(ACharacter::StaticClass())) {
			SaveAnimation(Context,CastChecked<ACharacter>(Actor),Result);
		}
	} else {
		const auto ActorID = Reflector::MakeActorID(Actor);
		const auto Record = GenerateMinimalRecord_Actor(Actor);
		SlotData.Minimal.Add(ActorID,Record);
	}
	//
	if (IsInGameThread()) {OnActorSaved(Actor);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnActorSaved,Actor),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void USavior::LoadActor(UObject* Context, AActor* Actor, ESaviorResult &Result) {
	if ((USavior::ThreadSafety==EThreadSafety::SynchronousLoading)||(USavior::ThreadSafety==EThreadSafety::AsynchronousLoading)) {
		USavior::SL_Complete++; USavior::SL_Progress = ((USavior::SL_Complete/USavior::SL_Workload)*100);
	} if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (Actor==nullptr) {Result=ESaviorResult::Failed; return;}
	if (TAG_ANOLOAD(Actor)) {Result=ESaviorResult::Failed; return;}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	//
	FName ActorID = NAME_None;
	if (Compression==ERecordType::Complex) {
		ActorID = Reflector::MakeActorID(Actor,true);
		if (!SlotData.Complex.Contains(ActorID)) {Result=ESaviorResult::Failed; return;}
	} else {
		ActorID = Reflector::MakeActorID(Actor);
		if (!SlotData.Minimal.Contains(ActorID)) {Result=ESaviorResult::Failed; return;}
	}
	//
	if (Compression==ERecordType::Complex) {
		auto Record = SlotData.Complex.FindRef(ActorID);
		//
		if (Actor->IsA(APawn::StaticClass())) {
			Record.IgnoreTransform=IgnorePawnTransformOnLoad;
		}
		//
		if (!TAG_ANOKILL(Actor)&&(Record.Destroyed)) {
			MarkActorAutoDestroyed(Actor);
		} else {
			UnpackRecord_Actor(Record,Actor,Result);
			//
			if (IsInGameThread()) {DeserializeActorMaterials(this,Actor);} else {
				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeActorMaterials,this,Actor),
					GET_STATID(STAT_FSimpleDelegateGraphTask_SerializeEntity),
					nullptr, ENamedThreads::GameThread
				);
			}
			//
			if (Actor->IsA(ACharacter::StaticClass())) {
				ESaviorResult AnimResult = ESaviorResult::Success;
				LoadAnimation(Context,CastChecked<ACharacter>(Actor),AnimResult);
			}
		}
	} else {
		const auto Record = SlotData.Minimal.FindRef(ActorID);
		if (!TAG_ANOKILL(Actor)&&(Record.Destroyed)) {
			MarkActorAutoDestroyed(Actor);
		} else {UnpackMinimalRecord_Actor(Record,Actor,Result);}
	}
	//
	if (IsInGameThread()) {OnActorLoaded(SlotMeta,Actor);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnActorLoaded,SlotMeta,Actor),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

AActor* USavior::LoadActorWithGUID(UObject* Context, const FGuid &SGUID, ESaviorResult &Result) {
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return nullptr;}
	if (!SGUID.IsValid()) {Result=ESaviorResult::Failed; return nullptr;}
	//
	World = Context->GetWorld();
	Result = ESaviorResult::Success;
	if (World==nullptr) {Result=ESaviorResult::Failed; return nullptr;}
	//
	AActor* Actor = nullptr;
	for (TActorIterator<AActor>ACT(World); ACT; ++ACT) {
		if (!(*ACT)->IsValidLowLevelFast()) {continue;}
		if (!(*ACT)->GetOutermost()->ContainsMap()) {continue;}
		if ((*ACT)->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		//
		if (TAG_ANOLOAD(*ACT)) {continue;}
		//
		const FGuid UID = Reflector::FindGUID(*ACT);
		if (UID==SGUID) {Actor=(*ACT); break;}
	}
	//
	if (Actor==nullptr) {
		Result = ESaviorResult::Failed;
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Load Actor by GUID]: Couldn't find in World any Actor owner of this SGUID:  %s"),*SGUID.ToString()));
	return Actor;}
	//
	if (Compression==ERecordType::Complex) {
		for (auto IT = SlotData.Complex.CreateConstIterator(); IT; ++IT) {
			const FSaviorRecord &Record = IT->Value;
			//
			if (Record.GUID==SGUID) {
				if (!TAG_ANOKILL(Actor)&&(Record.Destroyed)) {
					MarkActorAutoDestroyed(Actor);
				} else {UnpackRecord_Actor(Record,Actor,Result);}
			}
		}
	} else {Result=ESaviorResult::Failed; return Actor;}
	//
	if (IsInGameThread()) {OnActorLoaded(SlotMeta,Actor);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnActorLoaded,SlotMeta,Actor),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
	//
	return Actor;
}

void USavior::SaveActorHierarchy(UObject* Context, AActor* Actor, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Actor==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	World = Context->GetWorld();
	SaveActor(World,Actor,Result);
	if (Result==ESaviorResult::Failed) {return;}
	//
	TArray<AActor*>Children;
	TArray<AActor*>Attached;
	TSet<UActorComponent*>Components;
	//
	Actor->GetAllChildActors(Children);
	Actor->GetAttachedActors(Attached);
	Components = Actor->GetComponents();
	//
	for (auto CMP : Components.Array()) {SaveComponent(World,CMP,HierarchyResult);}
	for (auto Sub : Attached) {SaveActor(World,Sub,HierarchyResult); Components = Sub->GetComponents(); for (auto CMP : Components.Array()) {SaveComponent(World,CMP,HierarchyResult);}}
	for (auto Child : Children) {SaveActor(World,Child,HierarchyResult); Components = Child->GetComponents(); for (auto CMP : Components.Array()) {SaveComponent(World,CMP,HierarchyResult);}}
}

void USavior::LoadActorHierarchy(UObject* Context, AActor* Actor, const bool IgnoreTransform, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Actor==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	bool TaggedIgnore = TAG_ANOTRAN(Actor);
	if ((!TaggedIgnore) && IgnoreTransform) {Actor->Tags.Add(TEXT("!Tran"));}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	World = Context->GetWorld();
	LoadActor(World,Actor,Result);
	//
	if ((!TaggedIgnore) && IgnoreTransform) {Actor->Tags.Remove(TEXT("!Tran"));}
	if (Result==ESaviorResult::Failed) {return;}
	//
	TArray<AActor*>Children;
	TArray<AActor*>Attached;
	TSet<UActorComponent*>Components;
	//
	Actor->GetAllChildActors(Children);
	Actor->GetAttachedActors(Attached);
	Components = Actor->GetComponents();
	//
	for (auto CMP : Components.Array()) {LoadComponent(World,CMP,HierarchyResult);}
	for (auto Sub : Attached) {LoadActor(World,Sub,HierarchyResult); Components = Sub->GetComponents(); for (auto CMP : Components.Array()) {LoadComponent(World,CMP,HierarchyResult);}}
	for (auto Child : Children) {LoadActor(World,Child,HierarchyResult); Components = Child->GetComponents(); for (auto CMP : Components.Array()) {LoadComponent(World,CMP,HierarchyResult);}}
}

void USavior::LoadActorHierarchyWithGUID(UObject* Context, const FGuid &SGUID, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!SGUID.IsValid()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	World = Context->GetWorld();
	AActor* Actor = LoadActorWithGUID(World,SGUID,Result);
	//
	if (Result==ESaviorResult::Failed) {return;}
	//
	TArray<AActor*>Children;
	TArray<AActor*>Attached;
	TSet<UActorComponent*>Components;
	//
	Actor->GetAllChildActors(Children);
	Actor->GetAttachedActors(Attached);
	Components = Actor->GetComponents();
	//
	for (auto CMP : Components.Array()) {
		const FGuid UID = Reflector::FindGUID(CMP);
		if (!UID.IsValid()) {continue;}
		LoadComponentWithGUID(World,UID,HierarchyResult);
	}
	//
	for (auto Sub : Attached) {
		const FGuid UID = Reflector::FindGUID(Sub);
		if (!UID.IsValid()) {continue;}
		//
		for (auto IT = SlotData.Complex.CreateConstIterator(); IT; ++IT) {
			const FSaviorRecord &Record = IT->Value;
			//
			if (Record.GUID==UID) {
				LoadActorHierarchyWithGUID(World,UID,Result,HierarchyResult);
			break;}
		}
		//
		TSet<UActorComponent*> SubCMP;
		SubCMP = Sub->GetComponents();
		//
		for (auto CMP : SubCMP.Array()) {
			const FGuid CUID = Reflector::FindGUID(CMP);
			if (!CUID.IsValid()) {continue;}
			LoadComponentWithGUID(World,CUID,HierarchyResult);
		}
	}
	//
	for (auto Child : Children) {
		const FGuid UID = Reflector::FindGUID(Child);
		if (!UID.IsValid()) {continue;}
		//
		for (auto IT = SlotData.Complex.CreateConstIterator(); IT; ++IT) {
			const FSaviorRecord &Record = IT->Value;
			//
			if (Record.GUID==UID) {
				LoadActorHierarchyWithGUID(World,UID,Result,HierarchyResult);
			break;}
		}
		//
		TSet<UActorComponent*> SubCMP;
		SubCMP = Child->GetComponents();
		//
		for (auto CMP : SubCMP.Array()) {
			const FGuid CUID = Reflector::FindGUID(CMP);
			if (!CUID.IsValid()) {continue;}
			//
			LoadComponentWithGUID(World,CUID,HierarchyResult);
		}
	}
}

void USavior::SaveActorsOfClass(UObject* Context, TSubclassOf<AActor>Class, const bool SaveHierarchy, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Class.Get()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Context->GetWorld()==nullptr) {
		Result = ESaviorResult::Failed;
	return;} World = Context->GetWorld();
	//
	for (TActorIterator<AActor>Actor(World); Actor; ++Actor) {
		if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		if (!Actor->IsA(Class.Get())) {continue;}
		//
		if (!SaveHierarchy) {SaveActor(World,*Actor,Result);}
		else {SaveActorHierarchy(World,*Actor,Result,HierarchyResult);}
	}
}

void USavior::LoadActorsOfClass(UObject* Context, TSubclassOf<AActor>Class, const bool LoadHierarchy, ESaviorResult &Result, ESaviorResult &HierarchyResult) {
	if (Class.Get()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	HierarchyResult = ESaviorResult::Failed;
	//
	if (Context->GetWorld()==nullptr) {
		Result = ESaviorResult::Failed;
	return;} World = Context->GetWorld();
	//
	for (TActorIterator<AActor>Actor(World); Actor; ++Actor) {
		if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		if (!Actor->IsA(Class.Get())) {continue;}
		//
		if (!LoadHierarchy) {LoadActor(World,*Actor,Result);}
		else {LoadActorHierarchy(World,*Actor,false,Result,HierarchyResult);}
	}
}

void USavior::SavePlayerState(UObject* Context, APlayerController* Controller, ESaviorResult &Result) {
	if (Controller==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	if (Controller->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	const auto &Pawn = Controller->GetPawn();
	const auto &PlayerHUD = Controller->MyHUD;
	const auto &PlayerState = Controller->PlayerState;
	//
	Result = ESaviorResult::Success;
	SaveActor(Context->GetWorld(),Controller,Result);
	//
	if ((Pawn)&&Pawn->IsValidLowLevelFast()) {SaveActor(Context->GetWorld(),Pawn,Result);}
	if ((PlayerHUD)&&PlayerHUD->IsValidLowLevelFast()) {SaveActor(Context->GetWorld(),PlayerHUD,Result);}
	if ((PlayerState)&&PlayerState->IsValidLowLevelFast()) {SaveActor(Context->GetWorld(),PlayerState,Result);}
}

void USavior::LoadPlayerState(UObject* Context, APlayerController* Controller, ESaviorResult &Result) {
	if (Controller==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	if (Controller->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	const auto &Pawn = Controller->GetPawn();
	const auto &PlayerHUD = Controller->MyHUD;
	const auto &PlayerState = Controller->PlayerState;
	//
	Result = ESaviorResult::Success;
	LoadActor(Context->GetWorld(),Controller,Result);
	//
	if ((Pawn)&&Pawn->IsValidLowLevelFast()) {LoadActor(Context->GetWorld(),Pawn,Result);}
	if ((PlayerHUD)&&PlayerHUD->IsValidLowLevelFast()) {LoadActor(Context->GetWorld(),PlayerHUD,Result);}
	if ((PlayerState)&&PlayerState->IsValidLowLevelFast()) {LoadActor(Context->GetWorld(),PlayerState,Result);}
}

void USavior::SaveGameInstanceSingleTon(UGameInstance* Instance, ESaviorResult &Result) {
	if (Instance==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (Instance->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	FString Patch;
	const auto ObjectID = Instance->GetFName();
	ObjectID.ToString().Split(TEXT("_C"),&Patch,nullptr,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
	//
	if (Compression==ERecordType::Complex) {
		const auto Record = GenerateRecord_Object(Instance);
		SlotData.Complex.Add(*Patch,Record);
	} else {
		const auto Record = GenerateMinimalRecord_Object(Instance);
		SlotData.Minimal.Add(*Patch,Record);
	}
	//
	if (IsInGameThread()) {OnGameInstanceSaved(Instance);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnGameInstanceSaved,Instance),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void USavior::LoadGameInstanceSingleTon(UGameInstance* Instance, ESaviorResult &Result) {
	if (Instance==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (Instance->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	FString Patch;
	const auto ObjectID = Instance->GetFName();
	ObjectID.ToString().Split(TEXT("_C"),&Patch,nullptr,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
	//
	if (Compression==ERecordType::Complex) {
		if (!SlotData.Complex.Contains(*Patch)) {Result=ESaviorResult::Failed; return;}
		const auto Record = SlotData.Complex.FindRef(*Patch);
		//
		if (Record.Destroyed) {
			Instance->ConditionalBeginDestroy();
		} else {UnpackRecord_Object(Record,Instance,Result);}
	} else {
		if (!SlotData.Minimal.Contains(*Patch)) {Result=ESaviorResult::Failed; return;}
		const auto Record = SlotData.Minimal.FindRef(*Patch);
		//
		if (Record.Destroyed) {
			Instance->ConditionalBeginDestroy();
		} else {UnpackMinimalRecord_Object(Record,Instance,Result);}
	}
	//
	if (IsInGameThread()) {OnGameInstanceLoaded(SlotMeta,Instance);} else {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&OnGameInstanceLoaded,SlotMeta,Instance),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void USavior::StaticLoadObject(UObject* Context, UObject* Object, ESaviorResult &Result) {
	if (Object==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (Object->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (UGameInstance*const&GI=Context->GetWorld()->GetGameInstance()) {
		if (USaviorSubsystem*System=GI->GetSubsystem<USaviorSubsystem>()) {
			System->StaticSlot->LoadObject(Object,Result);
		}
	}
}

void USavior::StaticLoadComponent(UObject* Context, UActorComponent* Component, ESaviorResult &Result) {
	if (Component==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (TAG_CNOLOAD(Component)) {Result=ESaviorResult::Failed; return;}
	if (Component->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (UGameInstance*const&GI=Context->GetWorld()->GetGameInstance()) {
		if (USaviorSubsystem*System=GI->GetSubsystem<USaviorSubsystem>()) {
			System->StaticSlot->LoadComponent(Context,Component,Result);
		}
	}
}

void USavior::StaticLoadActor(UObject* Context, AActor* Actor, ESaviorResult &Result) {
	if (Actor==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (TAG_ANOLOAD(Actor)) {Result=ESaviorResult::Failed; return;}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (UGameInstance*const&GI=Context->GetWorld()->GetGameInstance()) {
		if (USaviorSubsystem*System=GI->GetSubsystem<USaviorSubsystem>()) {
			System->StaticSlot->LoadActor(Context,Actor,Result);
		}
	}
}

void USavior::LoadObjectData(UObject* Object, const FSaviorRecord &Data, ESaviorResult &Result) {
	if (Object==nullptr||Object->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	USaviorSubsystem* System = nullptr;
	Result = ESaviorResult::Success;
	//
	if (UGameInstance*const&GI=Object->GetWorld()->GetGameInstance()) {
		System = GI->GetSubsystem<USaviorSubsystem>();
	}
	//
	if (Object->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (Data.Destroyed) {
		Object->ConditionalBeginDestroy();
	} else {
		TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
		TSharedRef<TJsonReader<TCHAR>>JReader = TJsonReaderFactory<TCHAR>::Create(Data.Buffer);
		//
		if (!FJsonSerializer::Deserialize(JReader,JSON)) {
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data<->Buffer]: Corrupted Data. Unable to unpack Data Record for Object: [%s]"),*Object->GetName()));
		}
		//
		for (TFieldIterator<FProperty>PIT(Object->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
			FProperty* Property = *PIT;
			//
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame)) {continue;}
			//
			const FString ID = Property->GetName();
			const bool IsSet = Property->IsA<FSetProperty>();
			const bool IsMap = Property->IsA<FMapProperty>();
			const bool IsInt = Property->IsA<FIntProperty>();
			const bool IsBool = Property->IsA<FBoolProperty>();
			const bool IsByte = Property->IsA<FByteProperty>();
			const bool IsEnum = Property->IsA<FEnumProperty>();
			const bool IsName = Property->IsA<FNameProperty>();
			const bool IsText = Property->IsA<FTextProperty>();
			const bool IsString = Property->IsA<FStrProperty>();
			const bool IsArray = Property->IsA<FArrayProperty>();
			const bool IsFloat = Property->IsA<FFloatProperty>();
			const bool IsInt64 = Property->IsA<FInt64Property>();
			const bool IsClass = Property->IsA<FClassProperty>();
			const bool IsDouble = Property->IsA<FDoubleProperty>();
			const bool IsStruct = Property->IsA<FStructProperty>();
			const bool IsObject = Property->IsA<FObjectProperty>();
			//
			if ((IsInt)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FIntProperty>(Property),Object); continue;}
			if ((IsBool)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FBoolProperty>(Property),Object); continue;}
			if ((IsByte)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FByteProperty>(Property),Object); continue;}
			if ((IsEnum)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FEnumProperty>(Property),Object); continue;}
			if ((IsName)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FNameProperty>(Property),Object); continue;}
			if ((IsText)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FTextProperty>(Property),Object); continue;}
			if ((IsString)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FStrProperty>(Property),Object); continue;}
			if ((IsFloat)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FFloatProperty>(Property),Object); continue;}
			if ((IsInt64)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FInt64Property>(Property),Object); continue;}
			if ((IsDouble)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FDoubleProperty>(Property),Object); continue;}
			//
			if ((IsSet)&&(JSON->HasField(ID))) {JSONToFProperty(System->StaticSlot,JSON,ID,CastFieldChecked<FSetProperty>(Property),Object); continue;}
			if ((IsMap)&&(JSON->HasField(ID))) {JSONToFProperty(System->StaticSlot,JSON,ID,CastFieldChecked<FMapProperty>(Property),Object); continue;}
			if ((IsArray)&&(JSON->HasField(ID))) {JSONToFProperty(System->StaticSlot,JSON,ID,CastFieldChecked<FArrayProperty>(Property),Object); continue;}
			if ((IsClass)&&(JSON->HasField(ID))) {JSONToFProperty(System->StaticSlot,JSON,ID,CastFieldChecked<FClassProperty>(Property),Object); continue;}
			if ((IsObject)&&(JSON->HasField(ID))) {JSONToFProperty(System->StaticSlot,JSON,ID,CastFieldChecked<FObjectProperty>(Property),Object); continue;}
			//
			if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::IgnoreCase))) {continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FTransform>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Transform); continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FLinearColor>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::LColor); continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector2D>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector2D); continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==FRuntimeFloatCurve::StaticStruct())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Curve); continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FRotator>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Rotator); continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector3D); continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FColor>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::FColor); continue;} else
			if ((IsStruct)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Struct); continue;}
		}
		//
		//
		if (Object->IsA(UActorComponent::StaticClass())) {
			auto Component = CastChecked<UActorComponent>(Object);
			if (IsInGameThread()) {DeserializeComponent(Data,Component);} else {
				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeComponent,Data,Component),
					GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
					nullptr, ENamedThreads::GameThread
				);
			}
			//
			FString ID = Reflector::MakeComponentID(Component).ToString();
			LOG_SV(System->StaticSlot->DeepLogs,ESeverity::Info,FString::Printf(TEXT("UNPACKED DATA for %s :: "),*ID)+Data.Buffer);
			LOG_SV(System->StaticSlot->Debug,ESeverity::Info,FString("Deserialized :: ")+ID);
		} else if (Object->IsA(AActor::StaticClass())) {
			auto Actor = CastChecked<AActor>(Object);
			if (IsInGameThread()) {DeserializeActor(Data,Actor);} else {
				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeActor,Data,Actor),
					GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
					nullptr, ENamedThreads::GameThread
				);
			}
			//
			FString ID = Reflector::MakeActorID(Actor).ToString();
			LOG_SV(System->StaticSlot->DeepLogs,ESeverity::Info,FString::Printf(TEXT("UNPACKED DATA for %s :: "),*ID)+Data.Buffer);
			LOG_SV(System->StaticSlot->Debug,ESeverity::Info,FString("Deserialized :: ")+ID);
		} else {
			FString ID = Reflector::MakeObjectID(Object).ToString();
			LOG_SV(System->StaticSlot->DeepLogs,ESeverity::Info,FString::Printf(TEXT("UNPACKED DATA for %s :: "),*ID)+Data.Buffer);
			LOG_SV(System->StaticSlot->Debug,ESeverity::Info,FString("Deserialized :: ")+ID);
		}
	}
}

void USavior::LoadComponentData(UActorComponent* Component, const FSaviorRecord &Data, ESaviorResult &Result) {
	if (Component==nullptr||Component->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (TAG_CNOLOAD(Component)) {Result=ESaviorResult::Failed; return;}
	if (Component->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (!TAG_CNOKILL(Component)&&(Data.Destroyed)) {
		Component->DestroyComponent();
	} else {
		LoadObjectData(Component,Data,Result);
	}
}

void USavior::LoadActorData(AActor* Actor, const FSaviorRecord &Data, ESaviorResult &Result) {
	if (Actor==nullptr||Actor->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (TAG_ANOLOAD(Actor)) {Result=ESaviorResult::Failed; return;}
	if (Actor->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {Result=ESaviorResult::Failed; return;}
	//
	if (!TAG_ANOKILL(Actor)&&(Data.Destroyed)) {
		Actor->Destroy();
	} else {
		LoadObjectData(Actor,Data,Result);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior World Serialization Interface:

void USavior::SaveGameWorld(UObject* Context, ESaviorResult &Result) {
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	Result = PrepareSlotToSave(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameWorld>(this))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameWorld>(this))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::LoadGameWorld(UObject* Context, const bool ResetLevelOnLoad, ESaviorResult &Result) {
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	if (Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	USaviorSubsystem* System = nullptr;
	if (UGameInstance*const&GI=Context->GetWorld()->GetGameInstance()) {
		System = GI->GetSubsystem<USaviorSubsystem>();
	} if (System==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	//
	FindSlotFile(Result);
	if (Result==ESaviorResult::Failed) {
		EVENT_OnFinishDataLOAD.Broadcast(false);
		AbortCurrentTask(); return;
	}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	ReadSlotFromFile(Settings->DefaultPlayerID,Result);
	if (Result==ESaviorResult::Failed) {AbortCurrentTask(); return;}
	//
	if (const_cast<USavior*>(this)!=System->StaticSlot) {
		if (SlotMeta.SaveLocation!=Context->GetWorld()->GetName()||ResetLevelOnLoad) {
			USavior::ShadowCopySlot(this,System->StaticSlot,Result);
			AbortCurrentTask();
			UGameplayStatics::OpenLevel(Context,*SlotMeta.SaveLocation,true); return;
		}
	}
	//
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameWorld>(this))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameWorld>(this))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::SaveLevel(UObject* Context, TSoftObjectPtr<UWorld>LevelToSave, bool WriteFile, ESaviorResult &Result) {
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	FString _LevelName;
	LevelToSave.ToString().Split(TEXT("."),nullptr,&_LevelName,ESearchCase::IgnoreCase,ESearchDir::FromEnd);
	FName LevelName = (LevelToSave.IsNull()) ? TEXT("None") : *_LevelName;
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	Result = PrepareSlotToSave(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeLevel>(this,LevelName,WriteFile))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeLevel>(this,LevelName,WriteFile))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::LoadLevel(UObject* Context, TSoftObjectPtr<UWorld>LevelToLoad, bool ReadFile, ESaviorResult &Result) {
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	FString _LevelName;
	Result = ESaviorResult::Success;
	LevelToLoad.ToString().Split(TEXT("."),nullptr,&_LevelName,ESearchCase::IgnoreCase,ESearchDir::FromEnd);
	FName LevelName = (LevelToLoad.IsNull()) ? TEXT("None") : *_LevelName;
	//
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	if (ReadFile) {
		FindSlotFile(Result);
		if (Result==ESaviorResult::Failed) {
			EVENT_OnFinishDataLOAD.Broadcast(false);
			AbortCurrentTask(); return;
		}
		//
		ReadSlotFromFile(Settings->DefaultPlayerID,Result);
		if (Result==ESaviorResult::Failed) {AbortCurrentTask(); return;}
	}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeLevel>(this,LevelName))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeLevel>(this,LevelName))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::SaveGameMode(UObject* Context, bool WithGameInstance, ESaviorResult &Result) {
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	Result = PrepareSlotToSave(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameMode>(this,WithGameInstance))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameMode>(this,WithGameInstance))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::LoadGameMode(UObject* Context, bool WithGameInstance, ESaviorResult &Result) {
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	FindSlotFile(Result);
	if (Result==ESaviorResult::Failed) {
		EVENT_OnFinishDataLOAD.Broadcast(false);
		AbortCurrentTask(); return;
	}
	//
	ReadSlotFromFile(Settings->DefaultPlayerID,Result);
	if (Result==ESaviorResult::Failed) {AbortCurrentTask(); return;}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameMode>(this,WithGameInstance))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameMode>(this,WithGameInstance))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::SaveGameInstance(UObject* Context, ESaviorResult &Result) {
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataSAVE.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	Result = PrepareSlotToSave(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameInstance>(this))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousSaving;
			(new FAutoDeleteAsyncTask<TASK_SerializeGameInstance>(this))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::LoadGameInstance(UObject* Context, ESaviorResult &Result) {
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return;}
	if (!CheckInstance()) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	if (USavior::ThreadSafety!=EThreadSafety::IsCurrentlyThreadSafe) {
		EVENT_OnFinishDataLOAD.Broadcast(false);
		Result = ESaviorResult::Failed;
	return;}
	//
	FindSlotFile(Result);
	if (Result==ESaviorResult::Failed) {
		EVENT_OnFinishDataLOAD.Broadcast(false);
		AbortCurrentTask(); return;
	}
	//
	ReadSlotFromFile(Settings->DefaultPlayerID,Result);
	if (Result==ESaviorResult::Failed) {AbortCurrentTask(); return;}
	//
	Result = PrepareSlotToLoad(Context);
	if (Result==ESaviorResult::Failed) {return;}
	//
	//
	switch (RuntimeMode) {
		case ERuntimeMode::SynchronousTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::SynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameInstance>(this))->StartSynchronousTask();
		}	break;
		case ERuntimeMode::BackgroundTask:
		{
			USavior::LastThreadState = USavior::ThreadSafety;
			USavior::ThreadSafety = EThreadSafety::AsynchronousLoading;
			(new FAutoDeleteAsyncTask<TASK_DeserializeGameInstance>(this))->StartBackgroundTask();
		}	break;
	default: break;}
}

void USavior::StaticLoadGameWorld(UWorld* InWorld) {
	USaviorSubsystem* System = nullptr;
	//
	if (UGameInstance*const&GI=InWorld->GetGameInstance()) {
		System = GI->GetSubsystem<USaviorSubsystem>();
	} if (System==nullptr) {return;}
	//
	ESaviorResult Result = ESaviorResult::Success;
	USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;
	if (System->StaticSlot->GetSaveLocation()==InWorld->GetName()) {
		System->StaticSlot->SetWorld(InWorld);
		System->StaticSlot->LoadGameWorld(InWorld,false,Result);		
	}
}

UMetaSlot* USavior::SlotMetaToOBJ() {
	auto Meta = NewObject<UMetaSlot>(GetWorld(),TEXT("SlotMeta"));
	//
	Meta->Chapter = SlotMeta.Chapter;
	Meta->Progress = SlotMeta.Progress;
	Meta->PlayTime = SlotMeta.PlayTime;
	Meta->SaveDate = SlotMeta.SaveDate;
	Meta->PlayerName = SlotMeta.PlayerName;
	Meta->PlayerLevel = SlotMeta.PlayerLevel;
	Meta->SaveLocation = SlotMeta.SaveLocation;
	//
	return Meta;
}

UDataSlot* USavior::SlotDataToOBJ() {
	auto Data = NewObject<UDataSlot>(GetWorld(),TEXT("SlotData"));
	//
	Data->Minimal = SlotData.Minimal;
	Data->Complex = SlotData.Complex;
	//
	return Data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Object Serialization Interface:

FSaviorRecord USavior::GenerateRecord_Object(const UObject* Object) {
	FSaviorRecord Record;
	FString Buffer;
	//
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	TSharedRef<TJsonWriter<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>>JBuffer = TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Buffer);
	//
	if (Object==nullptr) {return Record;}
	for (TFieldIterator<FProperty>PIT(Object->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		if (!IsSupportedProperty(Property)) {continue;}
		if ((Object->IsA(ULightComponent::StaticClass()))) {
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) {continue;}
		} else {
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame)) {continue;}
		}
		//
		const FString ID = Property->GetName();
		const bool IsSet = Property->IsA<FSetProperty>();
		const bool IsMap = Property->IsA<FMapProperty>();
		const bool IsInt = Property->IsA<FIntProperty>();
		const bool IsBool = Property->IsA<FBoolProperty>();
		const bool IsByte = Property->IsA<FByteProperty>();
		const bool IsEnum = Property->IsA<FEnumProperty>();
		const bool IsName = Property->IsA<FNameProperty>();
		const bool IsText = Property->IsA<FTextProperty>();
		const bool IsString = Property->IsA<FStrProperty>();
		const bool IsInt64 = Property->IsA<FInt64Property>();
		const bool IsArray = Property->IsA<FArrayProperty>();
		const bool IsFloat = Property->IsA<FFloatProperty>();
		const bool IsClass = Property->IsA<FClassProperty>();
		const bool IsDouble = Property->IsA<FDoubleProperty>();
		const bool IsStruct = Property->IsA<FStructProperty>();
		const bool IsObject = Property->IsA<FObjectProperty>();
		//
		if (IsBool && ID.Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
			Record.Destroyed = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue_InContainer(Object);
		continue;}
		//
		if (IsInt) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FIntProperty>(Property),Object)); continue;}
		if (IsBool) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FBoolProperty>(Property),Object)); continue;}
		if (IsByte) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FByteProperty>(Property),Object)); continue;}
		if (IsEnum) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FEnumProperty>(Property),Object)); continue;}
		if (IsName) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FNameProperty>(Property),Object)); continue;}
		if (IsText) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FTextProperty>(Property),Object)); continue;}
		if (IsString) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FStrProperty>(Property),Object)); continue;}
		if (IsFloat) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FFloatProperty>(Property),Object)); continue;}
		if (IsInt64) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FInt64Property>(Property),Object)); continue;}
		if (IsDouble) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FDoubleProperty>(Property),Object)); continue;}
		if (IsClass) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FClassProperty>(Property),Object)); continue;}
		if (IsObject) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FObjectProperty>(Property),Object)); continue;}
		//
		if (IsSet) {JSON->SetArrayField(ID,FPropertyToJSON(CastFieldChecked<FSetProperty>(Property),Object)); continue;}
		if (IsMap) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FMapProperty>(Property),Object)); continue;}
		if (IsArray) {JSON->SetArrayField(ID,FPropertyToJSON(CastFieldChecked<FArrayProperty>(Property),Object)); continue;}
		//
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::IgnoreCase))) {continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FTransform>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Transform)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FLinearColor>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::LColor)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector2D>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector2D)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==FRuntimeFloatCurve::StaticStruct())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Curve)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FRotator>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Rotator)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector3D)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FColor>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::FColor)); continue;} else
		if ((IsStruct)) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Struct)); continue;}
	}	FJsonSerializer::Serialize(JSON.ToSharedRef(),JBuffer);
	//
	//
	Record.Active = true;
	Record.GUID = FindGUID(Object);
	Record.ClassPath = Object->GetClass()->GetDefaultObject()->GetPathName();
	//
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	if (Object->IsA(UActorComponent::StaticClass())) {
		auto Component = CastChecked<UActorComponent>(Object);
		const auto &Scene = Cast<USceneComponent>(Component);
		//
		Record.FullName = Reflector::MakeComponentID(const_cast<UActorComponent*>(Component)).ToString();
		Record.Active = Component->IsActive();
		//
		if (!TAG_CNOTRAN(Component) && (Scene && Scene->Mobility==EComponentMobility::Movable)) {
			Record.Scale = Scene->GetRelativeTransform().GetScale3D();
			Record.Location = Scene->GetRelativeTransform().GetLocation();
			Record.Rotation = Scene->GetRelativeTransform().GetRotation().Rotator();
		}
		//
		if (!TAG_CNODATA(Component)) {Record.Buffer=Buffer;}
		if (TAG_CNOKILL(Component)) {Record.Destroyed=false;}
		//
		LOG_SV(Debug,ESeverity::Info,FString("Serialized Component :: ")+Record.FullName);
	} else if (Object->IsA(AActor::StaticClass())) {
		auto Actor = CastChecked<AActor>(Object);
		//
		Record.FullName = Reflector::MakeActorID(const_cast<AActor*>(Actor),true).ToString();
		//
		if (const auto &Avatar = Cast<ACharacter>(Actor)) {
			if (const auto &Skel = Avatar->GetMesh()) {
				Record.ActorMesh = TSoftObjectPtr<USkeletalMesh>(Skel->SkeletalMesh).ToString();
			}
		} else if (const auto &Mesh=Cast<UStaticMeshComponent>(Actor->GetRootComponent())) {
			Record.ActorMesh = TSoftObjectPtr<UStaticMesh>(Mesh->GetStaticMesh()).ToString();
		}
		//
		if (!TAG_ANOTRAN(Actor)) {
			if (auto*Parent=Actor->GetParentActor()) {
				Record.Scale = Actor->GetActorTransform().GetRelativeTransform(Parent->GetActorTransform()).GetScale3D();
				Record.Location = Actor->GetActorTransform().GetRelativeTransform(Parent->GetActorTransform()).GetLocation();
				Record.Rotation = Actor->GetActorTransform().GetRelativeTransform(Parent->GetActorTransform()).GetRotation().Rotator();
			} else {
				Record.Scale = Actor->GetActorTransform().GetScale3D();
				Record.Location = Actor->GetActorTransform().GetLocation();
				Record.Rotation = Actor->GetActorTransform().GetRotation().Rotator();
			}
		}
		//
		if (!TAG_ANOPHYS(Actor) && Actor->GetRootComponent()) {
			Record.LinearVelocity = Actor->GetVelocity();
			//
			if (const auto &Primitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent())) {
				Record.AngularVelocity=Primitive->GetPhysicsAngularVelocityInDegrees();
				Record.Active = Primitive->IsActive();
			}
		}
		//
		if (!TAG_ANODATA(Actor)) {Record.Buffer=Buffer;}
		if (TAG_ANOKILL(Actor)) {Record.Destroyed=false;}
		if (!TAG_ANOHIDE(Actor)) {Record.HiddenInGame=Actor->IsHidden();}
		//
		if (auto*Parent=Actor->GetParentActor()) {
			Record.OuterName = Reflector::MakeActorID(Parent).ToString();
		}
		//
		LOG_SV(Debug,ESeverity::Info,FString("Serialized Actor :: ")+Record.FullName);
	} else {
		Record.Buffer = Buffer;
	}
	//
	//
	LOG_SV(DeepLogs,ESeverity::Info,FString::Printf(TEXT("SAVED DATA for %s :: "),*Reflector::MakeObjectID(const_cast<UObject*>(Object)).ToString())+Buffer);
	//
	return Record;
}

FSaviorRecord USavior::GenerateRecord_Component(const UActorComponent* Component) {
	return GenerateRecord_Object(Component);
}

FSaviorRecord USavior::GenerateRecord_Actor(const AActor* Actor) {
	return GenerateRecord_Object(Actor);
}

void USavior::UnpackRecord_Object(const FSaviorRecord &Record, UObject* Object, ESaviorResult &Result) {
	if (Object==nullptr) {Result=ESaviorResult::Failed; return;}
	Result = ESaviorResult::Success;
	//
	const auto &Settings = GetDefault<USaviorSettings>();
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	TSharedRef<TJsonReader<TCHAR>>JReader = TJsonReaderFactory<TCHAR>::Create(Record.Buffer);
	//
	if (!FJsonSerializer::Deserialize(JReader,JSON)) {
		LOG_SV(Debug,ESeverity::Warning,FString::Printf(TEXT("[Data<->Buffer]: Corrupted Data. Unable to unpack Data Record for Object: [%s]"),*Object->GetName()));
		Result=ESaviorResult::Failed; return;
	}
	//
	//
	for (TFieldIterator<FProperty>PIT(Object->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		if (!IsSupportedProperty(Property)) {continue;}
		if ((Object->IsA(ULightComponent::StaticClass()))) {
			if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) {continue;}
		} else {
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame)) {continue;}
		}
		//
		const FString ID = Property->GetName();
		const bool IsSet = Property->IsA<FSetProperty>();
		const bool IsMap = Property->IsA<FMapProperty>();
		const bool IsInt = Property->IsA<FIntProperty>();
		const bool IsBool = Property->IsA<FBoolProperty>();
		const bool IsByte = Property->IsA<FByteProperty>();
		const bool IsEnum = Property->IsA<FEnumProperty>();
		const bool IsName = Property->IsA<FNameProperty>();
		const bool IsText = Property->IsA<FTextProperty>();
		const bool IsString = Property->IsA<FStrProperty>();
		const bool IsArray = Property->IsA<FArrayProperty>();
		const bool IsFloat = Property->IsA<FFloatProperty>();
		const bool IsInt64 = Property->IsA<FInt64Property>();
		const bool IsClass = Property->IsA<FClassProperty>();
		const bool IsDouble = Property->IsA<FDoubleProperty>();
		const bool IsStruct = Property->IsA<FStructProperty>();
		const bool IsObject = Property->IsA<FObjectProperty>();
		//
		if ((IsInt)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FIntProperty>(Property),Object); continue;}
		if ((IsBool)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FBoolProperty>(Property),Object); continue;}
		if ((IsByte)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FByteProperty>(Property),Object); continue;}
		if ((IsEnum)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FEnumProperty>(Property),Object); continue;}
		if ((IsName)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FNameProperty>(Property),Object); continue;}
		if ((IsText)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FTextProperty>(Property),Object); continue;}
		if ((IsString)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FStrProperty>(Property),Object); continue;}
		if ((IsFloat)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FFloatProperty>(Property),Object); continue;}
		if ((IsInt64)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FInt64Property>(Property),Object); continue;}
		if ((IsDouble)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FDoubleProperty>(Property),Object); continue;}
		//
		if ((IsSet)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FSetProperty>(Property),Object); continue;}
		if ((IsMap)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FMapProperty>(Property),Object); continue;}
		if ((IsArray)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FArrayProperty>(Property),Object); continue;}
		if ((IsClass)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FClassProperty>(Property),Object); continue;}
		if ((IsObject)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FObjectProperty>(Property),Object); continue;}
		//
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::IgnoreCase))) {continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FTransform>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Transform); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FLinearColor>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::LColor); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector2D>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector2D); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==FRuntimeFloatCurve::StaticStruct())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Curve); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FRotator>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Rotator); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector3D); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FColor>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::FColor); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Struct); continue;}
	}
	//
	//
	if (Object->IsA(UActorComponent::StaticClass())) {
		auto Component = CastChecked<UActorComponent>(Object);
		if (IsInGameThread()) {DeserializeComponent(Record,Component);} else {
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeComponent,Record,Component),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
				nullptr, ENamedThreads::GameThread
			);
		}
		//
		FString ID = Reflector::MakeComponentID(Component).ToString();
		LOG_SV(DeepLogs,ESeverity::Info,FString::Printf(TEXT("UNPACKED DATA for %s :: "),*ID)+Record.Buffer);
		LOG_SV(Debug,ESeverity::Info,FString("Deserialized :: ")+ID);
	} else if (Object->IsA(AActor::StaticClass())) {
		auto Actor = CastChecked<AActor>(Object);
		if (IsInGameThread()) {DeserializeActor(Record,Actor);} else {
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeActor,Record,Actor),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
				nullptr, ENamedThreads::GameThread
			);
		}
		//
		FString ID = Reflector::MakeActorID(Actor).ToString();
		LOG_SV(DeepLogs,ESeverity::Info,FString::Printf(TEXT("UNPACKED DATA for %s :: "),*ID)+Record.Buffer);
		LOG_SV(Debug,ESeverity::Info,FString("Deserialized :: ")+ID);
	} else {
		FString ID = Reflector::MakeObjectID(Object).ToString();
		LOG_SV(DeepLogs,ESeverity::Info,FString::Printf(TEXT("UNPACKED DATA for %s :: "),*ID)+Record.Buffer);
		LOG_SV(Debug,ESeverity::Info,FString("Deserialized :: ")+ID);
	}
}

void USavior::UnpackRecord_Component(const FSaviorRecord &Record, UActorComponent* Component, ESaviorResult &Result) {
	UnpackRecord_Object(Record,Component,Result);
}

void USavior::UnpackRecord_Actor(const FSaviorRecord &Record, AActor* Actor, ESaviorResult &Result) {
	UnpackRecord_Object(Record,Actor,Result);
}

FSaviorMinimal USavior::GenerateMinimalRecord_Object(const UObject * Object) {
	FSaviorMinimal Record;
	FString Buffer;
	//
	if (Object==nullptr) {return Record;}
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	TSharedRef<TJsonWriter<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>>JBuffer = TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Buffer);
	//
	//
	for (TFieldIterator<FProperty>PIT(Object->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		if (!Property->HasAnyPropertyFlags(CPF_SaveGame)) {continue;}
		//
		const FString ID = Property->GetName();
		const bool IsSet = Property->IsA<FSetProperty>();
		const bool IsMap = Property->IsA<FMapProperty>();
		const bool IsInt = Property->IsA<FIntProperty>();
		const bool IsBool = Property->IsA<FBoolProperty>();
		const bool IsByte = Property->IsA<FByteProperty>();
		const bool IsEnum = Property->IsA<FEnumProperty>();
		const bool IsName = Property->IsA<FNameProperty>();
		const bool IsText = Property->IsA<FTextProperty>();
		const bool IsString = Property->IsA<FStrProperty>();
		const bool IsArray = Property->IsA<FArrayProperty>();
		const bool IsFloat = Property->IsA<FFloatProperty>();
		const bool IsInt64 = Property->IsA<FInt64Property>();
		const bool IsClass = Property->IsA<FClassProperty>();
		const bool IsDouble = Property->IsA<FDoubleProperty>();
		const bool IsStruct = Property->IsA<FStructProperty>();
		const bool IsObject = Property->IsA<FObjectProperty>();
		//
		if (IsBool && ID.Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
			Record.Destroyed = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue_InContainer(Object);
		continue;}
		//
		if (IsInt) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FIntProperty>(Property),Object)); continue;}
		if (IsBool) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FBoolProperty>(Property),Object)); continue;}
		if (IsByte) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FByteProperty>(Property),Object)); continue;}
		if (IsEnum) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FEnumProperty>(Property),Object)); continue;}
		if (IsName) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FNameProperty>(Property),Object)); continue;}
		if (IsText) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FTextProperty>(Property),Object)); continue;}
		if (IsString) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FStrProperty>(Property),Object)); continue;}
		if (IsFloat) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FFloatProperty>(Property),Object)); continue;}
		if (IsInt64) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FInt64Property>(Property),Object)); continue;}
		if (IsDouble) {JSON->SetField(ID,FPropertyToJSON(CastFieldChecked<FDoubleProperty>(Property),Object)); continue;}
		if (IsClass) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FClassProperty>(Property),Object)); continue;}
		if (IsObject) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FObjectProperty>(Property),Object)); continue;}
		//
		if (IsSet) {JSON->SetArrayField(ID,FPropertyToJSON(CastFieldChecked<FSetProperty>(Property),Object)); continue;}
		if (IsMap) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FMapProperty>(Property),Object)); continue;}
		if (IsArray) {JSON->SetArrayField(ID,FPropertyToJSON(CastFieldChecked<FArrayProperty>(Property),Object)); continue;}
		//
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::IgnoreCase))) {continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FTransform>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Transform)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FLinearColor>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::LColor)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector2D>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector2D)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==FRuntimeFloatCurve::StaticStruct())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Curve)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FRotator>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Rotator)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector3D)); continue;} else
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FColor>::Get())) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::FColor)); continue;} else
		if ((IsStruct)) {JSON->SetObjectField(ID,FPropertyToJSON(CastFieldChecked<FStructProperty>(Property),Object,EStructType::Struct)); continue;}
	} FJsonSerializer::Serialize(JSON.ToSharedRef(),JBuffer);
	//
	//
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	if (Object->IsA(UActorComponent::StaticClass())) {
		auto Component = CastChecked<UActorComponent>(Object);
		const auto &Scene = Cast<USceneComponent>(Component);
		//
		if (!TAG_CNOTRAN(Component) && (Scene && Scene->Mobility==EComponentMobility::Movable)) {
			Record.Location = Scene->GetRelativeTransform().GetLocation();
			Record.Rotation = Scene->GetRelativeTransform().GetRotation().Rotator();
		}
		//
		if (TAG_CNOKILL(Component)) {Record.Destroyed=false;}
		if (!TAG_CNODATA(Component)) {Record.Buffer=Buffer;}
		//
		LOG_SV(Debug,ESeverity::Info,FString("(Minimal) Serialized Component :: ")+Component->GetName());
	} else if (Object->IsA(AActor::StaticClass())) {
		auto Actor = CastChecked<AActor>(Object);
		//
		if (!TAG_ANOTRAN(Actor)) {
			Record.Location = Actor->ActorToWorld().GetLocation();
			Record.Rotation = Actor->ActorToWorld().GetRotation().Rotator();
		}
		//
		if (TAG_ANOKILL(Actor)) {Record.Destroyed=false;}
		if (!TAG_ANODATA(Actor)) {Record.Buffer=Buffer;}
		//
		LOG_SV(Debug,ESeverity::Info,FString("(Minimal) Serialized Actor :: ")+Actor->GetName());
	} else {Record.Buffer=Buffer;}
	//
	//
	LOG_SV(DeepLogs,ESeverity::Info,FString::Printf(TEXT("SAVED MINIMAL DATA for %s :: "),*Object->GetName())+Buffer);
	//
	return Record;
}

FSaviorMinimal USavior::GenerateMinimalRecord_Component(const UActorComponent* Component) {
	return GenerateMinimalRecord_Object(Component);
}

FSaviorMinimal USavior::GenerateMinimalRecord_Actor(const AActor* Actor) {
	return GenerateMinimalRecord_Object(Actor);
}

void USavior::UnpackMinimalRecord_Object(const FSaviorMinimal &Record, UObject* Object, ESaviorResult &Result) {
	if (Object==nullptr) {Result=ESaviorResult::Failed; return;}
	//
	Result = ESaviorResult::Success;
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	TSharedRef<TJsonReader<TCHAR>>JReader = TJsonReaderFactory<TCHAR>::Create(Record.Buffer);
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	//
	if (!FJsonSerializer::Deserialize(JReader,JSON)) {
		LOG_SV(Debug,ESeverity::Warning,FString::Printf(TEXT("[Data<->Buffer]: Corrupted Data. Unable to unpack Data Record for Object: [%s]"),*Object->GetName()));
	}
	//
	//
	for (TFieldIterator<FProperty>PIT(Object->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		if (!Property->HasAnyPropertyFlags(CPF_SaveGame)) {continue;}
		//
		const FString ID = Property->GetName();
		const bool IsSet = Property->IsA<FSetProperty>();
		const bool IsMap = Property->IsA<FMapProperty>();
		const bool IsInt = Property->IsA<FIntProperty>();
		const bool IsBool = Property->IsA<FBoolProperty>();
		const bool IsByte = Property->IsA<FByteProperty>();
		const bool IsEnum = Property->IsA<FEnumProperty>();
		const bool IsName = Property->IsA<FNameProperty>();
		const bool IsText = Property->IsA<FTextProperty>();
		const bool IsString = Property->IsA<FStrProperty>();
		const bool IsArray = Property->IsA<FArrayProperty>();
		const bool IsFloat = Property->IsA<FFloatProperty>();
		const bool IsInt64 = Property->IsA<FInt64Property>();
		const bool IsClass = Property->IsA<FClassProperty>();
		const bool IsDouble = Property->IsA<FDoubleProperty>();
		const bool IsStruct = Property->IsA<FStructProperty>();
		const bool IsObject = Property->IsA<FObjectProperty>();
		//
		if ((IsInt)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FIntProperty>(Property),Object); continue;}
		if ((IsBool)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FBoolProperty>(Property),Object); continue;}
		if ((IsByte)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FByteProperty>(Property),Object); continue;}
		if ((IsEnum)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FEnumProperty>(Property),Object); continue;}
		if ((IsName)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FNameProperty>(Property),Object); continue;}
		if ((IsText)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FTextProperty>(Property),Object); continue;}
		if ((IsString)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FStrProperty>(Property),Object); continue;}
		if ((IsFloat)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FFloatProperty>(Property),Object); continue;}
		if ((IsInt64)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FInt64Property>(Property),Object); continue;}
		if ((IsDouble)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FDoubleProperty>(Property),Object); continue;}
		//
		if ((IsSet)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FSetProperty>(Property),Object); continue;}
		if ((IsMap)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FMapProperty>(Property),Object); continue;}
		if ((IsArray)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FArrayProperty>(Property),Object); continue;}
		if ((IsClass)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FClassProperty>(Property),Object); continue;}
		if ((IsObject)&&(JSON->HasField(ID))) {JSONToFProperty(this,JSON,ID,CastFieldChecked<FObjectProperty>(Property),Object); continue;}
		//
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::IgnoreCase))) {continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FTransform>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Transform); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FLinearColor>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::LColor); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector2D>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector2D); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==FRuntimeFloatCurve::StaticStruct())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Curve); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FRotator>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Rotator); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FVector>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Vector3D); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FColor>::Get())) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::FColor); continue;} else
		if ((IsStruct)&&(JSON->HasField(ID))) {JSONToFProperty(JSON,ID,CastFieldChecked<FStructProperty>(Property),Object,EStructType::Struct); continue;}
	}
	//
	//
	if (Object->IsA(UActorComponent::StaticClass())) {
		auto Component = CastChecked<UActorComponent>(Object);
		if (IsInGameThread()) {DeserializeComponent(Record,Component);} else {
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeComponent,Record,Component),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
				nullptr, ENamedThreads::GameThread
			);
		}
	} else if (Object->IsA(AActor::StaticClass())) {
		auto Actor = CastChecked<AActor>(Object);
		if (IsInGameThread()) {DeserializeActor(Record,Actor);} else {
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&DeserializeActor,Record,Actor),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeserializeEntity),
				nullptr, ENamedThreads::GameThread
			);
		}
	}
	//
	//
	LOG_SV(DeepLogs,ESeverity::Info,FString::Printf(TEXT("UNPACKED MINIMAL DATA for %s :: "),*Object->GetName())+Record.Buffer);
	LOG_SV(Debug,ESeverity::Info,FString("Deserialized :: ")+Object->GetName());
}

void USavior::UnpackMinimalRecord_Component(const FSaviorMinimal &Record, UActorComponent* Component, ESaviorResult &Result) {
	UnpackMinimalRecord_Object(Record,Component,Result);
}

void USavior::UnpackMinimalRecord_Actor(const FSaviorMinimal &Record, AActor* Actor, ESaviorResult &Result) {
	UnpackMinimalRecord_Object(Record,Actor,Result);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Utilities:

USavior* USavior::NewSlotInstance(UObject* Context, USavior* Slot, ESaviorResult &Result) {
	if (Context==nullptr||Context->GetWorld()==nullptr) {Result=ESaviorResult::Failed; return nullptr;}
	if (Slot==nullptr) {Result=ESaviorResult::Failed; return nullptr;}
	//
	USavior* Instance = nullptr;
	Result = ESaviorResult::Success;
	//
	if ((Slot->IsInstance()&&(Slot->GetThreadSafety()==EThreadSafety::IsCurrentlyThreadSafe))) {return Slot;} else {
		Instance = NewObject<USavior>(Context->GetWorld(),Slot->GetClass(),Slot->GetFName(),RF_Standalone,Slot);
	}
	//
	ShadowCopySlot(Slot,Instance,Result);
	Instance->SetWorld(Context->GetWorld());
	Instance->Instanced = true;
	//
	return Instance;
}

USavior* USavior::ShadowCopySlot(USavior* From, USavior* To, ESaviorResult &Result) {
	if (From==nullptr) {Result=ESaviorResult::Failed; return To;}
	if (To==nullptr) {Result=ESaviorResult::Failed; return To;}
	//
	Result = ESaviorResult::Success;
	To->SlotFileName = FText::FromString(From->GetSlotName());
	//
	To->SetSlotMeta(From->GetSlotMeta());
	To->SetSlotData(From->GetSlotData());
	//
	To->SlotThumbnail = From->SlotThumbnail;
	To->LevelThumbnails = From->LevelThumbnails;
	To->FeedbackFont = From->FeedbackFont;
	To->SplashStretch = From->SplashStretch;
	To->ProgressBarTint = From->ProgressBarTint;
	To->PauseGameOnLoad = From->PauseGameOnLoad;
	To->ProgressBarOnMovie = From->ProgressBarOnMovie;
	To->SplashMovie = From->SplashMovie;
	To->SplashImage = From->SplashImage;
	To->BackBlurPower = From->BackBlurPower;
	To->LoadScreenTimer = From->LoadScreenTimer;
	To->FeedbackLOAD = From->FeedbackLOAD;
	To->FeedbackSAVE = From->FeedbackSAVE;
	To->LoadScreenTrigger = From->LoadScreenTrigger;
	To->LoadScreenMode = From->LoadScreenMode;
	//
	To->ActorScope = From->ActorScope;
	To->ComponentScope = From->ComponentScope;
	To->ObjectScope = From->ObjectScope;
	To->Compression = From->Compression;
	To->RuntimeMode = From->RuntimeMode;
	To->DeepLogs = From->DeepLogs;
	To->Debug = From->Debug;
	//
	To->IgnorePawnTransformOnLoad = From->IgnorePawnTransformOnLoad;
	To->WriteMetaOnSave = From->WriteMetaOnSave;
	//
	To->World = From->World;
	To->Instanced = From->Instanced;
	//
	To->EVENT_OnBeginDataLOAD = From->EVENT_OnBeginDataLOAD;
	To->EVENT_OnBeginDataSAVE = From->EVENT_OnBeginDataSAVE;
	To->EVENT_OnFinishDataLOAD = From->EVENT_OnFinishDataLOAD;
	To->EVENT_OnFinishDataSAVE = From->EVENT_OnFinishDataSAVE;
	//
	return To;
}

UObject* USavior::GetClassDefaultObject(UClass* Class) {
	return Class->GetDefaultObject(true);
}

UObject* USavior::NewObjectInstance(UObject* Context, UClass* Class) {
	if (Context==nullptr) {return nullptr;}
	if (Class==nullptr) {return nullptr;}
	//
	auto World = GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::LogAndReturnNull);
	if (World) {return NewObject<UObject>(Context,Class);}
	//
	return nullptr;
}

UObject* USavior::NewNamedObjectInstance(UObject* Context, UClass* Class, FName Name) {
	if (Context==nullptr) {return nullptr;}
	if (Class==nullptr) {return nullptr;}
	//
	auto World = GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::LogAndReturnNull);
	if (World) {return NewObject<UObject>(Context,Class,Name);}
	//
	return nullptr;
}

FGuid USavior::NewObjectGUID() {
	return FGuid::NewGuid();
}

FGuid USavior::CreateSGUID(UObject* Context) {
	FGuid GUID = Reflector::FindGUID(Context);
	if (!GUID.IsValid()) {GUID=FGuid::NewGuid();}
	//
	return GUID;
}

bool USavior::MatchesGUID(UObject* Context, UObject* ComparedTo) {
	return Reflector::IsMatchingSGUID(Context,ComparedTo);
}

AActor* USavior::FindActorWithGUID(UObject* Context, const FGuid &SGUID) {
	if (Context->GetWorld()==nullptr) {return nullptr;}
	//
	UWorld* World = Context->GetWorld();
	for (TActorIterator<AActor>Actor(World); Actor; ++Actor) {
		if (Actor->HasAnyFlags(RF_BeginDestroyed)) {continue;}
		//
		const FGuid GUID = Reflector::FindGUID(*Actor);
		if (GUID==SGUID) {return (*Actor);}
	}
	//
	return nullptr;
}

FGuid USavior::MakeInputSGUID(const FString Base) {
	return FGuid(Base);
}

FGuid USavior::MakeLiteralSGUID(const int32 A, const int32 B, const int32 C, const int32 D) {
	return FGuid(
		(uint32)FMath::Abs(A),
		(uint32)FMath::Abs(B),
		(uint32)FMath::Abs(C),
		(uint32)FMath::Abs(D)
	);
}

FGuid USavior::MakeObjectGUID(UObject* Object, const EGuidGeneratorMode Mode) {
	if (Object==nullptr||(!Object->IsValidLowLevel())) {
		LOG_SV(true,ESeverity::Error,FString("Make Object SGUID:  target object is invalid.")); return FGuid{};
	}	UObject* Outer = Object->GetOuter();
	//
	FString Path = Object->GetFullName(Outer);
	Path = Reflector::SanitizeObjectPath(Path);
	//
	if (Path.Equals(TEXT("ERROR:ID"))) {return FGuid(0,0,0,0);}
	if (Object==nullptr) {return FGuid(0,0,0,0);}
	//
	uint32 iA=0, iB=0, iC=0, iD=0;
	const FString Hash = FMD5::HashAnsiString(*Path);
	{
		FString A{}, B{}, C{}, D{};
		for (int32 I=0; I<Hash.Len(); ++I) {
			if (I<=8) {A.AppendChar(Hash[I]); continue;}
			if ((I>8)&&(I<=16)) {B.AppendChar(Hash[I]); continue;}
			if ((I>16)&&(I<=24)) {C.AppendChar(Hash[I]); continue;}
			if (I>24) {D.AppendChar(Hash[I]); continue;}
		}
		//
		for (const TCHAR &CH : A) {iA+=CH;}
		for (const TCHAR &CH : B) {iB+=CH;}
		for (const TCHAR &CH : C) {iC+=CH;}
		for (const TCHAR &CH : D) {iD+=CH;}
	}
	//
	switch(Mode) {
		case EGuidGeneratorMode::ResolvedNameToGUID:
		{
			FGuid GuidCheck = FGuid(iA,iB,iC,iD);
			for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
				if (!OBJ->IsValidLowLevelFast()) {continue;}
				if ((*OBJ)->GetClass()!=Object->GetClass()) {continue;}
				if ((*OBJ)==Object) {continue;}
				//
				const FGuid OGUID = Reflector::FindGUID(*OBJ);
				if (OGUID!=GuidCheck) {continue;}
				//
				iA+=1; iB+=1; iC+=1; iD+=1;
			}
		}	break;
		//
		case EGuidGeneratorMode::MemoryAddressToGUID:
		{
			const UPTRINT IntPtr = reinterpret_cast<UPTRINT>(Object);
			iA+=IntPtr; iB+=IntPtr; iC+=IntPtr; iD+=IntPtr;
		}	break;
	default:break;}
	//
	return FGuid(iA,iB,iC,iD);
}

FGuid USavior::MakeComponentGUID(UActorComponent* Instance, const EGuidGeneratorMode Mode) {
	if ((Instance==nullptr)||(!Instance->IsValidLowLevel())) {
		LOG_SV(true,ESeverity::Error,FString("Make Component SGUID:  target component is invalid.")); return FGuid{};
	}	AActor* Outer = Instance->GetTypedOuter<AActor>();
	//
	FString Path = Instance->GetFullName(Outer);
	Path = Reflector::SanitizeObjectPath(Path);
	//
	if (Path.Equals(TEXT("ERROR:ID"))) {return FGuid(0,0,0,0);}
	if (Instance==nullptr) {return FGuid(0,0,0,0);}
	//
	uint32 iA=0, iB=0, iC=0, iD=0;
	const FString Hash = FMD5::HashAnsiString(*Path);
	{
		FString A{}, B{}, C{}, D{};
		for (int32 I=0; I<Hash.Len(); ++I) {
			if (I<=8) {A.AppendChar(Hash[I]); continue;}
			if ((I>8)&&(I<=16)) {B.AppendChar(Hash[I]); continue;}
			if ((I>16)&&(I<=24)) {C.AppendChar(Hash[I]); continue;}
			if (I>24) {D.AppendChar(Hash[I]); continue;}
		}
		//
		for (const TCHAR &CH : A) {iA+=CH;}
		for (const TCHAR &CH : B) {iB+=CH;}
		for (const TCHAR &CH : C) {iC+=CH;}
		for (const TCHAR &CH : D) {iD+=CH;}
	}
	//
	switch(Mode) {
		case EGuidGeneratorMode::ResolvedNameToGUID:
		{
			FGuid GuidCheck = FGuid(iA,iB,iC,iD);
			for (TObjectIterator<UActorComponent>CMP; CMP; ++CMP) {
				if ((*CMP)->GetClass()!=Instance->GetClass()) {continue;}
				if ((*CMP)==Instance) {continue;}
				//
				const FGuid OGUID = Reflector::FindGUID(*CMP);
				if (OGUID!=GuidCheck) {continue;}
				//
				iA+=1; iB+=1; iC+=1; iD+=1;
			}
		} break;
		//
		case EGuidGeneratorMode::MemoryAddressToGUID:
		{
			const UPTRINT IntPtr = reinterpret_cast<UPTRINT>(Instance);
			iA+=IntPtr; iB+=IntPtr; iC+=IntPtr; iD+=IntPtr;
		} break;
	default:break;}
	//
	return FGuid(iA,iB,iC,iD);
}

FGuid USavior::MakeActorGUID(AActor* Instance, const EGuidGeneratorMode Mode) {
	if ((Instance==nullptr)||(!Instance->IsValidLowLevel())) {
		LOG_SV(true,ESeverity::Error,FString("Make Actor SGUID:  target actor is invalid.")); return FGuid{};
	}	UWorld* Outer = Instance->GetTypedOuter<UWorld>();
	//
	FString Path = Instance->GetFullName(Outer);
	Path = Reflector::SanitizeObjectPath(Path);
	//
	if (Path.Equals(TEXT("ERROR:ID"))) {return FGuid(0,0,0,0);}
	if (Instance==nullptr) {return FGuid(0,0,0,0);}
	//
	uint32 iA=0, iB=0, iC=0, iD=0;
	const FString Hash = FMD5::HashAnsiString(*Path);
	{
		FString A{}, B{}, C{}, D{};
		for (int32 I=0; I<Hash.Len(); ++I) {
			if (I<=8) {A.AppendChar(Hash[I]); continue;}
			if ((I>8)&&(I<=16)) {B.AppendChar(Hash[I]); continue;}
			if ((I>16)&&(I<=24)) {C.AppendChar(Hash[I]); continue;}
			if (I>24) {D.AppendChar(Hash[I]); continue;}
		}
		//
		for (const TCHAR &CH : A) {iA+=CH;}
		for (const TCHAR &CH : B) {iB+=CH;}
		for (const TCHAR &CH : C) {iC+=CH;}
		for (const TCHAR &CH : D) {iD+=CH;}
	}
	//
	switch(Mode) {
		case EGuidGeneratorMode::ResolvedNameToGUID:
		{
			if (Instance->GetWorld()) {
				FGuid GuidCheck = FGuid(iA,iB,iC,iD);
				for (TActorIterator<AActor>ACT(Instance->GetWorld()); ACT; ++ACT) {
					if ((*ACT)->GetClass()!=Instance->GetClass()) {continue;}
					if ((*ACT)==Instance) {continue;}
					//
					const FGuid OGUID = Reflector::FindGUID(*ACT);
					if (OGUID!=GuidCheck) {continue;}
					//
					iA+=1; iB+=1; iC+=1; iD+=1;
				}
			}
		} break;
		//
		case EGuidGeneratorMode::MemoryAddressToGUID:
		{
			const UPTRINT IntPtr = reinterpret_cast<UPTRINT>(Instance);
			iA+=IntPtr; iB+=IntPtr; iC+=IntPtr; iD+=IntPtr;
		} break;
	default:break;}
	//
	return FGuid(iA,iB,iC,iD);
}

FGuid USavior::GetObjectGUID(UObject* Instance) {
	if (Instance==nullptr) {return FGuid();}
	//
	return Reflector::FindGUID(Instance);
}

FGuid USavior::GetComponentGUID(UActorComponent* Instance) {
	if (Instance==nullptr) {return FGuid();}
	//
	return Reflector::FindGUID(Instance);
}

FGuid USavior::GetActorGUID(AActor* Instance) {
	if (Instance==nullptr) {return FGuid();}
	//
	return Reflector::FindGUID(Instance);
}

float USavior::CalculateWorkload() const {
	float Workload=0.f;
	//
	for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
		if ((*OBJ)==nullptr) {continue;}
		if (!(*OBJ)->GetOutermost()->ContainsMap()) {continue;}
		if ((*OBJ)->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		//
		if ((*OBJ)->IsA(AActor::StaticClass())) {
			for (auto ACT : ActorScope) {
				if (ACT.Get()==nullptr) {continue;}
				if ((*OBJ)->IsA(ACT)) {Workload++; break;}
			}
		} else if ((*OBJ)->IsA(UActorComponent::StaticClass())) {
			for (auto COM : ComponentScope) {
				if (COM.Get()==nullptr) {continue;}
				if ((*OBJ)->IsA(COM)) {Workload++; break;}
			}
		} else {
			for (auto OBS : ObjectScope) {
				if (OBS.Get()==nullptr) {continue;}
				if ((*OBJ)->IsA(OBS)) {Workload++; break;}
			}
		}
	}
	//
	return Workload;
}

void USavior::ClearWorkload() {
	USavior::SS_Workload=0; USavior::SS_Complete=0; USavior::SS_Progress=100.f;
	USavior::SL_Workload=0; USavior::SL_Complete=0; USavior::SL_Progress=100.f;
	//
	//
	for (TActorIterator<AActor>Actor(World); Actor; ++Actor) {
		if ((*Actor)==nullptr) {continue;}
		if (!(*Actor)->GetOutermost()->ContainsMap()) {continue;}
		if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		//
		if (IsActorMarkedAutoDestroy(*Actor)) {
			(*Actor)->SetActorTickEnabled(false);
			(*Actor)->SetActorHiddenInGame(true);
			(*Actor)->SetActorEnableCollision(false);
			(*Actor)->Destroy();
		}
	}
	//
	for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
		if ((*OBJ)==nullptr) {continue;}
		if ((*OBJ)->IsA(AActor::StaticClass())) {continue;}
		if ((*OBJ)->IsA(UActorComponent::StaticClass())) {continue;}
		//
		if (!(*OBJ)->GetOutermost()->ContainsMap()) {continue;}
		if ((*OBJ)->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed)) {continue;}
		//
		if (IsObjectMarkedAutoDestroy(*OBJ)) {
			(*OBJ)->BeginDestroy();
		}
	}
	//
	//
	USavior::ThreadSafety = EThreadSafety::IsCurrentlyThreadSafe;
	USavior::LastThreadState = EThreadSafety::IsCurrentlyThreadSafe;
	//
	if (World) {
		static FTimerDelegate TimerDelegate = FTimerDelegate::CreateStatic(&StaticRemoveLoadScreen);
		World->GetTimerManager().SetTimer(USavior::TH_LoadScreen,TimerDelegate,LoadScreenTimer,false);
	} else {RemoveLoadScreen();}
}

void USavior::Reset() {
	SlotData.Minimal.Empty();
	SlotData.Complex.Empty();
	//
	SlotMeta.Progress = 0;
	SlotMeta.PlayTime = 0;
	SlotMeta.PlayerLevel = 0;
	SlotMeta.Chapter.Empty();
	SlotMeta.PlayerName.Empty();
	SlotMeta.SaveLocation.Empty();
	SlotMeta.SaveDate = FDateTime::Now();
	//
	WriteMetaOnSave = true;
	IgnorePawnTransformOnLoad = false;
	//
	SlotFileName = FText{};
}

ESaviorResult USavior::MarkObjectAutoDestroyed(UObject* Object) {
	if (Object) {
		bool HasProperty = false;
		//
		for (TFieldIterator<FProperty>PIT(Object->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
				auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
				CastFieldChecked<FBoolProperty>(Property)->SetPropertyValue(ValuePtr,true);
				HasProperty = true;
		break;}}
		//
		if (!HasProperty) {
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("'Auto-Destroy' function requires Target Object to have a Boolean Property named 'Destroyed'. Property wasn't found in this Class:  %s"),*Object->GetClass()->GetName()));
		return ESaviorResult::Failed;}
		//
		const auto &Interface = Cast<ISAVIOR_Serializable>(Object);
		if (Interface) {Interface->Execute_OnMarkedAutoDestroy(Object);}
		else if (Object->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())) {
			ISAVIOR_Serializable::Execute_OnMarkedAutoDestroy(Object);
		}
	} return ESaviorResult::Success;
}

bool USavior::IsObjectMarkedAutoDestroy(UObject* Object) {
	if (Object) {
		bool IsMarked = false;
		//
		for (TFieldIterator<FProperty>PIT(Object->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			//
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
				const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
				IsMarked = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue(ValuePtr);
		break;}}
		//
		return IsMarked;
	} return false;
}

ESaviorResult USavior::MarkComponentAutoDestroyed(UActorComponent* Component) {
	if (Component) {
		bool HasProperty = false;
		//
		for (TFieldIterator<FProperty>PIT(Component->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
				auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
				CastFieldChecked<FBoolProperty>(Property)->SetPropertyValue(ValuePtr,true);
				HasProperty = true;
		break;}}
		//
		if (!HasProperty) {
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("'Auto-Destroy' function requires Target Component to have a Boolean Property named 'Destroyed'. Property wasn't found in this Class:  %s"),*Component->GetClass()->GetName()));
		return ESaviorResult::Failed;}
		//
		const auto &Interface = Cast<ISAVIOR_Serializable>(Component);
		if (Interface) {Interface->Execute_OnMarkedAutoDestroy(Component);}
		else if (Component->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())) {
			ISAVIOR_Serializable::Execute_OnMarkedAutoDestroy(Component);
		}
	} return ESaviorResult::Success;
}

bool USavior::IsComponentMarkedAutoDestroy(UActorComponent* Component) {
	if (Component) {
		bool IsMarked = false;
		//
		for (TFieldIterator<FProperty>PIT(Component->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			//
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
				const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
				IsMarked = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue(ValuePtr);
		break;}}
		//
		return IsMarked;
	} return false;
}

ESaviorResult USavior::MarkActorAutoDestroyed(AActor* Actor) {
	if (Actor) {
		bool HasProperty = false;
		//
		for (TFieldIterator<FProperty>PIT(Actor->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
				auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
				CastFieldChecked<FBoolProperty>(Property)->SetPropertyValue(ValuePtr,true);
				HasProperty = true;
		break;}}
		//
		if (!HasProperty) {
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("'Auto-Destroy' function requires Target Actor to have a Boolean Property named 'Destroyed'. Property wasn't found in this Class:  %s"),*Actor->GetClass()->GetName()));
		return ESaviorResult::Failed;}
		//
		const auto &Interface = Cast<ISAVIOR_Serializable>(Actor);
		if (Interface) {Interface->Execute_OnMarkedAutoDestroy(Actor);}
		else if (Actor->GetClass()->ImplementsInterface(USAVIOR_Serializable::StaticClass())) {
			ISAVIOR_Serializable::Execute_OnMarkedAutoDestroy(Actor);
		}
	} return ESaviorResult::Success;
}

bool USavior::IsActorMarkedAutoDestroy(AActor* Actor) {
	if (Actor) {
		bool IsMarked = false;
		//
		for (TFieldIterator<FProperty>PIT(Actor->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
			FProperty* Property = *PIT;
			const bool IsBool = Property->IsA<FBoolProperty>();
			const auto PID = Property->GetFName();
			//
			if (IsBool && PID.ToString().Equals(TEXT("Destroyed"),ESearchCase::IgnoreCase)) {
				const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
				IsMarked = CastFieldChecked<FBoolProperty>(Property)->GetPropertyValue(ValuePtr);
		break;}}
		//
		return IsMarked;
	} return false;
}

void USavior::SetDefaultPlayerID(const int32 NewID) {
	const auto &Settings = GetMutableDefault<USaviorSettings>();
	//
	const uint32 ID = (NewID < 0) ? 0 : NewID;
	if (Settings) {Settings->DefaultPlayerID=ID;}
}

void USavior::SetDefaultPlayerLevel(const int32 NewLevel) {
	const auto &Settings = GetMutableDefault<USaviorSettings>();
	//
	const uint32 Level = (NewLevel < 0) ? 0 : NewLevel;
	if (Settings) {Settings->DefaultPlayerID=Level;}
}

void USavior::SetDefaultPlayerName(const FString NewName) {
	const auto &Settings = GetMutableDefault<USaviorSettings>();
	if (Settings) {Settings->DefaultPlayerName=NewName;}
}

void USavior::SetDefaultChapter(const FString NewChapter) {
	const auto &Settings = GetMutableDefault<USaviorSettings>();
	if (Settings) {Settings->DefaultChapter=NewChapter;}
}

void USavior::SetDefaultLocation(const FString NewLocation) {
	const auto &Settings = GetMutableDefault<USaviorSettings>();
	if (Settings) {Settings->DefaultLocation=NewLocation;}
}

UTexture2D* USavior::GetSlotThumbnail(FVector2D &TextureSize) {
	FString Name;
	//
	for (auto Level : LevelThumbnails) {
		if (Level.Value.IsNull()) {continue;}
		Level.Key.ToString().Split(TEXT("."),nullptr,&Name,ESearchCase::IgnoreCase,ESearchDir::FromEnd);
		//
		if (Name==SlotMeta.SaveLocation) {
			SlotThumbnail = Level.Value.ToSoftObjectPath();
		break;}
	}
	//
	auto OBJ = SlotThumbnail.TryLoad();
	if (UTexture2D*Image=Cast<UTexture2D>(OBJ)) {
		TextureSize = FVector2D(Image->GetSizeX(),Image->GetSizeY());
		return Image;
	}
	//
	return nullptr;
}

FString USavior::GetSaveTimeISO() {
	FString S;
	//
	SlotMeta.SaveDate.ToIso8601().Split("T",nullptr,&S);
	S.Split(".",&S,nullptr);
	//
	return S;
}

FString USavior::GetSaveDateISO() {
	FString S;
	//
	SlotMeta.SaveDate.ToIso8601().Split("T",&S,nullptr);
	//
	return S;
}

const FSlotMeta & USavior::GetSlotMetaData() const {
	return SlotMeta;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FName USavior::MakeActorID(AActor* Actor, bool FullPathID) {
	return Reflector::MakeActorID(Actor,FullPathID);
}

FName USavior::MakeComponentID(UActorComponent* Component, bool FullPathID) {
	return Reflector::MakeComponentID(Component,FullPathID);
}

FName USavior::MakeObjectID(UObject* Object, bool FullPathID) {
	return Reflector::MakeObjectID(Object,FullPathID);
}

bool USavior::IsRecordEmpty(const FSaviorRecord &Record) {
	return (
		Record.Buffer.IsEmpty() &&
		Record.FullName.IsEmpty() &&
		Record.ClassPath.IsEmpty() &&
		Record.ActorMesh.IsEmpty() &&
		Record.OuterName.IsEmpty() &&
		(Record.Materials.Num()==0) &&
		(Record.Scale==FVector::ZeroVector) &&
		(Record.Location==FVector::ZeroVector) &&
		(Record.Rotation==FRotator::ZeroRotator) &&
		(Record.LinearVelocity==FVector::ZeroVector) &&
		(Record.AngularVelocity==FVector::ZeroVector) &&
		(!Record.GUID.IsValid())
	);
}

bool USavior::IsRecordMarkedDestroy(const FSaviorRecord &Record) {
	return Record.Destroyed;
}

TMap<FName,FSaviorRecord> USavior::GetSlotDataCopy() const {
	return SlotData.Complex;
}

FSaviorRecord USavior::FindRecordByName(const FName &ID) const {
	return SlotData.Complex.FindRef(ID);
}

FSaviorRecord USavior::FindRecordByGUID(const FGuid &ID) const {
	for (auto IT = SlotData.Complex.CreateConstIterator(); IT; ++IT) {
		const FSaviorRecord &Record = IT->Value;
		if (Record.GUID==ID) {return Record;}
	}
	//
	return FSaviorRecord{};
}

void USavior::AppendDataRecords(const TMap<FName, FSaviorRecord>&Records, ESaviorResult &Result) {
	if (Records.Num()==0) {Result=ESaviorResult::Failed; return;}
	//
	SlotData.Complex.Append(Records);
	//
	Result = ESaviorResult::Success;
}

void USavior::InsertDataRecord(const FName &ID, const FSaviorRecord &Record, ESaviorResult &Result) {
	if (IsRecordEmpty(Record)) {Result=ESaviorResult::Failed; return;}
	if (ID==NAME_None) {Result=ESaviorResult::Failed; return;}
	//
	SlotData.Complex.Add(ID,Record);
	//
	Result = ESaviorResult::Success;
}

void USavior::RemoveDataRecordByName(const FName &ID, ESaviorResult &Result) {
	if (!SlotData.Complex.Contains(ID)) {Result=ESaviorResult::Failed; return;}
	//
	SlotData.Complex.Remove(ID);
	//
	Result = ESaviorResult::Success;
}

void USavior::RemoveDataRecordByGUID(const FGuid &ID, ESaviorResult &Result) {
	FName Item = NAME_None;
	//
	for (auto IT = SlotData.Complex.CreateConstIterator(); IT; ++IT) {
		const FSaviorRecord &Record = IT->Value;
		if (Record.GUID==ID){Item=IT->Key;break;}
	} if (!Item.IsNone()) {
		SlotData.Complex.Remove(Item);
		Result = ESaviorResult::Success;
	}
	//
	Result = ESaviorResult::Failed;
}

void USavior::RemoveDataRecordByRef(const FSaviorRecord &Record, ESaviorResult &Result) {
	FName Item = NAME_None;
	//
	for (auto IT = SlotData.Complex.CreateConstIterator(); IT; ++IT) {
		const FSaviorRecord &ITR = IT->Value;
		if (ITR==Record){Item=IT->Key;break;}
	} if (!Item.IsNone()) {
		SlotData.Complex.Remove(Item);
		Result = ESaviorResult::Success;
	}
	//
	Result = ESaviorResult::Failed;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ParseActorTAG(const AActor* Actor, const FName Tag) {
	return Actor->ActorHasTag(Tag);
}

bool ParseComponentTAG(const UActorComponent* Component, const FName Tag) {
	return Component->ComponentHasTag(Tag);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void USavior::SetProgress(float New) {
	SlotMeta.Progress = New;
}

void USavior::SetPlayTime(int32 New) {
	SlotMeta.PlayTime = New;
}

void USavior::SetChapter(FString New) {
	SlotMeta.Chapter = New;
}

void USavior::SetPlayerLevel(int32 New) {
	SlotMeta.PlayerLevel = New;
}

void USavior::SetSaveDate(FDateTime New) {
	SlotMeta.SaveDate = New;
}

void USavior::SetPlayerName(FString New) {
	SlotMeta.PlayerName = New;
}

void USavior::SetSaveLocation(FString New) {
	SlotMeta.SaveLocation = New;
}

float USavior::GetProgress() const {
	return SlotMeta.Progress;
}

int32 USavior::GetPlayTime() const {
	return SlotMeta.PlayTime;
}

FString USavior::GetChapter() const {
	return SlotMeta.Chapter;
}

int32 USavior::GetPlayerLevel() const {
	return SlotMeta.PlayerLevel;
}

FDateTime USavior::GetSaveDate() const {
	return SlotMeta.SaveDate;
}

FString USavior::GetPlayerName() const {
	return SlotMeta.PlayerName;
}

FString USavior::GetSaveLocation() const {
	return SlotMeta.SaveLocation;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Savior Loading-Screen Interface:

void USavior::LaunchLoadScreen(const EThreadSafety Mode, const FText Info) {
	if (GEngine==nullptr||World==nullptr) {return;}
	//
	const auto &PC = World->GetFirstPlayerController();
	//
	if (PC) {
		const auto &HUD = Cast<AHUD_SaviorUI>(PC->GetHUD());
		if (HUD) {
			switch (LoadScreenMode) {
				case ELoadScreenMode::BackgroundBlur:
				{
					HUD->DisplayBlurLoadScreenHUD(Mode,Info,FeedbackFont,ProgressBarTint,BackBlurPower);
				}	break;
				//
				case ELoadScreenMode::SplashScreen:
				{
					HUD->DisplaySplashLoadScreenHUD(Mode,Info,FeedbackFont,ProgressBarTint,SplashImage,SplashStretch);
				}	break;
				//
				case ELoadScreenMode::MoviePlayer:
				{
					HUD->DisplayMovieLoadScreenHUD(Mode,Info,FeedbackFont,ProgressBarTint,SplashMovie,ProgressBarOnMovie);
				}	break;
			default: break;}
		}
	}
}

void USavior::RemoveLoadScreen() {
	StaticRemoveLoadScreen();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void USaviorSubsystem::Initialize(FSubsystemCollectionBase &Collection) {
	StaticSlot = NewObject<USavior>(this,TEXT("StaticSlot"));
	//
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(StaticSlot,&USavior::StaticLoadGameWorld);
}

void USaviorSubsystem::Deinitialize() {
	StaticSlot->ConditionalBeginDestroy();
}

void USaviorSubsystem::Reset() {
	StaticSlot->SlotData.Minimal.Empty();
	StaticSlot->SlotData.Complex.Empty();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////