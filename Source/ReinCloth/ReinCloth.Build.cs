// Copyright © 2026 kafues511 All Rights Reserved.

using UnrealBuildTool;

public class ReinCloth : ModuleRules
{
	public ReinCloth(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDefinitions.Add("REIN_CLOTH_SETUP_MODE=REIN_CLOTH_SETUP_MODE_LIB");

		PrivateIncludePaths.AddRange(
			new string[]
			{
				System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				// ... add private dependencies that you statically link with here ...
				"Projects",
				"RenderCore",
				"Renderer",
				"RHI",
			}
		);

		if (Target.bBuildEditor && Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"GeometryAlgorithms",
					"GeometryCore",
					// ... add private dependencies that you statically link with here ...
					"MeshDescription",
					"StaticMeshDescription",
				}
			);

			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "lib", "ReinClothLib.lib"));
		}
	}
}
