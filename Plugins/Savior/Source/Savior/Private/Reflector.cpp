//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2022 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Reflector.h"
#include "Savior.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Profiler Routines:

DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.JSONToFProperty"),STAT_FSimpleDelegateGraphTask_JSONToFProperty,STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.JSONSetToFProperty"),STAT_FSimpleDelegateGraphTask_JSONSetToFProperty,STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ParseJSONtoFStruct"),STAT_FSimpleDelegateGraphTask_ParseJSONtoFStruct,STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.JSONArrayToFProperty"),STAT_FSimpleDelegateGraphTask_JSONArrayToFProperty,STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.JSONMapKeyToFProperty"),STAT_FSimpleDelegateGraphTask_JSONMapKeyToFProperty,STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.JSONMapValueToFProperty"),STAT_FSimpleDelegateGraphTask_JSONMapValueToFProperty,STATGROUP_TaskGraphTasks);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Reflector Debug Routines:

DEFINE_LOG_CATEGORY(SaviorLog);

void LOG_SV(const bool Debug, const ESeverity Severity, const TCHAR* Message) {
	if (!Debug) {return;}
	//
	switch (Severity) {
		case ESeverity::Info:
			UE_LOG(SaviorLog,Log,TEXT("{S}:: %s"),Message); break;
		case ESeverity::Warning:
			UE_LOG(SaviorLog,Warning,TEXT("{S}:: %s"),Message); break;
		case ESeverity::Error:
			UE_LOG(SaviorLog,Error,TEXT("{S}:: %s"),Message); break;
		case ESeverity::Fatal:
			UE_LOG(SaviorLog,Fatal,TEXT("{S}:: %s"),Message); break;
	default: break;}
}

void LOG_SV(const bool Debug, const ESeverity Severity, const FString Message) {
	if (!Debug) {return;}
	//
	switch (Severity) {
		case ESeverity::Info:
			UE_LOG(SaviorLog,Log,TEXT("{S}:: %s"),*Message); break;
		case ESeverity::Warning:
			UE_LOG(SaviorLog,Warning,TEXT("{S}:: %s"),*Message); break;
		case ESeverity::Error:
			UE_LOG(SaviorLog,Error,TEXT("{S}:: %s"),*Message); break;
		case ESeverity::Fatal:
			UE_LOG(SaviorLog,Fatal,TEXT("{S}:: %s"),*Message); break;
	default: break;}
}

void LOG_SV(const bool Debug, const ESeverity Severity, const FName Message) {
	if (!Debug) {return;}
	//
	switch (Severity) {
		case ESeverity::Info:
			UE_LOG(SaviorLog,Log,TEXT("{S}:: %s"),*Message.ToString()); break;
		case ESeverity::Warning:
			UE_LOG(SaviorLog,Warning,TEXT("{S}:: %s"),*Message.ToString()); break;
		case ESeverity::Error:
			UE_LOG(SaviorLog,Error,TEXT("{S}:: %s"),*Message.ToString()); break;
		case ESeverity::Fatal:
			UE_LOG(SaviorLog,Fatal,TEXT("{S}:: %s"),*Message.ToString()); break;
	default: break;}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Reflector Utilities:

bool Reflector::IsSupportedProperty(const FProperty* Property) {
	return (
		Property->IsA<FStrProperty>()  ||
		Property->IsA<FIntProperty>()  ||
		Property->IsA<FSetProperty>()  ||
		Property->IsA<FMapProperty>()  ||
		Property->IsA<FBoolProperty>()  ||
		Property->IsA<FByteProperty>()  ||
		Property->IsA<FEnumProperty>()  ||
		Property->IsA<FNameProperty>()  ||
		Property->IsA<FTextProperty>()  ||
		Property->IsA<FFloatProperty>() ||
		Property->IsA<FInt64Property>()  ||
		Property->IsA<FArrayProperty>()  ||
		Property->IsA<FClassProperty>()  ||
		Property->IsA<FDoubleProperty>() ||
		Property->IsA<FStructProperty>()  ||
		Property->IsA<FObjectProperty>()
	);
}

bool Reflector::IsSafeToRespawnInstance(const UObject* CDO) {
	const auto &Settings = GetDefault<USaviorSettings>();
	//
	bool IsValidClass = false;
	for (auto INS : Settings->InstanceScope) {
		if (INS.Get()==nullptr) {continue;}
		if (CDO->IsA(INS)) {IsValidClass=true; break;}
	} return IsValidClass;
}

const FString Reflector::SanitizeObjectPath(const FString &InPath) {
	const TCHAR* BadCH = INVALID_OBJECTNAME_CHARACTERS;
	//
	FString Sanitize = InPath;
	//
	while (*BadCH) {
		Sanitize.ReplaceCharInline(*BadCH,TCHAR('_'),ESearchCase::CaseSensitive); ++BadCH;
	} return Sanitize;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Procedural Routines:

void Reflector::RespawnActorFromData(UWorld* World, const TSet<TSubclassOf<UObject>>RespawnScope, const FSaviorRecord Data) {
	if (World==nullptr) {return;}
	check(IsInGameThread());
	//
	if (Data.FullName.IsEmpty()||Data.FullName.Equals(TEXT("nullptr"))) {return;}
	if (Data.ClassPath.IsEmpty()||Data.ClassPath.Equals(TEXT("nullptr"))) {return;}
	//
	bool GUID = false;
	for (TActorIterator<AActor>Actor(World); Actor; ++Actor) {
		if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
		if (Reflector::MakeActorID(*Actor,true).ToString()==Data.FullName) {GUID=true; break;}
		if (IsMatchingSGUID(Data.GUID,*Actor)) {GUID=true; break;}
	} if (GUID) {return;}
	//
	UObject* OTR = nullptr;
	for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
		if (Reflector::MakeObjectID(*OBJ).ToString()==Data.OuterName) {OTR=(*OBJ); break;}
	}
	//
	const FSoftObjectPath ClassPath(Data.ClassPath);
	const auto CDO = ClassPath.ResolveObject();
	//
	if (CDO) {
		if (!CDO->IsA(AActor::StaticClass())) {return;}
		const auto Class = CDO->GetClass();
		//
		bool IsValidClass = false;
		for (auto RES : RespawnScope) {
			if (RES.Get()==nullptr) {continue;}
			if (CDO->IsA(RES)) {IsValidClass=true; break;}
		} if (!IsValidClass) {return;}
		//
		FActorSpawnParameters Parameters;
		Parameters.Owner = Cast<AActor>(OTR);
		Parameters.Template = CastChecked<AActor>(CDO);
		//
		FTransform Transform(Data.Rotation,Data.Location,Data.Scale);
		const auto ACT = World->SpawnActorDeferred<AActor>(Class,Transform,Parameters.Owner,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		//
		if (ACT) {
			const auto &IT = Cast<ISAVIOR_Procedural>(ACT);
			//
			if (IT) {
				IT->Execute_OnBeginRespawn(ACT,Data);
			} else if (ACT->GetClass()->ImplementsInterface(USAVIOR_Procedural::StaticClass())) {
				ISAVIOR_Procedural::Execute_OnBeginRespawn(ACT,Data);
			}
			//
			ACT->FinishSpawning(Transform,false);
			if (auto*Root=ACT->GetRootComponent()) {
				if (Parameters.Owner) {
					FAttachmentTransformRules Attach(EAttachmentRule::KeepRelative,false);
					ACT->AttachToActor(Parameters.Owner,Attach);
				}
			}	ACT->SetActorRelativeLocation(Data.Location,false,nullptr,ETeleportType::TeleportPhysics);
			//
			ACT->SetActorRelativeRotation(Data.Rotation);
			ACT->SetActorRelativeScale3D(Data.Scale);
			//
			SetGUID(Data.GUID,ACT);
			//
			if ((ACT)&&(IT)) {
				IT->Execute_OnFinishRespawn(ACT,Data);
			} else if ((ACT)&&(ACT->GetClass()->ImplementsInterface(USAVIOR_Procedural::StaticClass()))) {
				ISAVIOR_Procedural::Execute_OnFinishRespawn(ACT,Data);
			}
			//
			TInlineComponentArray<UActorComponent*>ComponentList;
			ACT->GetComponents(ComponentList);
			ESaviorResult Result;
			//
			for (const auto &CMP : ComponentList) {
				USavior::StaticLoadComponent(World,CMP,Result);
			}
			//
			USavior::StaticLoadActor(World,ACT,Result);
		}
	}
}

void Reflector::RespawnComponentFromData(UWorld* World, const TSet<TSubclassOf<UObject>>RespawnScope, const FSaviorRecord Data) {
	if (World==nullptr) {return;}
	check(IsInGameThread());
	//
	if (Data.FullName.IsEmpty()||Data.FullName.Equals(TEXT("nullptr"))) {return;}
	if (Data.ClassPath.IsEmpty()||Data.ClassPath.Equals(TEXT("nullptr"))) {return;}
	if (Data.OuterName.IsEmpty()||Data.OuterName.Equals(TEXT("nullptr"))) {return;}
	//
	bool GUID = false;
	for (TObjectIterator<UActorComponent>OBJ; OBJ; ++OBJ) {
		if ((*OBJ)->HasAnyFlags(RF_DefaultSubObject|RF_BeginDestroyed)) {continue;}
		if (Reflector::MakeComponentID(*OBJ,true).ToString()==Data.FullName) {GUID=true; break;}
		if ((Data.GUID.IsValid())&&(IsMatchingSGUID(Data.GUID,*OBJ))) {GUID=true; break;}
	} if (GUID) {return;}
	//
	UObject* OTR = nullptr;
	for (TObjectIterator<UObject>OBJ; OBJ; ++OBJ) {
		if (Reflector::MakeObjectID(*OBJ).ToString()==Data.OuterName) {OTR=(*OBJ); break;}
	}
	//
	const FSoftObjectPath ClassPath(Data.ClassPath);
	const auto CDO = ClassPath.ResolveObject();
	//
	if ((CDO)&&(OTR)) {
		if (!CDO->IsA(UActorComponent::StaticClass())) {return;}
		//
		FString Name{};
		bool IsValidClass = false;
		//
		for (auto RES : RespawnScope) {
			if (RES.Get()==nullptr) {continue;}
			if (CDO->IsA(RES)) {IsValidClass=true; break;}
		} if (!IsValidClass) {return;}
		//
		if (AActor*ACT=Cast<AActor>(OTR)) {
			Data.FullName.Split(TEXT("."),nullptr,&Name,ESearchCase::IgnoreCase,ESearchDir::FromEnd);
			const auto &CMP = NewObject<UActorComponent>(OTR,CDO->GetClass(),*Name,RF_NoFlags,CDO);
			//
			if (CMP) {
				const auto &IT = Cast<ISAVIOR_Procedural>(CMP);
				//
				if (IT) {
					IT->Execute_OnBeginRespawn(CMP,Data);
				} else if (CMP->GetClass()->ImplementsInterface(USAVIOR_Procedural::StaticClass())) {
					ISAVIOR_Procedural::Execute_OnBeginRespawn(CMP,Data);
				}
				//
				FTransform Transform(Data.Rotation,Data.Location,Data.Scale);
				ACT->FinishAddComponent(CMP,false,Transform);
				//
				if ((CMP)&&(IT)) {
					IT->Execute_OnFinishRespawn(CMP,Data);
				} else if ((CMP)&&(CMP->GetClass()->ImplementsInterface(USAVIOR_Procedural::StaticClass()))) {
					ISAVIOR_Procedural::Execute_OnFinishRespawn(CMP,Data);
				}
				//
				ESaviorResult Result;
				SetGUID(Data.GUID,CMP);
				USavior::StaticLoadComponent(World,CMP,Result);
			}
		}
	}
}

FMaterialRecord Reflector::CaptureMaterialSnapshot(UMaterialInstanceDynamic* Instance) {
	AActor* Outer = Instance->GetTypedOuter<AActor>();
	FMaterialRecord Record;
	//
	if (Outer==nullptr||!Outer->IsValidLowLevel()) {return Record;}
	if (Instance==nullptr||!Instance->IsValidLowLevel()) {return Record;}
	//
	if (Outer->GetWorld()==nullptr) {return Record;}
	if (!Outer->GetWorld()->IsGameWorld()) {return Record;}
	//
	TArray<FMaterialParameterInfo>TextureInfo;
	TArray<FMaterialParameterInfo>ScalarInfo;
	TArray<FMaterialParameterInfo>VectorInfo;
	TArray<FGuid>TextureID;
	TArray<FGuid>ScalarID;
	TArray<FGuid>VectorID;
	//
	Instance->GetAllScalarParameterInfo(ScalarInfo,ScalarID);
	Instance->GetAllVectorParameterInfo(VectorInfo,VectorID);
	Instance->GetAllTextureParameterInfo(TextureInfo,TextureID);
	//
	for (int32 I=0; I<ScalarID.Num(); I++) {
		if (!ScalarInfo.IsValidIndex(I)) {continue;}
		auto ParamInfo = ScalarInfo[I];
		float ParamValue = 0;
		//
		if (Instance->GetScalarParameterValue(ParamInfo,ParamValue)) {
			FMatScalarRecord Info;
			//
			Info.ParamValue = ParamValue;
			Info.ParamName = ParamInfo.Name;
			//
			Record.ScalarParams.Add(ScalarID[I],Info);
		}
	}
	//
	for (int32 I=0; I<VectorID.Num(); I++) {
		if (!VectorInfo.IsValidIndex(I)) {continue;}
		auto ParamInfo = VectorInfo[I];
		FLinearColor ParamValue;
		//
		if (Instance->GetVectorParameterValue(ParamInfo,ParamValue)) {
			FMatVectorRecord Info;
			//
			Info.ParamValue = ParamValue;
			Info.ParamName = ParamInfo.Name;
			//
			Record.VectorParams.Add(VectorID[I],Info);
		}
	}
	//
	for (int32 I=0; I<TextureID.Num(); I++) {
		if (!TextureInfo.IsValidIndex(I)) {continue;}
		auto ParamInfo = TextureInfo[I];
		UTexture* ParamValue;
		//
		if (Instance->GetTextureParameterValue(ParamInfo,ParamValue)) {
			FMatTextureRecord Info;
			//
			Info.ParamName = ParamInfo.Name;
			Info.ParamValue = FSoftObjectPath(ParamValue);
			//
			Record.TextureParams.Add(TextureID[I],Info);
		}
	}
	//
	return Record;
}

bool Reflector::RestoreMaterialFromSnapshot(UMaterialInstanceDynamic* Instance, const FString &OuterID, const FSaviorRecord &Data) {
	const FName ID = *FString::Printf(TEXT("%s.%s"),*OuterID,*Instance->GetName());
	if (!Data.Materials.Contains(ID)) {return false;}
	//
	const FMaterialRecord &MatInfo = Data.Materials.FindChecked(ID);
	TArray<FMaterialParameterInfo>TextureInfo;
	TArray<FMaterialParameterInfo>ScalarInfo;
	TArray<FMaterialParameterInfo>VectorInfo;
	TArray<FGuid>TextureID;
	TArray<FGuid>ScalarID;
	TArray<FGuid>VectorID;
	//
	Instance->GetAllScalarParameterInfo(ScalarInfo,ScalarID);
	Instance->GetAllVectorParameterInfo(VectorInfo,VectorID);
	Instance->GetAllTextureParameterInfo(TextureInfo,TextureID);
	//
	for (int32 I=0; I<ScalarID.Num(); I++) {
		if (!MatInfo.ScalarParams.Contains(ScalarID[I])) {continue;}
		if (!ScalarInfo.IsValidIndex(I)) {continue;}
		//
		auto &ParamInfo = ScalarInfo[I];
		auto &ParamData = MatInfo.ScalarParams.FindChecked(ScalarID[I]);
		//
		if (ParamInfo.Name != ParamData.ParamName) {
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Outdated Parameter (%s) Data in Storage:  \"%s\" --> \"%s\" , variable name didn't match. Skipping..."),*ScalarID[I].ToString(),*ParamData.ParamName.ToString(),*ParamInfo.Name.ToString()));
		continue;}
		//
		Instance->SetScalarParameterValue(ParamInfo.Name,ParamData.ParamValue);
	}
	//
	for (int32 I=0; I<VectorID.Num(); I++) {
		if (!MatInfo.VectorParams.Contains(VectorID[I])) {continue;}
		if (!VectorInfo.IsValidIndex(I)) {continue;}
		//
		auto &ParamInfo = VectorInfo[I];
		auto &ParamData = MatInfo.VectorParams.FindChecked(VectorID[I]);
		//
		if (ParamInfo.Name != ParamData.ParamName) {
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Outdated Parameter (%s) Data in Storage:  \"%s\" --> \"%s\" , variable name didn't match. Skipping..."),*VectorID[I].ToString(),*ParamData.ParamName.ToString(),*ParamInfo.Name.ToString()));
		continue;}
		//
		Instance->SetVectorParameterValue(ParamInfo.Name,ParamData.ParamValue);
	}
	//
	for (int32 I=0; I<TextureID.Num(); I++) {
		if (!MatInfo.TextureParams.Contains(TextureID[I])) {continue;}
		if (!TextureInfo.IsValidIndex(I)) {continue;}
		//
		auto &ParamInfo = TextureInfo[I];
		auto &ParamData = MatInfo.TextureParams.FindChecked(TextureID[I]);
		//
		if (ParamInfo.Name != ParamData.ParamName) {
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Outdated Parameter (%s) Data in Storage:  \"%s\" --> \"%s\" , variable name didn't match. Skipping..."),*TextureID[I].ToString(),*ParamData.ParamName.ToString(),*ParamInfo.Name.ToString()));
		continue;}
		//
		if (ParamData.ParamValue.IsValid()) {
			UTexture* TextureParam = Cast<UTexture>(ParamData.ParamValue.ResolveObject());
			if (TextureParam==nullptr) {TextureParam=Cast<UTexture>(ParamData.ParamValue.TryLoad());}
			if (TextureParam!=nullptr) {Instance->SetTextureParameterValue(ParamInfo.Name,TextureParam);}
		}
	}
	//
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Property Reflector:

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FBoolProperty* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const bool Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueBoolean(Value));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FByteProperty* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const uint8 Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueNumber(Value));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FIntProperty* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const int32 Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueNumber(Value));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FInt64Property* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const int64 Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueNumber(Value));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FFloatProperty* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const float Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueNumber(Value));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FDoubleProperty* Property,const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const float Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueNumber(Value));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FEnumProperty* Property, const UObject* Container) {
	FString Output = TEXT("None");
	UEnum* Enum = Property->GetEnum();
	//
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const FString Value = Enum->GetNameStringByValue(Property->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(ValuePtr));
	//
	return MakeShareable(new FJsonValueString(Value));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FNameProperty* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const FName Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueString(Value.ToString()));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FTextProperty* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const FText Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueString(Value.ToString()));
}

