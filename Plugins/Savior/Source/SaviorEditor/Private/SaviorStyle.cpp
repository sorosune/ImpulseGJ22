//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//			Copyright 2022 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SaviorStyle.h"

#include "Styling/SlateStyle.h"
#include "Interfaces/IPluginManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PLUGIN_BRUSH(RelativePath,...) FSlateImageBrush(FSaviorStyle::InContent(RelativePath,".png"),__VA_ARGS__)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FSlateStyleSet> FSaviorStyle::StyleSet = nullptr;
TSharedPtr<ISlateStyle> FSaviorStyle::Get() {return StyleSet;}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString FSaviorStyle::InContent(const FString &RelativePath, const ANSICHAR* Extension) {
	static FString Content = IPluginManager::Get().FindPlugin(TEXT("Savior"))->GetContentDir();
	return (Content/RelativePath)+Extension;
}

FName FSaviorStyle::GetStyleSetName() {
	static FName StyleName(TEXT("SaviorStyle"));
	return StyleName;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FSaviorStyle::Initialize() {
	if (StyleSet.IsValid()) {return;}
	//
	const FVector2D Icon16x16(16.0f,16.0f);
	const FVector2D Icon128x128(128.0f,128.0f);
	//
	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	//
	StyleSet->Set("ClassIcon.Savior", new PLUGIN_BRUSH(TEXT("Icons/Savior_16x"),Icon16x16));
	StyleSet->Set("ClassThumbnail.Savior", new PLUGIN_BRUSH(TEXT("Icons/Savior_128x"),Icon128x128));
	//
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

void FSaviorStyle::Shutdown() {
	if (StyleSet.IsValid()) {
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef PLUGIN_BRUSH

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////