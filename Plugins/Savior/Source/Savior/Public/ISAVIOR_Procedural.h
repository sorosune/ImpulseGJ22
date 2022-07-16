//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2022 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SaviorTypes.h"
#include "Savior_Shared.h"

#include "ISAVIOR_Procedural.generated.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UINTERFACE(Category=Synaptech, BlueprintType, meta=(DisplayName="[SAVIOR] Procedural"))
class SAVIOR_API USAVIOR_Procedural : public UInterface {
	GENERATED_BODY()
};

class SAVIOR_API ISAVIOR_Procedural {
	GENERATED_BODY()
public:
	/// Begin Respawn Listener.
	UFUNCTION(Category="Savior", BlueprintNativeEvent, meta=(DisplayName="[SAVIOR] On Begin Respawn:", Keywords="Savior Respawn Begin"))
	void OnBeginRespawn(const FSaviorRecord &Data);
	//
	/// Finish Respawn Listener.
	UFUNCTION(Category="Savior", BlueprintNativeEvent, meta=(DisplayName="[SAVIOR] On Finish Respawn:", Keywords="Savior Respawn Finish"))
	void OnFinishRespawn(const FSaviorRecord &Data);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////