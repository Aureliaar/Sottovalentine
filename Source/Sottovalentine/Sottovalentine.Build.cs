// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sottovalentine : ModuleRules
{
	public Sottovalentine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Sottovalentine",
			"Sottovalentine/Variant_Horror",
			"Sottovalentine/Variant_Horror/UI",
			"Sottovalentine/Variant_Shooter",
			"Sottovalentine/Variant_Shooter/AI",
			"Sottovalentine/Variant_Shooter/UI",
			"Sottovalentine/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
