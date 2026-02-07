// Copyright Theory of Magic. All Rights Reserved.

using UnrealBuildTool;

public class ShortStory : ModuleRules
{
	public ShortStory(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayTags",
				"Json",
				"JsonUtilities"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"FMODStudio",
				"ImageWrapper",
				"RenderCore",
				"Projects" // For IPluginManager
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
