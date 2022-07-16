// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelSaver.h"
#include <Core/HelperFiles/DefinedDebugHelpers.h>


ULevelSaver::ULevelSaver()
{
	SGUID = USavior::MakeObjectGUID(this, EGuidGeneratorMode::ResolvedNameToGUID);
}

ULevelData* ULevelSaver::GetLevelData(FString InLevelName)
{
	for (auto& Level : Levels)
		if (Level->LevelName == InLevelName)
			return Level;
	return nullptr;
}

void ULevelSaver::SaveLevelData(FString InLevelName)
{
	ULevelData* LevelData = GetLevelData(InLevelName);
	if (!LevelData)
	{
		V_LOG("Failed to save data for level: " + InLevelName);
		return;
	}
	ESaviorResult result, resulth;
	SaveSlot->SaveObjectHierarchy(LevelData, result, resulth);
}

void ULevelSaver::LoadLevelData(FString InLevelName)
{
	ULevelData* LevelData = GetLevelData(InLevelName);
	if (!LevelData)
	{
		V_LOG("Failed to load data for level: " + InLevelName);
		return;
	}
	ESaviorResult result, resulth;
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
	SaveSlot->ReadSlotFromFile(0, result);
	SaveSlot->LoadObjectHierarchy(LevelData, result, resulth);
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
}

void ULevelSaver::SaveGame()
{
	Saved = true;
	ESaviorResult result, resulth;
	SaveSlot->SaveObjectHierarchy(this, result, resulth);
	SaveSlot->WriteSlotToFile(0, result);
}

void ULevelSaver::LoadGame()
{
	ESaviorResult result, resulth;
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
	SaveSlot->ReadSlotFromFile(0, result);
	SaveSlot->LoadObjectHierarchy(this, result, resulth);
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
}
