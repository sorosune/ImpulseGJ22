// Fill out your copyright notice in the Description page of Project Settings.


#include "ImpulseGameInstance.h"


void UImpulseGameInstance::Init()
{
	Super::Init();
	this->Rename(TEXT("ImpulseGameInstance"), GetOuter());
	LevelSaver = NewObject<ULevelSaver>(this, LevelSaverClass);
	LevelSaver->LoadGame();
}

UImpulseGameInstance* UImpulseGameInstance::GetImpulseGameInstance(UObject* Context)
{
	UWorld* World = Context->GetWorld();
	if (!World)
		return nullptr;
	return Cast<UImpulseGameInstance>(World->GetGameInstance());
}