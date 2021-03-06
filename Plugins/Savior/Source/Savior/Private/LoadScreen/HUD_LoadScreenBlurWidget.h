//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
///			Copyright 2022 (C) Bruno Xavier Leite
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Runtime/Slate/Public/SlateBasics.h"
#include "Runtime/SlateCore/Public/Fonts/SlateFontInfo.h"
#include "Runtime/Slate/Public/Widgets/Layout/SScaleBox.h"
#include "Runtime/SlateCore/Public/Widgets/SCompoundWidget.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SThrobber;
class SProgressBar;
class SBackgroundBlur;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class HUD_LoadScreenBlurWidget : public SCompoundWidget {
private:
	TSharedPtr<SBackgroundBlur>Background;
	TSharedPtr<SProgressBar>ProgressBar;
	TSharedPtr<STextBlock>Feedback;
	TSharedPtr<SThrobber>Throbber;
	//
	FLinearColor ProgressBarTint;
	FSlateFontInfo FeedbackFont;
	EThreadSafety TaskMode;
	FText FeedbackText;
	//
	float DPIScale = 1.f;
	float Ticker = 0.f;
	float Blur = 1.f;
	//
	float GetDPIScale() const;
	TOptional<float> GetWorkloadProgress() const;
public:
	SLATE_BEGIN_ARGS(HUD_LoadScreenBlurWidget)
	: _ProgressBarTint(), _FeedbackFont(), _TaskMode(), _FeedbackText(), _Blur()
	{}//
		SLATE_ARGUMENT(FLinearColor,ProgressBarTint);
		SLATE_ARGUMENT(FSlateFontInfo,FeedbackFont);
		SLATE_ARGUMENT(EThreadSafety,TaskMode);
		SLATE_ARGUMENT(FText,FeedbackText);
		SLATE_ARGUMENT(float,Blur);
	SLATE_END_ARGS()
	//
	void Construct(const FArguments &InArgs);
	virtual void Tick(const FGeometry &AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
public:
	void SetProgressBarTint(const FLinearColor &NewColor);
	void SetFeedBackFont(const FSlateFontInfo &NewFont);
	void SetTaskMode(const EThreadSafety NewMode);
	void SetFeedbackText(const FString &NewText);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////