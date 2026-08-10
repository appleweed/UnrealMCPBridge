// Copyright Omar Abdelwahed 2026. All Rights Reserved.

#include "UMGAuthoringLibrary.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

bool UUMGAuthoringLibrary::SetWidgetTreeRoot(UWidgetBlueprint* WidgetBlueprint, UWidget* RootWidget)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		UE_LOG(LogTemp, Error, TEXT("[UMGAuthoring] SetWidgetTreeRoot: null WidgetBlueprint or WidgetTree"));
		return false;
	}

	UWidgetTree* Tree = WidgetBlueprint->WidgetTree;
	Tree->Modify();
	Tree->RootWidget = RootWidget;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	UE_LOG(LogTemp, Display, TEXT("[UMGAuthoring] SetWidgetTreeRoot: %s -> root %s"),
		*WidgetBlueprint->GetName(),
		RootWidget ? *RootWidget->GetName() : TEXT("None"));
	return true;
}

UWidget* UUMGAuthoringLibrary::CreateWidgetInTree(UWidgetBlueprint* WidgetBlueprint,
	TSubclassOf<UWidget> WidgetClass, FName WidgetName, bool bIsVariable)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[UMGAuthoring] CreateWidgetInTree: null WidgetBlueprint, WidgetTree, or WidgetClass"));
		return nullptr;
	}

	// Outer == the WidgetTree, clean deterministic name. Do NOT mark/compile the Blueprint here:
	// a synchronous recompile reparents this fresh widget to /Engine/Transient with a TRASH_ prefix.
	UWidget* W = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, WidgetName);
	if (W)
	{
		W->bIsVariable = bIsVariable;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[UMGAuthoring] CreateWidgetInTree: ConstructWidget failed for %s"),
			*WidgetClass->GetName());
	}
	return W;
}

bool UUMGAuthoringLibrary::SetWidgetIsVariable(UWidget* Widget, bool bIsVariable)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Error, TEXT("[UMGAuthoring] SetWidgetIsVariable: null Widget"));
		return false;
	}

	// Flag only — no reparent, no compile. Those side-effects corrupt WidgetTree persistence.
	Widget->bIsVariable = bIsVariable;
	return true;
}

bool UUMGAuthoringLibrary::FinalizeWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint, UWidget* RootWidget)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !RootWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[UMGAuthoring] FinalizeWidgetBlueprint: null WidgetBlueprint, WidgetTree, or RootWidget"));
		return false;
	}

	// Suppress every modal prompt (compile results, save, "referenced?"). Modal dialogs on the
	// game thread block the MCP bridge and can crash the editor. GIsRunningUnattendedScript makes
	// FMessageDialog return its default answer without showing UI.
	TGuardValue<bool> UnattendedGuard(GIsRunningUnattendedScript, true);

	UWidgetTree* Tree = WidgetBlueprint->WidgetTree;

	// Root must live under the tree (child widgets get auto-reparented by AddChild; the root does not).
	if (RootWidget->GetOuter() != Tree)
	{
		RootWidget->Rename(nullptr, Tree, REN_DontCreateRedirectors | REN_NonTransactional);
	}

	Tree->Modify();
	Tree->RootWidget = RootWidget;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);

	UPackage* Package = WidgetBlueprint->GetOutermost();
	Package->MarkPackageDirty();
	const FString FileName = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const bool bSaved = UPackage::SavePackage(Package, WidgetBlueprint, *FileName, SaveArgs);

	UE_LOG(LogTemp, Display, TEXT("[UMGAuthoring] FinalizeWidgetBlueprint: %s root=%s saved=%s"),
		*WidgetBlueprint->GetName(), *RootWidget->GetName(), bSaved ? TEXT("true") : TEXT("false"));
	return bSaved;
}

UUserWidget* UUMGAuthoringLibrary::CreateWidgetInstance(UObject* WorldContextObject,
	TSubclassOf<UUserWidget> WidgetClass, APlayerController* OwningController)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || !WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[UMGAuthoring] CreateWidgetInstance: null World or WidgetClass"));
		return nullptr;
	}

	return OwningController ? CreateWidget<UUserWidget>(OwningController, WidgetClass)
		: CreateWidget<UUserWidget>(World, WidgetClass);
}

bool UUMGAuthoringLibrary::DeleteAssetHeadless(const FString& PackageOrObjectPath)
{
	TGuardValue<bool> UnattendedGuard(GIsRunningUnattendedScript, true);

	// Accept either a package path or an Object path ("/Game/.../WBP.WBP") — strip the .Object suffix.
	FString P = PackageOrObjectPath;
	int32 Dot;
	if (P.FindChar('.', Dot))
	{
		P = P.Left(Dot);
	}

	if (!UEditorAssetLibrary::DoesAssetExist(P))
	{
		return true;
	}
	return UEditorAssetLibrary::DeleteAsset(P);
}