TSharedPtr<FJsonValue>Reflector::FPropertyToJSON(FStrProperty* Property, const UObject* Container) {
	const auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const FString Value = Property->GetPropertyValue(ValuePtr);
	//
	return MakeShareable(new FJsonValueString(Value));
}

TSharedPtr<FJsonObject>Reflector::FPropertyToJSON(FObjectProperty* Property, const UObject* Container) {
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	const auto OBJ = *Property->ContainerPtrToValuePtr<UObject*>(Container);
	//
	if (OBJ) {
		FString FullName;
		if (OBJ->IsA(AActor::StaticClass())) {
			FullName = Reflector::MakeActorID(CastChecked<AActor>(OBJ),true).ToString();
		} else if (OBJ->IsA(UActorComponent::StaticClass())) {
			FullName = Reflector::MakeComponentID(CastChecked<UActorComponent>(OBJ),true).ToString();
		} else {FullName=Reflector::MakeObjectID(OBJ).ToString();}
		//
		const auto ClassPath = OBJ->GetClass()->GetDefaultObject()->GetPathName();
		//
		JSON->SetStringField(TEXT("FullName"),FullName);
		JSON->SetStringField(TEXT("ClassPath"),ClassPath);
	} else {
		JSON->SetStringField(TEXT("FullName"),TEXT("nullptr"));
		JSON->SetStringField(TEXT("ClassPath"),TEXT("nullptr"));
	}
	//
	return JSON;
}

TSharedPtr<FJsonObject>Reflector::FPropertyToJSON(FClassProperty* Property, const UObject* Container) {
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	const auto CLS = *Property->ContainerPtrToValuePtr<UClass*>(Container);
	//
	if (CLS) {
		const auto ClassPath = FSoftClassPath(CLS);
		JSON->SetStringField(TEXT("ClassPath"),ClassPath.ToString());
	} else {
		JSON->SetStringField(TEXT("ClassPath"),TEXT("nullptr"));
	}
	//
	return JSON;
}

TArray<TSharedPtr<FJsonValue>>Reflector::FPropertyToJSON(FArrayProperty* Property, const UObject* Container) {
	auto ArrayPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptArrayHelper Helper(Property,ArrayPtr);
	TArray<TSharedPtr<FJsonValue>>JArray;
	//
	if (Property->Inner->IsA<FObjectProperty>()) {
		const auto OBJs = *Property->ContainerPtrToValuePtr<TArray<UObject*>>(Container);
		//
		for (auto OBJ : OBJs) {
			if (OBJ) {
				FString FullName;
				if (OBJ->IsA(AActor::StaticClass())) {
					FullName = Reflector::MakeActorID(CastChecked<AActor>(OBJ),true).ToString();
				} else if (OBJ->IsA(UActorComponent::StaticClass())) {
					FullName = Reflector::MakeComponentID(CastChecked<UActorComponent>(OBJ),true).ToString();
				} else {FullName=Reflector::MakeObjectID(OBJ).ToString();}
				//
				const auto ClassPath = OBJ->GetClass()->GetDefaultObject()->GetPathName();
				TSharedPtr<FJsonValue>JValue = MakeShareable(new FJsonValueString(ClassPath+TEXT("::")+FullName));
				JArray.Push(JValue);
			} else {
				TSharedPtr<FJsonValue>JValue = MakeShareable(new FJsonValueString(TEXT("nullptr::nullptr")));
				JArray.Push(JValue);
			}
		}
	} else {
		for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(Property->Inner,Helper.GetRawPtr(I),0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {JArray.Push(Value);}
		}
	}
	//
	return JArray;
}

