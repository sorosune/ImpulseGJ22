// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Savior.h"
#include "SaviorMetaData.h"
#include <Runtime/Engine/Classes/Slate/SlateBrushAsset.h>
#include "LevelSaver.generated.h"


USTRUCT(BlueprintType)
struct FLevelData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FString LevelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int BestTime = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool LevelLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool LevelHidden = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<int> TrophyIndices;

};


/**
 * 
 */
UCLASS()
class IMPULSEGJ22_API ULevelSaver : public UAutoInstanced
{
	GENERATED_BODY()

//======================================================================================
// Blueprint Public
public:

	// Blueprint Flags

	// Blueprint Variables

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGuid SGUID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USavior* SaveSlot;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FLevelData> Levels;

	// Getters

	UFUNCTION(BlueprintCallable)
	FLevelData GetLevelData(FString InLevelName);

	// Setters

	// Wrappers

	// External Virtual Functions

	// External Overrides

	// External Regular Functions

	UFUNCTION(BlueprintCallable)
	void AddLevelData(FLevelData InLevelData) { Levels.Add(InLevelData); }

	// External Events

//======================================================================================
// C++ Public
public:

	// Flags

	// Initialized Variables

	// Constructor
	ULevelSaver();

	// Initializers, and Actor Lifecycle Functions

	bool SaveGame();

	bool LoadGame();

	void SetLevelData(FLevelData InLevelData);

//======================================================================================
// C++ Protected
protected:

	// Internal Variables

	// Internal Virtual Functions

	// Internal Overrides

	// Internal Regular Functions

	// Internal Events and Implementations 

//======================================================================================
// C++ Private
private:

	// Internal Variables

	// Overrides

	// Regular Functions
	
};
