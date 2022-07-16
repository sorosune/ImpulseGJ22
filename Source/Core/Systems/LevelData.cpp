// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelData.h"

ULevelData::ULevelData()
{
	SGUID = USavior::MakeObjectGUID(this, EGuidGeneratorMode::ResolvedNameToGUID);
}