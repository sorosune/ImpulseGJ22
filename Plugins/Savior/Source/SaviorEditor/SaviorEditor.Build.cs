// Copyright 2022 (C) Bruno Xavier Leite

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules {
//
public class SaviorEditor : ModuleRules {
    public SaviorEditor(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Public/SaviorEditor_Shared.h";
		bEnforceIWYU = true;
		//
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Engine",
                "Savior",
                "CoreUObject"
            }
        );
        //
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Projects",
                "UnrealEd",
				"SlateCore",
                "AssetTools"
            }
        );
    }
}
//
}