// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImpulseGJ22 : ModuleRules
{
	public ImpulseGJ22(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "Savior" });
	}
}