TArray<TSharedPtr<FJsonValue>>Reflector::FPropertyToJSON(FSetProperty* Property, const UObject* Container) {
	auto SetPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptSetHelper Helper(Property,SetPtr);
	TArray<TSharedPtr<FJsonValue>>JArray;
	//
	if (Property->ElementProp->IsA<FObjectProperty>()) {
		const auto OBJs = *Property->ContainerPtrToValuePtr<TSet<UObject*>>(Container);
		for (auto OBJ : OBJs) {
			if (OBJ) {
				FString FullName;
				if (OBJ->IsA(AActor::StaticClass())) {
					FullName = Reflector::MakeActorID(CastChecked<AActor>(OBJ),true).ToString();
				} else if (OBJ->IsA(UActorComponent::StaticClass())) {
					FullName = Reflector::MakeComponentID(CastChecked<UActorComponent>(OBJ),true).ToString();
				} else {FullName=Reflector::MakeObjectID(OBJ).ToString();}
				//
				const auto ClassPath = OBJ->GetClass()->GetDefaultObject()->GetPathName();
				TSharedPtr<FJsonValue>JValue = MakeShareable(new FJsonValueString(ClassPath+TEXT("::")+FullName));
				JArray.Push(JValue);
			} else {
				TSharedPtr<FJsonValue>JValue = MakeShareable(new FJsonValueString(TEXT("nullptr::nullptr")));
				JArray.Push(JValue);
			}
		}
	} else {
		for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(Property->ElementProp,Helper.GetElementPtr(I),0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {JArray.Push(Value);}
		}
	}
	//
	return JArray;
}

TSharedPtr<FJsonObject>Reflector::FPropertyToJSON(FMapProperty* Property, const UObject* Container) {
	auto MapPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptMapHelper Helper(Property,MapPtr);
	//
	TArray<TSharedPtr<FJsonValue>>Keys;
	TArray<TSharedPtr<FJsonValue>>Values;
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	//
	if (Property->KeyProp->IsA<FObjectProperty>()) {
		const auto OBJs = *Property->ContainerPtrToValuePtr<TMap<UObject*,void*>>(Container);
		for (auto OBJ : OBJs) {
			if ((OBJ.Key)&&OBJ.Key->IsValidLowLevelFast()) {
				FString FullName;
				//
				if (OBJ.Key->IsA(AActor::StaticClass())) {
					FullName = Reflector::MakeActorID(CastChecked<AActor>(OBJ.Key),true).ToString();
				} else if (OBJ.Key->IsA(UActorComponent::StaticClass())) {
					FullName = Reflector::MakeComponentID(CastChecked<UActorComponent>(OBJ.Key),true).ToString();
				} else {FullName=Reflector::MakeObjectID(OBJ.Key).ToString();}
				//
				const auto ClassPath = OBJ.Key->GetClass()->GetDefaultObject()->GetPathName();
				TSharedPtr<FJsonValue>JKey = MakeShareable(new FJsonValueString(ClassPath+TEXT("::")+FullName));
				Keys.Push(JKey);
			} else {
				TSharedPtr<FJsonValue>JKey = MakeShareable(new FJsonValueString(TEXT("nullptr::nullptr")));
				Keys.Push(JKey);
			}
		}
	} else {
		for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
			auto Key = FJsonObjectConverter::UPropertyToJsonValue(Property->KeyProp,Helper.GetKeyPtr(I),0,CPF_Transient);
			if (Key.IsValid()&&(!Key->IsNull())) {Keys.Push(Key);}
		}
	}
	//
	if ((Property->KeyProp->IsA<FNameProperty>())&&(Property->ValueProp->IsA<FObjectProperty>())) {
		const auto OBJs = *Property->ContainerPtrToValuePtr<TMap<FName,UObject*>>(Container);
		for (auto OBJ : OBJs) {
			if ((OBJ.Value)&&OBJ.Value->IsValidLowLevelFast()) {
				FString FullName;
				//
				if (OBJ.Value->IsA(AActor::StaticClass())) {
					FullName = Reflector::MakeActorID(CastChecked<AActor>(OBJ.Value),true).ToString();
				} else if (OBJ.Value->IsA(UActorComponent::StaticClass())) {
					FullName = Reflector::MakeComponentID(CastChecked<UActorComponent>(OBJ.Value),true).ToString();
				} else {FullName=Reflector::MakeObjectID(OBJ.Value).ToString();}
				//
				const auto ClassPath = OBJ.Value->GetClass()->GetDefaultObject()->GetPathName();
				TSharedPtr<FJsonValue>JValue = MakeShareable(new FJsonValueString(ClassPath+TEXT("::")+FullName));
				Values.Push(JValue);
			} else {
				TSharedPtr<FJsonValue>JValue = MakeShareable(new FJsonValueString(TEXT("nullptr::nullptr")));
				Values.Push(JValue);
			}
		}
	} else if ((Property->KeyProp->IsA<FObjectProperty>())&&(Property->ValueProp->IsA<FObjectProperty>())) {
		const auto OBJs = *Property->ContainerPtrToValuePtr<TMap<UObject*,UObject*>>(Container);
		for (auto OBJ : OBJs) {
			if ((OBJ.Value)&&OBJ.Value->IsValidLowLevelFast()) {
				FString FullName;
				//
				if (OBJ.Value->IsA(AActor::StaticClass())) {
					FullName = Reflector::MakeActorID(CastChecked<AActor>(OBJ.Value),true).ToString();
				} else if (OBJ.Value->IsA(UActorComponent::StaticClass())) {
					FullName = Reflector::MakeComponentID(CastChecked<UActorComponent>(OBJ.Value),true).ToString();
				} else {FullName=Reflector::MakeObjectID(OBJ.Value).ToString();}
				//
				const auto ClassPath = OBJ.Value->GetClass()->GetDefaultObject()->GetPathName();
				TSharedPtr<FJsonValue>JValue = MakeShareable(new FJsonValueString(ClassPath+TEXT("::")+FullName));
				Values.Push(JValue);
			}
		}
	} else if (Property->ValueProp->IsA<FObjectProperty>()) {
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Property Map Value Type <-> [Data Map Value Type] mismatch:  %s"),*Property->GetName()));
		LOG_SV(true,ESeverity::Warning,TEXT("Only  TMap<UObject*,UObject*>  OR  TMap<FName,UObject*>  are supported for Maps of Object References!"));
	} else {
		for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(Property->ValueProp,Helper.GetValuePtr(I),0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {Values.Push(Value);}
		}
	}
	//
	if ((Keys.Num()>0)&&(Values.Num()>0)) {
		JSON->SetArrayField(Property->GetName()+TEXT("_KEY"),Keys);
		JSON->SetArrayField(Property->GetName()+TEXT("_VALUE"),Values);
	}
	//
	return JSON;
}

TSharedPtr<FJsonObject>Reflector::FPropertyToJSON(FStructProperty* Property, const UObject* Container, const EStructType StructType) {
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	//
	switch (StructType) {
		case EStructType::LColor:
		{
			auto StructPtr = Property->ContainerPtrToValuePtr<FLinearColor>(Container);
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {return Value->AsObject();}
		}	break;
		//
		case EStructType::FColor:
		{
			auto StructPtr = Property->ContainerPtrToValuePtr<FColor>(Container);
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {return Value->AsObject();}
		}	break;
		//
		case EStructType::Vector2D:
		{
			auto StructPtr = Property->ContainerPtrToValuePtr<FVector2D>(Container);
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {return Value->AsObject();}
		}	break;
		//
		case EStructType::Vector3D:
		{
			auto StructPtr = Property->ContainerPtrToValuePtr<FVector>(Container);
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {return Value->AsObject();}
		}	break;
		//
		case EStructType::Rotator:
		{
			auto StructPtr = Property->ContainerPtrToValuePtr<FRotator>(Container);
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {return Value->AsObject();}
		}	break;
		//
		case EStructType::Transform:
		{
			auto StructPtr = Property->ContainerPtrToValuePtr<FTransform>(Container);
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,CPF_Transient);
			if (Value.IsValid()&&(!Value->IsNull())) {return Value->AsObject();}
		}	break;
		//
		case EStructType::Struct:
		{
			if (Property->Struct==FRuntimeFloatCurve::StaticStruct()) {
				auto StructPtr = Property->ContainerPtrToValuePtr<FRuntimeFloatCurve>(Container);
				for (TFieldIterator<FProperty>IT(Property->Struct); IT; ++IT) {
					FProperty* Field = *IT;
					for (int32 I=0; I<Field->ArrayDim; ++I) {
						const auto ValuePtr = Field->ContainerPtrToValuePtr<FRichCurve>(StructPtr,I);
						if (FStructProperty* CurveData = CastField<FStructProperty>(Field)) {ParseFStructToJSON(JSON,CurveData,ValuePtr,Container);}
					}
				} break;
			} else {
				auto StructPtr = Property->ContainerPtrToValuePtr<void>(Container);
				ParseFStructToJSON(JSON,Property,StructPtr,Container);
			}
		}	break;
	default: break;}
	//
	return JSON;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Property Parser:

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FBoolProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const bool Value = JSON->GetBoolField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FIntProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const int32 Value = JSON->GetIntegerField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FInt64Property* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const int64 Value = (int64)JSON->GetNumberField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FByteProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const uint8 Value = JSON->GetIntegerField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FEnumProperty* Property, UObject* Container) {
	const UEnum* Enum = Property->GetEnum();
	const auto Value = Enum->GetValueByName(*JSON->GetStringField(PID));
	//
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	Property->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FFloatProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const float Value = JSON->GetNumberField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FDoubleProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const double Value = JSON->GetNumberField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FNameProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const FName Value = *JSON->GetStringField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FTextProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const FText Value = FText::FromString(JSON->GetStringField(PID));
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FStrProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const FString Value = JSON->GetStringField(PID);
	//
	Property->SetPropertyValue(ValuePtr,Value);
}

