// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelSaver.h"
#include <Core/HelperFiles/DefinedDebugHelpers.h>


ULevelSaver::ULevelSaver()
{
	SGUID = USavior::MakeObjectGUID(this, EGuidGeneratorMode::ResolvedNameToGUID);
}

FLevelData ULevelSaver::GetLevelData(FString InLevelName)
{
	for (auto& Level : Levels)
		if (Level.LevelName == InLevelName)
			return Level;
	return FLevelData();
}

void ULevelSaver::SaveGame()
{
	ESaviorResult result, resulth;
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
	SaveSlot->SaveObjectHierarchy(this, result, resulth);
	if(result != ESaviorResult::Failed)
		SaveSlot->WriteSlotToFile(0, result);
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
}

void ULevelSaver::LoadGame()
{
	ESaviorResult result, resulth;
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
	SaveSlot->ReadSlotFromFile(0, result);
	SaveSlot->LoadObjectHierarchy(this, result, resulth);
	SaveSlot = USavior::NewSlotInstance(this, SaveSlot, result);
}
