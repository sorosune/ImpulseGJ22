// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Savior.h"
#include "SaviorMetaData.h"
#include "LevelData.generated.h"


/**
 * 
 */
UCLASS()
class IMPULSEGJ22_API ULevelData : public UAutoInstanced
{
	GENERATED_BODY()

//======================================================================================
// Blueprint Public
public:

	// Blueprint Flags

	// Blueprint Variables

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGuid SGUID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	FString LevelName;

	UPROPERTY(SaveGame)
	bool Saved = false;

	// Getters

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
	ULevelData();

	// Initializers, and Actor Lifecycle Functions

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
