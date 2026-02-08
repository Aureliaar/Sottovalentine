// Copyright Theory of Magic. All Rights Reserved.

using UnrealBuildTool;

public class ShortStoryEditor : ModuleRules
{
	public ShortStoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"ShortStory"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetTools",
				"Slate",
				"SlateCore"
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
