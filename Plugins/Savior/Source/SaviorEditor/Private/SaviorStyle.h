//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2022 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Editor/UnrealEd/Public/ClassIconFinder.h"
#include "Runtime/SlateCore/Public/Styling/SlateStyle.h"
#include "Runtime/SlateCore/Public/Styling/SlateStyleRegistry.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FSaviorStyle {
private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
	static FString InContent(const FString &RelativePath, const ANSICHAR* Extension);
public:
	static void Initialize();
	static void Shutdown();
	static FName GetStyleSetName();
	static TSharedPtr<ISlateStyle> Get();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////