void Reflector::JSONToFProperty(USavior* Savior, TSharedPtr<FJsonObject>&JSON, const FString &PID, FObjectProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const auto JOBJ = JSON->GetObjectField(PID);
	//
	const FString Class = JOBJ->GetStringField(TEXT("ClassPath"));
	const FString Instance = JOBJ->GetStringField(TEXT("FullName"));
	//
	if (Class.IsEmpty()||Class.Equals(TEXT("nullptr"))) {return;}
	//
	const FSoftObjectPath ClassPath(Class);
	TSoftObjectPtr<UObject>ClassPtr(ClassPath);
	//
	auto CDO = ClassPtr.Get();
	if ((CDO)&&CDO->IsA(AActor::StaticClass())) {
		for (TObjectIterator<AActor>Actor; Actor; ++Actor) {
			if (!(*Actor)->GetOutermost()->ContainsMap()) {continue;}
			if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
			//
			const FString FullName = Reflector::MakeActorID(*Actor,true).ToString();
			if (FullName==Instance) {Property->SetPropertyValue(ValuePtr,*Actor); break;}
		}
	} else if ((CDO)&&CDO->IsA(UActorComponent::StaticClass())) {
		for (TObjectIterator<UActorComponent>CMP; CMP; ++CMP) {
			if (!(*CMP)->GetOutermost()->ContainsMap()) {continue;}
			if ((*CMP)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
			//
			const FString FullName = Reflector::MakeComponentID(*CMP,true).ToString();
			if (FullName==Instance) {Property->SetPropertyValue(ValuePtr,*CMP); break;}
		}
	} else if (IsInGameThread()&&(CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
		GT_NewObjectInstance(Savior,Property,Container,CDO,Instance);
	} else if ((CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&GT_NewObjectInstance,Savior,Property,Container,CDO,Instance),
			GET_STATID(STAT_FSimpleDelegateGraphTask_JSONToFProperty),
			nullptr, ENamedThreads::GameThread
		);
	}
}

void Reflector::JSONToFProperty(USavior* Savior, TSharedPtr<FJsonObject>&JSON, const FString &PID, FClassProperty* Property, UObject* Container) {
	auto ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	const auto JCLS = JSON->GetObjectField(PID);
	//
	const FString Class = JCLS->GetStringField(TEXT("ClassPath"));
	if (Class.IsEmpty()||Class.Equals(TEXT("nullptr"))) {return;}
	//
	const FSoftClassPath ClassPath(Class);
	if (ClassPath.IsValid()) {Property->SetPropertyValue(ValuePtr,ClassPath.ResolveClass());}
}

void Reflector::JSONToFProperty(USavior* Savior, TSharedPtr<FJsonObject>&JSON, const FString &PID, FArrayProperty* Property, UObject* Container) {
	const auto ArrayPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptArrayHelper Helper(Property,ArrayPtr);
	//
	const auto JArray = JSON->GetArrayField(PID);
	Helper.Resize(JArray.Num());
	//
	if (Property->Inner->IsA<FObjectProperty>()) {
		TArray<UObject*>&_REF = *Property->ContainerPtrToValuePtr<TArray<UObject*>>(Container);
		//
		for (int32 I=0, N=JArray.Num()-1; I<=N; I++) {
			const auto &JValue = JArray[I];
			if (JValue.IsValid()&&(!JValue->AsString().IsEmpty())) {
				FString Class, Instance;
				//
				JValue->AsString().Split(TEXT("::"),&Class,&Instance);
				if (Class.IsEmpty()||Class.Equals(TEXT("nullptr"))) {continue;}
				//
				const FSoftObjectPath ClassPath(Class);
				TSoftObjectPtr<UObject>ClassPtr(ClassPath);
				//
				auto CDO = ClassPtr.Get();
				if ((CDO)&&CDO->IsA(AActor::StaticClass())) {
					for (TObjectIterator<AActor>Actor; Actor; ++Actor) {
						if (!(*Actor)->GetOutermost()->ContainsMap()) {continue;}
						if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
						//
						const FString FullName = Reflector::MakeActorID(*Actor,true).ToString();
						if (FullName==Instance) {_REF[I]=(*Actor); break;}
					}
				} else if ((CDO)&&CDO->IsA(UActorComponent::StaticClass())) {
					for (TObjectIterator<UActorComponent>CMP; CMP; ++CMP) {
						if (!(*CMP)->GetOutermost()->ContainsMap()) {continue;}
						if ((*CMP)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
						//
						const FString FullName = Reflector::MakeComponentID(*CMP,true).ToString();
						if (FullName==Instance) {_REF[I]=(*CMP); break;}
					}
				} else if (IsInGameThread()&&(CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
					GT_NewObjectIntoArray(Savior,Property,Container,CDO,Instance,I);
				} else if ((CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
						FSimpleDelegateGraphTask::FDelegate::CreateStatic(&GT_NewObjectIntoArray,Savior,Property,Container,CDO,Instance,I),
						GET_STATID(STAT_FSimpleDelegateGraphTask_JSONArrayToFProperty),
						nullptr, ENamedThreads::GameThread
					);
				}
			}
		}
	} else {
		for (int32 I=0, N=JArray.Num()-1; I<=N; I++) {
			const auto &JValue = JArray[I];
			//
			if (JValue.IsValid() && (!JValue->IsNull())) {
				if (!FJsonObjectConverter::JsonValueToUProperty(JValue,Property->Inner,Helper.GetRawPtr(I),0,CPF_Transient)) {
					LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Array Element Type <-> Property Array Element Type] mismatch:  %s"),*PID));
				}
			}
		}
	}
}

void Reflector::JSONToFProperty(USavior* Savior, TSharedPtr<FJsonObject>&JSON, const FString &PID, FSetProperty* Property, UObject* Container) {
	const auto SetPtr = Property->ContainerPtrToValuePtr<void>(Container);
	FScriptSetHelper Helper(Property,SetPtr);
	//
	const auto JSet = JSON->GetArrayField(PID);
	Helper.EmptyElements();
	//
	if (Property->ElementProp->IsA<FObjectProperty>()) {
		TSet<UObject*>&_REF = *Property->ContainerPtrToValuePtr<TSet<UObject*>>(Container);
		for (int32 I=0, N=JSet.Num()-1; I<=N; I++) {
			const auto &JValue = JSet[I];
			//
			if (JValue.IsValid()&&(!JValue->AsString().IsEmpty())) {
				FString Class, Instance;
				//
				JValue->AsString().Split(TEXT("::"),&Class,&Instance);
				if (Class.IsEmpty()||Class.Equals(TEXT("nullptr"))) {continue;}
				//
				const FSoftObjectPath ClassPath(Class);
				TSoftObjectPtr<UObject>ClassPtr(ClassPath);
				//
				auto CDO = ClassPtr.Get();
				if ((CDO)&&CDO->IsA(AActor::StaticClass())) {
					for (TObjectIterator<AActor>Actor; Actor; ++Actor) {
						if (!(*Actor)->GetOutermost()->ContainsMap()) {continue;}
						if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
						//
						const FString FullName = Reflector::MakeActorID(*Actor,true).ToString();
						if (FullName==Instance) {_REF.Add(*Actor); break;}
					}
				} else if ((CDO)&&CDO->IsA(UActorComponent::StaticClass())) {
					for (TObjectIterator<UActorComponent>CMP; CMP; ++CMP) {
						if (!(*CMP)->GetOutermost()->ContainsMap()) {continue;}
						if ((*CMP)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
						//
						const FString FullName = Reflector::MakeComponentID(*CMP,true).ToString();
						if (FullName==Instance) {_REF.Add(*CMP); break;}
					}
				} else if (IsInGameThread()&&(CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
					GT_NewObjectIntoSet(Savior,Property,Container,CDO,Instance);
				} else if ((CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
						FSimpleDelegateGraphTask::FDelegate::CreateStatic(&GT_NewObjectIntoSet,Savior,Property,Container,CDO,Instance),
						GET_STATID(STAT_FSimpleDelegateGraphTask_JSONSetToFProperty),
						nullptr, ENamedThreads::GameThread
					);
				}
			}
		}
	} else {
		for (int32 I=0, N=JSet.Num()-1; I<=N; I++) {
			const auto &JValue = JSet[I];
			//
			auto Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
			if (JValue.IsValid()&&(!JValue->IsNull())) {
				if (!FJsonObjectConverter::JsonValueToUProperty(JValue,Property->ElementProp,Helper.GetElementPtr(Index),0,CPF_Transient)) {
					LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Set Element Type <-> Property Set Element Type] mismatch:  %s"),*PID));
				}
			}
		}
	}	Helper.Rehash();
}

void Reflector::JSONToFProperty(USavior* Savior, TSharedPtr<FJsonObject>&JSON, const FString &PID, FMapProperty* Property, UObject* Container) {
	auto MapPtr = Property->ContainerPtrToValuePtr<void>(Container);
	const auto JMap = JSON->GetObjectField(PID);
	FScriptMapHelper Helper(Property,MapPtr);
	//
	//
	TArray<TSharedPtr<FJsonValue>>JKeys;
	if (JMap->HasField(PID+TEXT("_KEY"))) {
		JKeys = JMap->GetArrayField(PID+TEXT("_KEY"));
		Helper.EmptyValues();
		//
		if (Property->KeyProp->IsA<FObjectProperty>()) {
			TMap<UObject*,void*>&_REF = *Property->ContainerPtrToValuePtr<TMap<UObject*,void*>>(Container);
			for (int32 I=0, N=JKeys.Num()-1; I<=N; I++) {
				const auto &JKey = JKeys[I];
				//
				if (JKey.IsValid()&&(!JKey->AsString().IsEmpty())) {
					FString Class, Instance;
					//
					JKey->AsString().Split(TEXT("::"),&Class,&Instance);
					if (Class.IsEmpty()||Class.Equals(TEXT("nullptr"))) {continue;}
					//
					const FSoftObjectPath ClassPath(Class);
					TSoftObjectPtr<UObject>ClassPtr(ClassPath);
					//
					auto CDO = ClassPtr.Get();
					if ((CDO)&&CDO->IsA(AActor::StaticClass())) {
						for (TObjectIterator<AActor>Actor; Actor; ++Actor) {
							if (!(*Actor)->GetOutermost()->ContainsMap()) {continue;}
							if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
							//
							const FString FullName = Reflector::MakeActorID(*Actor,true).ToString();
							if (FullName==Instance) {_REF.Add(*Actor); break;}
						}
					} else if ((CDO)&&CDO->IsA(UActorComponent::StaticClass())) {
						for (TObjectIterator<UActorComponent>CMP; CMP; ++CMP) {
							if (!(*CMP)->GetOutermost()->ContainsMap()) {continue;}
							if ((*CMP)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
							//
							const FString FullName = Reflector::MakeComponentID(*CMP,true).ToString();
							if (FullName==Instance) {_REF.Add(*CMP); break;}
						}
					} else if (IsInGameThread()&&(CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
						GT_NewObjectIntoMapKey(Savior,Property,Container,CDO,Instance);
					} else if ((CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
						FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
							FSimpleDelegateGraphTask::FDelegate::CreateStatic(&GT_NewObjectIntoMapKey,Savior,Property,Container,CDO,Instance),
							GET_STATID(STAT_FSimpleDelegateGraphTask_JSONMapKeyToFProperty),
							nullptr, ENamedThreads::GameThread
						);
					}
				}
			}
		} else {
			for (int32 I=0, N=JKeys.Num()-1; I<=N; I++) {
				auto IKey = Helper.AddDefaultValue_Invalid_NeedsRehash();
				const auto &JKey = JKeys[I];
				//
				if (JKey.IsValid()&&(!JKey->IsNull())) {
					if (!FJsonObjectConverter::JsonValueToUProperty(JKey,Property->KeyProp,Helper.GetKeyPtr(IKey),0,CPF_Transient)) {
						LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Map Key Type <-> Property Map Key Type] mismatch:  %s"),*PID));
					}
				}
			}
	} Helper.Rehash();}
	//
	if (JMap->HasField(PID+TEXT("_VALUE"))) {
		const auto JValues = JMap->GetArrayField(PID+TEXT("_VALUE"));
		if ((Property->KeyProp->IsA<FObjectProperty>())&&(Property->ValueProp->IsA<FObjectProperty>())) {
			TMap<UObject*,UObject*>&_REF = *Property->ContainerPtrToValuePtr<TMap<UObject*,UObject*>>(Container);
			//
			for (int32 I=0, N=JKeys.Num()-1; I<=N; I++) {
				if (!JValues.IsValidIndex(I)) {continue;}
				const auto &JValue = JValues[I];
				const auto &JKey = JKeys[I];
				//
				if (JValue.IsValid()&&(!JValue->AsString().IsEmpty())) {
					FString Key, Class, Instance;
					JKey->AsString().Split(TEXT("::"),nullptr,&Key);
					JValue->AsString().Split(TEXT("::"),&Class,&Instance);
					if (Class.IsEmpty()||Class.Equals(TEXT("nullptr"))) {continue;}
					//
					const FSoftObjectPath ClassPath(Class);
					TSoftObjectPtr<UObject>ClassPtr(ClassPath);
					//
					TArray<UObject*>Keys;
					UObject* KeyOBJ = nullptr;
					_REF.GenerateKeyArray(Keys);
					//
					for (auto OBJ : Keys) {
						const FString FullName = Reflector::MakeObjectID(OBJ).ToString();
						if (FullName==Key) {KeyOBJ=OBJ; break;}
					} if (KeyOBJ==nullptr) {continue;}
					//
					auto CDO = ClassPtr.Get();
					if ((CDO)&&CDO->IsA(AActor::StaticClass())) {
						for (TObjectIterator<AActor>Actor; Actor; ++Actor) {
							if (!(*Actor)->GetOutermost()->ContainsMap()) {continue;}
							if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
							//
							const FString FullName = Reflector::MakeActorID(*Actor,true).ToString();
							if (FullName==Instance) {_REF.Emplace(*Actor); break;}
						}
					} else if ((CDO)&&CDO->IsA(UActorComponent::StaticClass())) {
						for (TObjectIterator<UActorComponent>CMP; CMP; ++CMP) {
							if (!(*CMP)->GetOutermost()->ContainsMap()) {continue;}
							if ((*CMP)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
							//
							const FString FullName = Reflector::MakeComponentID(*CMP,true).ToString();
							if (FullName==Instance) {_REF.Emplace(*CMP); break;}
						}
					} else if (IsInGameThread()&&(CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
						GT_NewObjectIntoMapValue(Savior,Property,Container,KeyOBJ,CDO,Instance);
					} else if ((CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
						FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
							FSimpleDelegateGraphTask::FDelegate::CreateStatic(&GT_NewObjectIntoMapValue,Savior,Property,Container,KeyOBJ,CDO,Instance),
							GET_STATID(STAT_FSimpleDelegateGraphTask_JSONMapValueToFProperty),
							nullptr, ENamedThreads::GameThread
						);
					}
				}
			}
		} else if ((Property->KeyProp->IsA<FNameProperty>())&&(Property->ValueProp->IsA<FObjectProperty>())) {
			TMap<FName,UObject*>&_REF = *Property->ContainerPtrToValuePtr<TMap<FName,UObject*>>(Container);
			for (int32 I=0, N=JKeys.Num()-1; I<=N; I++) {
				if (!JValues.IsValidIndex(I)) {continue;}
				const auto &JValue = JValues[I];
				const auto &JKey = JKeys[I];
				//
				if (JValue.IsValid()&&(!JValue->AsString().IsEmpty())) {
					FString Class, Instance;
					FName Key = *JKey->AsString();
					JValue->AsString().Split(TEXT("::"),&Class,&Instance);
					if (Class.IsEmpty()||Class.Equals(TEXT("nullptr"))) {continue;}
					//
					const FSoftObjectPath ClassPath(Class);
					TSoftObjectPtr<UObject>ClassPtr(ClassPath);
					//
					auto CDO = ClassPtr.Get();
					if ((CDO)&&CDO->IsA(AActor::StaticClass())) {
						for (TObjectIterator<AActor>Actor; Actor; ++Actor) {
							if (!(*Actor)->GetOutermost()->ContainsMap()) {continue;}
							if ((*Actor)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
							//
							const FString FullName = Reflector::MakeActorID(*Actor,true).ToString();
							if (FullName==Instance) {_REF.Emplace(Key,*Actor); break;}
						}
					} else if ((CDO)&&CDO->IsA(UActorComponent::StaticClass())) {
						for (TObjectIterator<UActorComponent>CMP; CMP; ++CMP) {
							if (!(*CMP)->GetOutermost()->ContainsMap()) {continue;}
							if ((*CMP)->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed)) {continue;}
							//
							const FString FullName = Reflector::MakeComponentID(*CMP,true).ToString();
							if (FullName==Instance) {_REF.Emplace(Key,*CMP); break;}
						}
					} else if (IsInGameThread()&&(CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
						GT_NewObjectIntoNamedMapValue(Savior,Property,Container,Key,CDO,Instance);
					} else if ((CDO)&&(Reflector::IsSafeToRespawnInstance(CDO))) {
						FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
							FSimpleDelegateGraphTask::FDelegate::CreateStatic(&GT_NewObjectIntoNamedMapValue,Savior,Property,Container,Key,CDO,Instance),
							GET_STATID(STAT_FSimpleDelegateGraphTask_JSONMapValueToFProperty),
							nullptr, ENamedThreads::GameThread
						);
					}
				}
			}
		} else if (Property->ValueProp->IsA<FObjectProperty>()) {
			LOG_SV(true,ESeverity::Warning,TEXT("Only  TMap<UObject*,UObject*>  OR  TMap<FName,UObject*>  are supported for Maps of Object References!"));
		} else {
			for (int32 I=0, N=JKeys.Num()-1; I<=N; I++) {
				if (!JValues.IsValidIndex(I)) {continue;}
				const auto &JValue = JValues[I];
				//
				if (JValue.IsValid()&&(!JValue->IsNull())) {
					if (!FJsonObjectConverter::JsonValueToUProperty(JValue,Property->ValueProp,Helper.GetValuePtr(I),0,CPF_Transient)) {
						LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Map Value Type <-> Property Map Value Type] mismatch:  %s"),*PID));
					}
				}
			}
		}
	}
}

void Reflector::JSONToFProperty(TSharedPtr<FJsonObject>&JSON, const FString &PID, FStructProperty* Property, UObject* Container, const EStructType StructType) {
	switch (StructType) {
		case EStructType::FColor:
		case EStructType::LColor:
		{
			if (Property->Struct==TBaseStructure<FColor>::Get()) {
				const auto JColor = JSON->GetObjectField(PID);
				FColor Color; if (JColor.IsValid()) {
					Color.R = JColor->GetIntegerField(TEXT("r")); Color.G = JColor->GetIntegerField(TEXT("g"));
					Color.B = JColor->GetIntegerField(TEXT("b")); Color.A = JColor->GetIntegerField(TEXT("a"));
				}
				auto StructPtr = Property->ContainerPtrToValuePtr<FColor>(Container);
				if (StructPtr) {(*StructPtr)=Color;}
			} else if (Property->Struct==TBaseStructure<FLinearColor>::Get()) {
				const auto JColor = JSON->GetObjectField(PID);
				FLinearColor Color; if (JColor.IsValid()) {
					Color.R = JColor->GetNumberField(TEXT("r")); Color.G = JColor->GetNumberField(TEXT("g"));
					Color.B = JColor->GetNumberField(TEXT("b")); Color.A = JColor->GetNumberField(TEXT("a"));
				}
				auto StructPtr = Property->ContainerPtrToValuePtr<FLinearColor>(Container);
				if (StructPtr) {(*StructPtr)=Color;}
			} else {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Type <-> Property Type] mismatch:  %s"),*PID));}
		}	break;
		//
		case EStructType::Vector2D:
		{
			if (Property->Struct!=TBaseStructure<FVector2D>::Get()) {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Type <-> Property Type] mismatch:  %s"),*PID)); break;}
			const auto JVector = JSON->GetObjectField(PID);
			FVector2D Vector; if (JVector.IsValid()) {
				Vector.X = JVector->GetNumberField(TEXT("x"));
				Vector.Y = JVector->GetNumberField(TEXT("y"));
			}
			auto StructPtr = Property->ContainerPtrToValuePtr<FVector2D>(Container);
			if (StructPtr) {(*StructPtr)=Vector;}
		}	break;
		//
		case EStructType::Vector3D:
		{
			if (Property->Struct!=TBaseStructure<FVector>::Get()) {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Type <-> Property Type] mismatch:  %s"),*PID)); break;}
			const auto JVector = JSON->GetObjectField(PID);
			//
			FVector Vector; if (JVector.IsValid()) {
				Vector.X = JVector->GetNumberField(TEXT("x"));
				Vector.Y = JVector->GetNumberField(TEXT("y"));
				Vector.Z = JVector->GetNumberField(TEXT("z"));
			}
			//
			auto StructPtr = Property->ContainerPtrToValuePtr<FVector>(Container);
			if (StructPtr) {(*StructPtr)=Vector;}
		}	break;
		//
		case EStructType::Rotator:
		{
			if (Property->Struct!=TBaseStructure<FRotator>::Get()) {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Type <-> Property Type] mismatch:  %s"),*PID)); break;}
			const auto JRotator = JSON->GetObjectField(PID);
			//
			FRotator Rotator; if (JRotator.IsValid()) {
				Rotator.Pitch = JRotator->GetNumberField(TEXT("pitch"));
				Rotator.Roll = JRotator->GetNumberField(TEXT("roll"));
				Rotator.Yaw = JRotator->GetNumberField(TEXT("yaw"));
			}
			//
			auto StructPtr = Property->ContainerPtrToValuePtr<FRotator>(Container);
			if (StructPtr) {(*StructPtr)=Rotator;}
		}	break;
		//
		case EStructType::Curve:
		{
			if (Property->Struct!=FRuntimeFloatCurve::StaticStruct()) {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Type <-> Property Type] mismatch:  %s"),*PID)); break;}
			FRuntimeFloatCurve FloatCurve; FRichCurve CurveData;
			//
			const auto JCurve = JSON->GetObjectField(PID);
			FJsonObjectConverter::JsonObjectToUStruct<FRichCurve>(JCurve.ToSharedRef(),&CurveData);
			auto StructPtr = Property->ContainerPtrToValuePtr<FRuntimeFloatCurve>(Container);
			//
			if (StructPtr) {
				FloatCurve.EditorCurveData = CurveData;
				FloatCurve.ExternalCurve = nullptr;
				(*StructPtr)=FloatCurve;
			}
		}	break;
		//
		case EStructType::Struct:
		{
			auto JStruct = JSON->GetObjectField(PID);
			void* StructPtr = Property->ContainerPtrToValuePtr<void>(Container);
			//
			if (StructPtr) {
				ParseJSONtoFStruct(JStruct,Property,StructPtr,Container);
			} else {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Type <-> Property Type] mismatch:  %s"),*PID));}
		}	break;
	default: break;}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Struct Parser:

TSharedPtr<FJsonObject> Reflector::ParseFStructToJSON(FStructProperty* Property, const void* StructPtr, const UObject* Container) {
	TSharedPtr<FJsonObject>JSON = MakeShareable(new FJsonObject);
	UScriptStruct* Struct = Property->Struct;
	//
	for (TFieldIterator<FProperty>IT(Struct); IT; ++IT) {
		FProperty* Field = *IT; 
		for (int32 I=0; I<Field->ArrayDim; ++I) {
			auto ValuePtr = Field->ContainerPtrToValuePtr<void>(StructPtr,I);
			ParseFStructPropertyToJSON(JSON,Field,ValuePtr,Container,0,CPF_Transient,Struct->IsNative());
		}
	}
	//
	return JSON;
}

void Reflector::ParseFStructToJSON(TSharedPtr<FJsonObject>&JSON, FStructProperty* Property, const void* StructPtr, const UObject* Container) {
	UScriptStruct* Struct = Property->Struct;
	for (TFieldIterator<FProperty>IT(Struct); IT; ++IT) {
		FProperty* Field = *IT; 
		for (int32 I=0; I<Field->ArrayDim; ++I) {
			auto ValuePtr = Field->ContainerPtrToValuePtr<void>(StructPtr,I);
			ParseFStructPropertyToJSON(JSON,Field,ValuePtr,Container,0,CPF_Transient,Struct->IsNative());
		}
	}
}

void Reflector::ParseJSONtoFStruct(TSharedPtr<FJsonObject>&JSON, FStructProperty* Property, void* StructPtr, UObject* Container) {
	UScriptStruct* Struct = Property->Struct;
	//
	for (TFieldIterator<FProperty>IT(Struct); IT; ++IT) {
		FProperty* Field = *IT; 
		for (int32 I=0; I<Field->ArrayDim; ++I) {
			void* ValuePtr = Field->ContainerPtrToValuePtr<void>(StructPtr,I);
			if (ValuePtr) {ParseJSONtoFStructProperty(JSON,Field,ValuePtr,Container,0,CPF_Transient,Struct->IsNative());}
		}
	}
}

void Reflector::ParseJSONtoFStructProperty(TSharedPtr<FJsonObject>&JSON, FProperty* Property, void* ValuePtr, UObject* Container, int64 CheckFlags, int64 SkipFlags, const bool IsNative) {
	if (FBoolProperty* _Bool = CastField<FBoolProperty>(Property)) {
		const FString Field = (IsNative) ? _Bool->GetName() : SanitizePropertyID(_Bool->GetName());
		if (JSON->HasField(Field)) {
			_Bool->SetPropertyValue(ValuePtr,JSON->GetBoolField(Field));
		}
	}
	//
	if (FEnumProperty* _Enum = CastField<FEnumProperty>(Property)) {
		UEnum* EnumPtr = _Enum->GetEnum();
		const FString Field = (IsNative) ? _Enum->GetName() : SanitizePropertyID(_Enum->GetName());
		//
		if ((EnumPtr)&&JSON->HasField(Field)) {
			const auto Value = EnumPtr->GetValueByName(*JSON->GetStringField(Field));
			_Enum->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr,Value);
		}
	}
	//
	if (FByteProperty* _Byte = CastField<FByteProperty>(Property)) {
		const FString Field = (IsNative) ? _Byte->GetName() : SanitizePropertyID(_Byte->GetName());
		if (JSON->HasField(Field)) {
			_Byte->SetIntPropertyValue(ValuePtr,(uint64)JSON->GetIntegerField(Field));
		}
	}
	//
	if (FIntProperty* _Int = CastField<FIntProperty>(Property)) {
		const FString Field = (IsNative) ? _Int->GetName() : SanitizePropertyID(_Int->GetName());
		if (JSON->HasField(Field)) {
			_Int->SetIntPropertyValue(ValuePtr,(int64)JSON->GetIntegerField(Field));
		}
	}
	//
	if (FFloatProperty* _Float = CastField<FFloatProperty>(Property)) {
		const FString Field = (IsNative) ? _Float->GetName() : SanitizePropertyID(_Float->GetName());
		if (JSON->HasField(Field)) {
			_Float->SetFloatingPointPropertyValue(ValuePtr,JSON->GetNumberField(Field));
		}
	}
	//
	if (FNameProperty* _Name = CastField<FNameProperty>(Property)) {
		const FString Field = (IsNative) ? _Name->GetName() : SanitizePropertyID(_Name->GetName());
		if (JSON->HasField(Field)) {
			_Name->SetPropertyValue(ValuePtr,*JSON->GetStringField(Field));
		}
	}
	//
	if (FStrProperty* _String = CastField<FStrProperty>(Property)) {
		const FString Field = (IsNative) ? _String->GetName() : SanitizePropertyID(_String->GetName());
		if (JSON->HasField(Field)) {
			_String->SetPropertyValue(ValuePtr,JSON->GetStringField(Field));
		}
	}
	//
	if (FTextProperty* _Text = CastField<FTextProperty>(Property)) {
		const FString Field = (IsNative) ? _Text->GetName() : SanitizePropertyID(_Text->GetName());
		if (JSON->HasField(Field)) {
			_Text->SetPropertyValue(ValuePtr,FText::FromString(JSON->GetStringField(Field)));
		}
	}
	//
	if (FArrayProperty* _Array = CastField<FArrayProperty>(Property)) {
		const FString Field = (IsNative) ? _Array->GetName() : SanitizePropertyID(_Array->GetName());
		if (JSON->HasField(Field)) {
			if (_Array->Inner->IsA<FObjectProperty>()) {
				/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Array of Object Pointers, Members of User-Defined Structs, so this is disabled:
				LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Array of Object References (when members of User-Defined Structs) so this Struct Member will NOT be loaded:  %s"),*Field));
			} else {
				const auto JArray = JSON->GetArrayField(Field);
				FScriptArrayHelper Helper(_Array,ValuePtr);
				Helper.Resize(JArray.Num());
				//
				for (int32 I=0, N=JArray.Num()-1; I<=N; I++) {
					const auto &JValue = JArray[I];
					if (JValue.IsValid()&&(!JValue->IsNull())) {
						if (!FJsonObjectConverter::JsonValueToUProperty(JValue,_Array->Inner,Helper.GetRawPtr(I),0,SkipFlags)) {
							LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Array Element Type <-> Property Array Element Type] mismatch:  %s"),*Field));
						}
					}
				}
			}
		}
	}
	//
	if (FSetProperty* _Set = CastField<FSetProperty>(Property)) {
		const FString Field = (IsNative) ? _Set->GetName() : SanitizePropertyID(_Set->GetName());
		if (JSON->HasField(Field)) {
			if (_Set->ElementProp->IsA<FObjectProperty>()) {
				/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Set of Object Pointers, Members of User-Defined Structs, so this is disabled:
				LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Set of Object References (when members of User-Defined Structs) so this Struct Member will NOT be loaded:  %s"),*Field));
			} else {
				const auto JSet = JSON->GetArrayField(Field);
				FScriptSetHelper Helper(_Set,ValuePtr);
				Helper.EmptyElements();
				//
				for (int32 I=0, N=JSet.Num()-1; I<=N; I++) {
					const auto &JValue = JSet[I];
					const auto Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
					if (JValue.IsValid()&&(!JValue->IsNull())) {
						if (!FJsonObjectConverter::JsonValueToUProperty(JValue,_Set->ElementProp,Helper.GetElementPtr(Index),0,SkipFlags)) {
							LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Set Element Type <-> Property Set Element Type] mismatch:  %s"),*Field));
						}
					}
				}	Helper.Rehash();
			}
		}
	}
	//
	if (FMapProperty* _Map = CastField<FMapProperty>(Property)) {
		int32 Num=0;
		const FString Field = (IsNative) ? _Map->GetName() : SanitizePropertyID(_Map->GetName());
		if (JSON->HasField(Field+TEXT("_KEY"))) {
			if (_Map->KeyProp->IsA<FObjectProperty>()) {
				/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Map of Object Pointers, Members of User-Defined Structs, so this is disabled:
				LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Map of Object References (when members of User-Defined Structs) so this Struct Member will NOT be loaded:  %s"),*Field));
			} else {
				const auto JKeys = JSON->GetArrayField(Field+TEXT("_KEY"));
				FScriptMapHelper Helper(_Map,ValuePtr);
				Helper.EmptyValues(); Num = JKeys.Num();
				//
				for (int32 I=0, N=JKeys.Num()-1; I<=N; I++) {
					const auto &JKey = JKeys[I];
					const auto IKey = Helper.AddDefaultValue_Invalid_NeedsRehash();
					if (JKey.IsValid()&&(!JKey->IsNull())) {
						if (!FJsonObjectConverter::JsonValueToUProperty(JKey,_Map->KeyProp,Helper.GetKeyPtr(IKey),0,SkipFlags)) {
							LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Map Key Type <-> Property Map Key Type] mismatch:  %s"),*Field));
						}
					}
				}	Helper.Rehash();
			}
		}
		//
		if (JSON->HasField(Field+TEXT("_VALUE"))) {
			if (_Map->ValueProp->IsA<FObjectProperty>()) {
				/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Map of Object Pointers, Members of User-Defined Structs, so this is disabled:
				LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Map of Object References (when members of User-Defined Structs) so this Struct Member will NOT be loaded:  %s"),*Field));
			} else {
				const auto JValues = JSON->GetArrayField(Field+TEXT("_VALUE"));
				FScriptMapHelper Helper(_Map,ValuePtr);
				check(JValues.Num()==Num);
				//
				for (int32 I=0, N=JValues.Num()-1; I<=N; I++) {
					const auto &JValue = JValues[I];
					if (JValue.IsValid()&&(!JValue->IsNull())) {
						if (!FJsonObjectConverter::JsonValueToUProperty(JValue,_Map->ValueProp,Helper.GetValuePtr(I),0,SkipFlags)) {
							LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Map Value Type <-> Property Map Value Type] mismatch:  %s"),*Field));
						}
					}
				}
			}
		}
	}
	//
	if (FStructProperty* _Struct = CastField<FStructProperty>(Property)) {
		const FString Field = (IsNative) ? _Struct->GetName() : SanitizePropertyID(_Struct->GetName());
		if (JSON->HasField(Field)) {
			auto JStruct = JSON->GetObjectField(Field);
			if (JStruct.IsValid()) {ParseJSONtoFStruct(JStruct,_Struct,ValuePtr,Container);}
		}
	}
	//
	if (FObjectProperty* _Object = CastField<FObjectProperty>(Property)) {
		const FString Field = (IsNative) ? _Object->GetName() : SanitizePropertyID(_Object->GetName());
		if (JSON->HasField(Field)) {
			/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Object Pointers, Members of User-Defined Structs, so this is disabled:
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Object References (when members of User-Defined Structs) so this Struct Member will NOT be loaded:  %s"),*Field));
		} else {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Object Struct Member <-> Property Object Struct Member] mismatch:  %s"),*Field));}
	}
	//
	if (FClassProperty* _Class = CastField<FClassProperty>(Property)) {
		const FString Field = (IsNative) ? _Class->GetName() : SanitizePropertyID(_Class->GetName());
		if (JSON->HasField(Field)) {
			/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Object Pointers, Members of User-Defined Structs, so this is disabled:
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Object References (when members of User-Defined Structs) so this Struct Member will NOT be loaded:  %s"),*Field));
		} else {LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("[Data Class Struct Member <-> Property Class Struct Member] mismatch:  %s"),*Field));}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Struct Member Parser:

void Reflector::ParseFStructPropertyToJSON(TSharedPtr<FJsonObject>&JSON, FProperty* Property, const void* ValuePtr, const UObject* Container, int64 CheckFlags, int64 SkipFlags, const bool IsNative) {
	TSharedPtr<FJsonValue>JValue;
	//
	if (FBoolProperty* _Bool = CastField<FBoolProperty>(Property)) {
		JValue = FJsonObjectConverter::UPropertyToJsonValue(_Bool,ValuePtr,CheckFlags,SkipFlags);
		const FString ID = (IsNative) ? _Bool->GetName() : SanitizePropertyID(_Bool->GetName());
		if (JValue.IsValid()&&(!JValue->IsNull())) {JSON->SetField(ID,JValue);}
	}
	//
	if (FEnumProperty* _Enum = CastField<FEnumProperty>(Property)) {
		UEnum* EnumPtr = _Enum->GetEnum();
		FString Value = EnumPtr->GetNameStringByValue(_Enum->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(ValuePtr));
		const FString ID = (IsNative) ? _Enum->GetName() : SanitizePropertyID(_Enum->GetName());
		JSON->SetStringField(ID,Value);
	}
	//
	if (FNumericProperty* _Numeric = CastField<FNumericProperty>(Property)) {
		JValue = FJsonObjectConverter::UPropertyToJsonValue(_Numeric,ValuePtr,CheckFlags,SkipFlags);
		const FString ID = (IsNative) ? _Numeric->GetName() : SanitizePropertyID(_Numeric->GetName());
		if (JValue.IsValid()&&(!JValue->IsNull())) {JSON->SetField(ID,JValue);}
	}
	//
	if (FNameProperty* _Name = CastField<FNameProperty>(Property)) {
		JValue = FJsonObjectConverter::UPropertyToJsonValue(_Name,ValuePtr,CheckFlags,SkipFlags);
		const FString ID = (IsNative) ? _Name->GetName() : SanitizePropertyID(_Name->GetName());
		if (JValue.IsValid()&&(!JValue->IsNull())) {JSON->SetField(ID,JValue);}
	}
	//
	if (FStrProperty* _String = CastField<FStrProperty>(Property)) {
		JValue = FJsonObjectConverter::UPropertyToJsonValue(_String,ValuePtr,CheckFlags,SkipFlags);
		const FString ID = (IsNative) ? _String->GetName() : SanitizePropertyID(_String->GetName());
		if (JValue.IsValid()&&(!JValue->IsNull())) {JSON->SetField(ID,JValue);}
	}
	//
	if (FTextProperty* _Text = CastField<FTextProperty>(Property)) {
		JValue = FJsonObjectConverter::UPropertyToJsonValue(_Text,ValuePtr,CheckFlags,SkipFlags);
		const FString ID = (IsNative) ? _Text->GetName() : SanitizePropertyID(_Text->GetName());
		if (JValue.IsValid()&&(!JValue->IsNull())) {JSON->SetField(ID,JValue);}
	}
	//
	if (FArrayProperty* _Array = CastField<FArrayProperty>(Property)) {
		FScriptArrayHelper Helper(_Array,ValuePtr);
		TArray<TSharedPtr<FJsonValue>>JArray;
		if (_Array->Inner->IsA<FObjectProperty>()||_Array->Inner->IsA<FClassProperty>()) {
			/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Array of Object Pointers, Members of User-Defined Structs, so this is disabled:
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Array of Object References (when members of User-Defined Structs) so this Struct Member will NOT be saved:  %s"),*_Array->GetName()));
		} else {
			for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
				auto Value = FJsonObjectConverter::UPropertyToJsonValue(_Array->Inner,Helper.GetRawPtr(I),CheckFlags,SkipFlags);
				if (Value.IsValid()&&(!Value->IsNull())) {JArray.Push(Value);}
			}
		} if (JArray.Num()>0) {
			const FString ID = (IsNative) ? _Array->GetName() : SanitizePropertyID(_Array->GetName());
			JSON->SetArrayField(ID,JArray);
		}
	}
	//
	if (FSetProperty* _Set = CastField<FSetProperty>(Property)) {
		FScriptSetHelper Helper(_Set,ValuePtr);
		TArray<TSharedPtr<FJsonValue>>JArray;
		if (_Set->ElementProp->IsA<FObjectProperty>()||_Set->ElementProp->IsA<FClassProperty>()) {
			/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Set of Object Pointers, Members of User-Defined Structs, so this is disabled:
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Set of Object References (when members of 'User-Defined Structs') so this Struct Member will NOT be saved:  %s"),*_Set->GetName()));
		} else {
			for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
				auto Value = FJsonObjectConverter::UPropertyToJsonValue(_Set->ElementProp,Helper.GetElementPtr(I),CheckFlags,SkipFlags);
				if (Value.IsValid()&&(!Value->IsNull())) {JArray.Push(Value);}
			}
		} if (JArray.Num()>0) {
			const FString ID = (IsNative) ? _Set->GetName() : SanitizePropertyID(_Set->GetName());
			JSON->SetArrayField(ID,JArray);
		}
	}
	//
	if (FMapProperty* _Map = CastField<FMapProperty>(Property)) {
		TArray<TSharedPtr<FJsonValue>>Keys;
		TArray<TSharedPtr<FJsonValue>>Values;
		FScriptMapHelper Helper(_Map,ValuePtr);
		//
		if (_Map->KeyProp->IsA<FObjectProperty>()||_Map->KeyProp->IsA<FClassProperty>()) {
			/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Map of Object Pointers, Members of User-Defined Structs, so this is disabled:
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Map of Object References (when members of 'User-Defined Structs') so this Struct Member will NOT be saved:  %s"),*_Map->KeyProp->GetName()));
		} else {
			for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
				auto Key = FJsonObjectConverter::UPropertyToJsonValue(_Map->KeyProp,Helper.GetKeyPtr(I),CheckFlags,SkipFlags);
				if (Key.IsValid()&&(!Key->IsNull())) {Keys.Push(Key);}
			}
		}
		//
		if (_Map->ValueProp->IsA<FObjectProperty>()||_Map->ValueProp->IsA<FClassProperty>()) {
			/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Map of Object Pointers, Members of User-Defined Structs, so this is disabled:
			LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Map of Object References (when members of 'User-Defined Structs') so this Struct Member will NOT be saved:  %s"),*_Map->ValueProp->GetName()));
		} else {
			for (int32 I=0, N=Helper.Num()-1; I<=N; I++) {
				auto Value = FJsonObjectConverter::UPropertyToJsonValue(_Map->ValueProp,Helper.GetValuePtr(I),CheckFlags,SkipFlags);
				if (Value.IsValid()&&(!Value->IsNull())) {Values.Push(Value);}
			}
		}
		//
		if ((Keys.Num()>0)&&(Values.Num()>0)) {
			const FString ID = (IsNative) ? _Map->GetName() : SanitizePropertyID(_Map->GetName());
			JSON->SetArrayField(ID+TEXT("_KEY"),Keys);
			JSON->SetArrayField(ID+TEXT("_VALUE"),Values);
		}
	}
	//
	if (FStructProperty* _Struct = CastField<FStructProperty>(Property)) {
		const FString ID = (IsNative) ? _Struct->GetName() : SanitizePropertyID(_Struct->GetName());
		JSON->SetObjectField(ID,ParseFStructToJSON(_Struct,ValuePtr,Container));
	}
	//
	if (FObjectProperty* _Object = CastField<FObjectProperty>(Property)) {
		/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Object Pointers, Members of User-Defined Structs, so this is disabled:
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Object References (when members of User-Defined Structs) so this Struct Member will NOT be saved:  %s"),*_Object->GetName()));
	}
	//
	if (FClassProperty* _Class = CastField<FClassProperty>(Property)) {
		/// Can't go so deep. Unreal Engine has a deep check to prevent us from serializing any Object Pointers, Members of User-Defined Structs, so this is disabled:
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Unreal Engine explicitly disallows serializing ASCII Object References (when members of User-Defined Structs) so this Struct Member will NOT be saved:  %s"),*_Class->GetName()));
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ID Interface:

bool Reflector::HasGUID(UObject* Container) {
	if (!Container->IsValidLowLevelFast()) {return false;}
	//
	for (TFieldIterator<FProperty>PIT(Container->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
		&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {return true;}
	}
	//
	return false;
}

const FGuid Reflector::FindGUID(UObject* Container) {
	FGuid GUID; GUID.Invalidate();
	//
	if (!Container->IsValidLowLevelFast()) {return GUID;}
	//
	for (TFieldIterator<FProperty>PIT(Container->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			GUID = ParseGUID(CastFieldChecked<FStructProperty>(Property),Container);
		break;}
	}
	//
	return GUID;
}

const FGuid Reflector::FindGUID(const UObject* Container) {
	FGuid GUID; GUID.Invalidate();
	//
	if (!Container->IsValidLowLevelFast()) {return GUID;}
	//
	for (TFieldIterator<FProperty>PIT(Container->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			GUID = ParseGUID(CastFieldChecked<FStructProperty>(Property),Container);
		break;}
	}
	//
	return GUID;
}

const FString Reflector::FindSGUID(UObject* Container) {
	if (!Container->IsValidLowLevelFast()) {
		return TEXT("");
	} FString GUID;
	//
	for (TFieldIterator<FProperty>PIT(Container->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			GUID = ParseSGUID(CastFieldChecked<FStructProperty>(Property),Container);
		break;}
	}
	//
	return GUID;
}

const FString Reflector::FindSGUID(const UObject* Container) {
	if (!Container->IsValidLowLevelFast()) {
		return TEXT("");
	} FString GUID;
	//
	for (TFieldIterator<FProperty>PIT(Container->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			GUID = ParseSGUID(CastFieldChecked<FStructProperty>(Property),Container);
		break;}
	}
	//
	return GUID;
}

void Reflector::SetGUID(const FGuid &GUID, UObject* Container) {
	for (TFieldIterator<FProperty>PIT(Container->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			auto OID = Property->ContainerPtrToValuePtr<FGuid>(Container);
			if (OID) {(*OID)=GUID;}
		break;}
	}
}

const FGuid Reflector::ParseGUID(FStructProperty* Property, const UObject* Container) {
	FGuid GUID; GUID.Invalidate();
	//
	UScriptStruct* Struct = Property->Struct;
	if (Struct!=TBaseStructure<FGuid>::Get()) {return GUID;}
	//
	const FGuid* StructPtr = Property->ContainerPtrToValuePtr<FGuid>(Container);
	const auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,0);
	//
	FGuid::Parse(Value->AsString(),GUID);
	//
	return GUID;
}

const FString Reflector::ParseSGUID(FStructProperty* Property, const UObject* Container) {
	FGuid GUID; GUID.Invalidate();
	//
	UScriptStruct* Struct = Property->Struct;
	if (Struct!=TBaseStructure<FGuid>::Get()) {return FString();}
	//
	const FGuid* StructPtr = Property->ContainerPtrToValuePtr<FGuid>(Container);
	const auto Value = FJsonObjectConverter::UPropertyToJsonValue(CastFieldChecked<FProperty>(Property),StructPtr,0,0);
	//
	if (FGuid::Parse(Value->AsString(),GUID)) {
		if (GUID.IsValid()) {return GUID.ToString();}
	}
	//
	return FString();
}

bool Reflector::IsMatchingSGUID(UObject* Target, UObject* ComparedTo) {
	if (!Target->IsValidLowLevelFast()||!ComparedTo->IsValidLowLevelFast()) {return false;}
	FGuid GUID; GUID.Invalidate();
	FGuid OID; OID.Invalidate();
	//
	for (TFieldIterator<FProperty>PIT(Target->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			GUID = ParseGUID(CastFieldChecked<FStructProperty>(Property),Target);
		break;}
	}
	//
	for (TFieldIterator<FProperty>PIT(ComparedTo->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			OID = ParseGUID(CastFieldChecked<FStructProperty>(Property),ComparedTo);
		break;}
	}
	//
	return (GUID==OID);
}

bool Reflector::IsMatchingSGUID(const FGuid &SGUID, UObject* Target) {
	if (!Target->IsValidLowLevelFast()) {return false;}
	FGuid OID; OID.Invalidate();
	//
	for (TFieldIterator<FProperty>PIT(Target->GetClass(),EFieldIteratorFlags::IncludeSuper,EFieldIteratorFlags::ExcludeDeprecated,EFieldIteratorFlags::ExcludeInterfaces); PIT; ++PIT) {
		FProperty* Property = *PIT;
		//
		const bool IsStruct = Property->IsA<FStructProperty>();
		if ((IsStruct)&&(CastFieldChecked<FStructProperty>(Property)->Struct==TBaseStructure<FGuid>::Get())
			&&(Property->GetName().Equals(TEXT("SGUID"),ESearchCase::CaseSensitive))) {
			OID = ParseGUID(CastFieldChecked<FStructProperty>(Property),Target);
		break;}
	}
	//
	return (SGUID==OID);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Reflector::GetPlayerNetworkID(const APlayerController* PlayerController, FString &ID, const bool AppendPort) {
	if (!PlayerController) {return false;}
	if (!PlayerController->PlayerState->IsValidLowLevelFast()) {return false;}
	if (!PlayerController->PlayerState->GetUniqueId().IsValid()) {return false;}
	//
	ID = PlayerController->PlayerState->GetUniqueId()->ToString();
	if ((!ID.IsEmpty())&&(!AppendPort)) {ID.Split(FString(":"),&ID,nullptr,ESearchCase::IgnoreCase,ESearchDir::FromEnd);}
	//
	return false;
}

const FName Reflector::MakeObjectID(UObject* OBJ, bool FullPath) {
	if (OBJ==nullptr) {
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Cannot create Object ID for invalid object reference.")));
		return TEXT("ERROR:ID");
	}
	//
	FString OwnerID, SequenceNum;
	const auto &Owner = OBJ->GetOuter();
	const auto &Outermost = OBJ->GetTypedOuter<UWorld>();
	//
	FString Temp;
	if (Owner) {
		FString OwnerGUID = FindSGUID(Owner);
		OwnerID = (FullPath) ? (FString(TEXT("/"))+Owner->GetFullName(Outermost)) : Owner->GetFName().ToString();
		//
		if (OwnerID.Contains(TEXT("_C_"))) {
			Temp = OwnerID;
			OwnerID.Split(TEXT("_C_"),&OwnerID, &SequenceNum,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
			if(SequenceNum.IsNumeric())
				OwnerID += "_" + SequenceNum;
			else
				OwnerID += SequenceNum;
		}
		else if (OwnerID.EndsWith("_C"))
			OwnerID.Split(TEXT("_C"), &OwnerID, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		//
		if (!OwnerGUID.IsEmpty()) {
			OwnerID.Append(TEXT("_"));
			OwnerID.Append(OwnerGUID);
		}
	}
	//
	FString NetworkID;
	FString GUID = FindSGUID(OBJ);
	FString ID = (FullPath) ? OBJ->GetFullName(Owner) : OBJ->GetFName().ToString();
	//
	if (ID.Contains(TEXT("_C_"))) {
		Temp = ID;
		ID.Split(TEXT("_C_"),&ID, &SequenceNum,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
		if (SequenceNum.IsNumeric())
			ID += "_" + SequenceNum;
		else
			ID += SequenceNum;
	}
	else if (ID.EndsWith("_C"))
		ID.Split(TEXT("_C"), &ID, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	//
	if (!GUID.IsEmpty()) {
		ID.Append(TEXT("_")); ID.Append(GUID);
	} if (!OwnerID.IsEmpty()) {ID=(OwnerID+FString(TEXT("_"))+ID);}
	//
	if (AActor*const&ACTO=Cast<AActor>(Owner)) {
		if (ACTO->GetNetOwningPlayer()) {
			if (!GetPlayerNetworkID(ACTO->GetNetOwningPlayer()->GetPlayerController(ACTO->GetWorld()),NetworkID,false)) {
				if (ACTO->GetNetOwningPlayer()->GetPlayerController(ACTO->GetWorld())) {
					ID = FString(TEXT("P"))+FString::FromInt(UGameplayStatics::GetPlayerControllerID(ACTO->GetNetOwningPlayer()->GetPlayerController(ACTO->GetWorld())))+(FString(TEXT("_"))+ID);
				}
			} else {ID=(NetworkID+FString(TEXT("_"))+ID);}
		}
	} else if (UActorComponent*const&CMP_Outer=Cast<UActorComponent>(Owner)) {
		if (AActor*const&ACTC=CMP_Outer->GetOwner()) {
			if (ACTC->GetNetOwningPlayer()) {
				if (!GetPlayerNetworkID(ACTC->GetNetOwningPlayer()->GetPlayerController(ACTC->GetWorld()),NetworkID,false)) {
					if (ACTC->GetNetOwningPlayer()->GetPlayerController(ACTC->GetWorld())) {
						ID = FString(TEXT("P"))+FString::FromInt(UGameplayStatics::GetPlayerControllerID(ACTC->GetNetOwningPlayer()->GetPlayerController(ACTC->GetWorld())))+(FString(TEXT("_"))+ID);
					}
				} else {ID=(NetworkID+FString(TEXT("_"))+ID);}
			}
		}
	}
	//
	if (ID.IsEmpty()) {ID=OBJ->GetName();} return *ID;
}

const FName Reflector::MakeActorID(AActor* Actor, bool FullPath) {
	if (Actor==nullptr) {
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Cannot create Actor ID for invalid actor reference.")));
		return TEXT("ERROR:ID");
	}
	//
	const auto &Outermost = Actor->GetTypedOuter<UWorld>();
	//
	FString NetworkID;
	FString GUID = FindSGUID(Actor);
	FString ID = (FullPath) ? (FString(TEXT("/"))+Actor->GetFullName(Outermost)) : Actor->GetFName().ToString();
	//
	FString Temp, SequenceNum;
	if (ID.Contains(TEXT("_C_"))) {
		Temp = ID;
		ID.Split(TEXT("_C_"), &ID, &SequenceNum, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (SequenceNum.IsNumeric())
			ID += "_" + SequenceNum;
		else
			ID += SequenceNum;
	}
	else if (ID.EndsWith("_C"))
		ID.Split(TEXT("_C"), &ID, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	//
	if (!GUID.IsEmpty()) {
		ID.Append(TEXT("_"));
		ID.Append(GUID);
	}
	//
	if (Actor->GetNetOwningPlayer()) {
		if (!GetPlayerNetworkID(Actor->GetNetOwningPlayer()->GetPlayerController(Actor->GetWorld()),NetworkID,false)) {
			if (Actor->GetNetOwningPlayer()->GetPlayerController(Actor->GetWorld())) { ID.RemoveFromStart(TEXT("/"));
				ID = FString(TEXT("/P"))+FString::FromInt(UGameplayStatics::GetPlayerControllerID(Actor->GetNetOwningPlayer()->GetPlayerController(Actor->GetWorld())))+(FString(TEXT("_"))+ID);
			}
		} else {ID=(NetworkID+FString(TEXT("_"))+ID);}
	} if (ID.IsEmpty()) {ID=Actor->GetName();} return *ID;
}

const FName Reflector::MakeComponentID(UActorComponent* CMP, bool FullPath) {
	if (CMP==nullptr) {
		LOG_SV(true,ESeverity::Warning,FString::Printf(TEXT("Cannot create Component ID for invalid component reference.")));
		return TEXT("ERROR:ID");
	}
	//
	FString OwnerID, Temp, SequenceNum;
	const auto &Owner = CMP->GetOwner();
	const auto &Outermost = CMP->GetTypedOuter<UWorld>();
	//
	if (Owner) {
		FString OwnerGUID = FindSGUID(Owner);
		OwnerID = (FullPath) ? (FString(TEXT("/"))+Owner->GetFullName(Outermost)) : Owner->GetFName().ToString();
		//
		if (OwnerID.Contains(TEXT("_C_"))) {
			Temp = OwnerID;
			OwnerID.Split(TEXT("_C_"), &OwnerID, &SequenceNum, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (SequenceNum.IsNumeric())
				OwnerID += "_" + SequenceNum;
			else
				OwnerID += SequenceNum;
		}
		else if (OwnerID.EndsWith("_C"))
			OwnerID.Split(TEXT("_C"), &OwnerID, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		//
		if (!OwnerGUID.IsEmpty()) {
			OwnerID.Append(TEXT("_"));
			OwnerID.Append(OwnerGUID);
		}
	}
	//
	FString NetworkID;
	FString GUID = FindSGUID(CMP);
	FString ID = /*(FullPath) ? CMP->GetFullName(Outermost) :*/ CMP->GetFName().ToString();
	//
	if (ID.Contains(TEXT("_C_"))) {
		Temp = ID;
		ID.Split(TEXT("_C_"), &ID, &SequenceNum, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (SequenceNum.IsNumeric())
			ID += "_" + SequenceNum;
		else
			ID += SequenceNum;
	}
	else if (ID.EndsWith("_C"))
		ID.Split(TEXT("_C"), &ID, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	//
	if (!GUID.IsEmpty()) {
		ID.Append(TEXT("_"));
		ID.Append(GUID);
	}
	//
	if (!OwnerID.IsEmpty()) {ID=(OwnerID+FString(TEXT("_"))+ID);}
	//
	if (Owner && Owner->GetNetOwningPlayer()) {
		if (!GetPlayerNetworkID(Owner->GetNetOwningPlayer()->GetPlayerController(Owner->GetWorld()),NetworkID,false)) {
			if (Owner->GetNetOwningPlayer()->GetPlayerController(Owner->GetWorld())) { ID.RemoveFromStart(TEXT("/"));
				ID = FString(TEXT("/P"))+FString::FromInt(UGameplayStatics::GetPlayerControllerID(Owner->GetNetOwningPlayer()->GetPlayerController(Owner->GetWorld())))+(FString(TEXT("_"))+ID);
			}
		} else {ID=(NetworkID+FString(TEXT("_"))+ID);}
	} if (ID.IsEmpty()) {ID=CMP->GetName();} return *ID;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////