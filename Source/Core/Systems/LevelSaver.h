// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Savior.h"
#include "SaviorMetaData.h"
#include <Core/Systems/LevelData.h>
#include "LevelSaver.generated.h"


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

	UPROPERTY(SaveGame)
	bool Saved = false;

	// Getters

	UFUNCTION(BlueprintCallable)
	ULevelData* GetLevelData(FString InLevelName);

	// Setters

	// Wrappers

	// External Virtual Functions

	// External Overrides

	// External Regular Functions

	// External Events

//======================================================================================
// C++ Public
public:

	// Flags

	// Initialized Variables

	UPROPERTY(SaveGame)
	TArray<ULevelData*> Levels;

	// Constructor
	ULevelSaver();

	// Initializers, and Actor Lifecycle Functions

	void SaveLevelData(FString InLevelName);

	void LoadLevelData(FString InLevelName);

	void SaveGame();

	void LoadGame();

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
