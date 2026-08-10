// Copyright Omar Abdelwahed 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UMGAuthoringLibrary.generated.h"

class UWidgetBlueprint;
class UWidget;
class UUserWidget;
class APlayerController;

/**
 * Editor-only library for authoring Widget Blueprints (UMG) from Python / MCP.
 *
 * Pure-Python authoring already covers most construction (create the WBP asset, load the editor
 * WidgetTree, new_object children, add_child_to_* parenting, brushes/fonts/slot layout). This
 * library fills the gaps Python's set_editor_property can't reach and provides a HEADLESS
 * compile/save/delete path so batch widget authoring never trips a modal dialog (modal prompts
 * block the MCP bridge's game thread and can crash the editor).
 *
 * Auto-reflects to Python as unreal.UMGAuthoringLibrary.<snake_case_name>.
 */
UCLASS()
class UUMGAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Assign the editor WidgetTree's RootWidget and mark the Blueprint modified so a
	 * subsequent compile serializes the hierarchy. (For the fully-headless path prefer
	 * FinalizeWidgetBlueprint, which reparents + compiles + saves in one dialog-free call.)
	 * @return true if the root was assigned.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|UMG")
	static bool SetWidgetTreeRoot(UWidgetBlueprint* WidgetBlueprint, UWidget* RootWidget);

	/**
	 * Construct a widget parented INTO the Blueprint's WidgetTree (Outer == the tree, clean name)
	 * via ConstructWidget, optionally exposing it as a member variable.
	 *
	 * Does NOT mark/compile the Blueprint — marking here triggers a synchronous recompile that
	 * reparents the fresh widget to /Engine/Transient with a TRASH_ prefix. Construction only.
	 * @return The constructed widget (GetOuter() == WidgetTree, no TRASH_ prefix), or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|UMG")
	static UWidget* CreateWidgetInTree(UWidgetBlueprint* WidgetBlueprint,
		TSubclassOf<UWidget> WidgetClass,
		FName WidgetName,
		bool bIsVariable);

	/**
	 * Set ONLY the "Is Variable" flag on an already-constructed widget. No reconstruction, no
	 * reparenting, no compile — those side-effects corrupt tree persistence.
	 * @return true if the flag was set.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|UMG")
	static bool SetWidgetIsVariable(UWidget* Widget, bool bIsVariable);

	/**
	 * Headless finalize: reparent Root into the tree, set it as RootWidget, mark modified,
	 * compile, and SAVE the package — all with modal dialogs suppressed
	 * (GIsRunningUnattendedScript). The churn-killer for unattended batch authoring.
	 * @return true if the package saved.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|UMG")
	static bool FinalizeWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint, UWidget* RootWidget);

	/**
	 * Instantiate a UserWidget the proper way (runs Initialize) so it renders and can be
	 * add_to_viewport()'d for screenshot verification. Mirrors UWidgetBlueprintLibrary::Create.
	 * Pass a live world context object (e.g. a PIE actor) and optionally an owning controller.
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|UMG", meta = (WorldContext = "WorldContextObject"))
	static UUserWidget* CreateWidgetInstance(UObject* WorldContextObject,
		TSubclassOf<UUserWidget> WidgetClass,
		APlayerController* OwningController);

	/**
	 * Delete an asset headlessly (no "asset is referenced" / confirmation prompts). Accepts a
	 * package path or an Object path (the .Object suffix is stripped).
	 * @return true if the asset no longer exists (already-absent counts as success).
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|UMG")
	static bool DeleteAssetHeadless(const FString& PackageOrObjectPath);
};
