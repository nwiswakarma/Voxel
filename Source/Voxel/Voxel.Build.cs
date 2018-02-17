// 

using System.IO;
using System.Collections;
using UnrealBuildTool;

public class Voxel : ModuleRules
{
    public Voxel(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Get the engine path. Ends with "Engine/"
        string EnginePath = Path.GetFullPath(BuildConfiguration.RelativeEnginePath);

        // Include path for tessellation library
        PublicIncludePaths.Add(EnginePath + "Source/ThirdParty/nvtesslib/inc");

        PublicIncludePaths.AddRange(
            new string[] { }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                "Voxel/Private",
                "Voxel/Private/VoxelData",
                "Voxel/Private/VoxelRender",

                "Voxel/Private/VoxelAssets",
                "Voxel/Private/VoxelModifiers",
                "Voxel/Private/VoxelWorldGenerators",

                "Voxel/Private/ThirdParty",

                "Voxel/Classes/VoxelAssets",
                "Voxel/Classes/VoxelModifiers",
                "Voxel/Classes/VoxelWorldGenerators",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Landscape",
                "Sockets",
                "Networking",
                "RenderCore",
                "ShaderCore",
                "RHI"
            }
        );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "ProceduralMeshComponent",
                "GenericWorkerThread",
                "DracoPlugin",
                "ZSTDPlugin",
                "SQLite3Plugin"
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
