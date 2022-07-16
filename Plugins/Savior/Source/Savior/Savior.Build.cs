// Copyright 2022 (C) Bruno Xavier Leite

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules {
//
public class Savior : ModuleRules {
    public Savior(ReadOnlyTargetRules Target) : base(Target) {
        PrivatePCHHeaderFile = "Public/Savior_Shared.h";
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnforceIWYU = true;
		//
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"UMG",
				"Core",
				"Json",
				"Slate",
				"Engine",
				"SlateCore",
				"CoreUObject",
				"MoviePlayer",
				"GameplayTags",
				"JsonUtilities"
			}
		);
		//
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory,"Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory,"Private"));
	}
}
//
}