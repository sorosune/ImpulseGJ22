// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Savior.h"
#include "SaviorMetaData.h"
#include "LevelSaver.generated.h"


USTRUCT(BlueprintType)
struct FLevelData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	FString LevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int BestTime = -1;
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

	// External Events

//======================================================================================
// C++ Public
public:

	// Flags

	// Initialized Variables

	// Constructor
	ULevelSaver();

	// Initializers, and Actor Lifecycle Functions

	void SaveGame();

	void LoadGame();

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
