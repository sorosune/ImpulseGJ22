// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LevelSaver.h"
#include "ImpulseGameInstance.generated.h"


/**
 * 
 */
UCLASS()
class IMPULSEGJ22_API UImpulseGameInstance : public UGameInstance
{
	GENERATED_BODY()

//======================================================================================
// Blueprint Public
public:

	// Blueprint Flags

	// Blueprint Variables

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ULevelSaver* LevelSaver;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<ULevelSaver> LevelSaverClass;

	// Getters

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "Context"))
	static UImpulseGameInstance* GetImpulseGameInstance(UObject* Context);

	// Setters

	UFUNCTION(BlueprintCallable)
	void SetLevelData(FLevelData InLevelData) { LevelSaver->SetLevelData(InLevelData); }

	// Wrappers

	// External Virtual Functions

	// External Overrides

	// External Regular Functions

	UFUNCTION(BlueprintCallable)
	void SaveGame() { LevelSaver->SaveGame(); }

	UFUNCTION(BlueprintCallable)
	void LoadGame() { LevelSaver->LoadGame(); }

	// External Events

//======================================================================================
// C++ Public
public:

	// Flags

	// Initialized Variables

	// Constructor

	// Initializers, and Actor Lifecycle Functions

	virtual void Init() override;

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
