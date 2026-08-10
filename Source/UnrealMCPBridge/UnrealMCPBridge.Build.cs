// Copyright Omar Abdelwahed 2025. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCPBridge : ModuleRules
{
	public UnrealMCPBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Sockets",
                "Networking"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"PythonScriptPlugin",
                "Json",
				"JsonUtilities",
				"Settings",
				"BlueprintGraph",
				"KismetCompiler",
				"Kismet",
				"UMG",
				"UMGEditor",
				"EditorScriptingUtilities",
				"Niagara",
				"NiagaraEditor",
				"Sequencer",
				"ImageWrapper",
				"LevelEditor",
				"TraceLog",
				"TraceAnalysis",
				"TraceServices"
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
