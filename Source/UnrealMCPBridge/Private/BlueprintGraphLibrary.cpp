// Copyright Omar Abdelwahed 2026. All Rights Reserved.

#include "BlueprintGraphLibrary.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MakeArray.h"
#include "Kismet/KismetMathLibrary.h"

#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintFunctionNodeSpawner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

// UE 5.5 PerformAction takes FVector2D; UE 5.6+ uses FVector2f
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
using FGraphNodePosition = FVector2f;
#else
using FGraphNodePosition = FVector2D;
#endif

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

UEdGraph* UBlueprintGraphLibrary::GetEventGraph(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph && Blueprint->UbergraphPages.Num() > 0)
	{
		EventGraph = Blueprint->UbergraphPages[0];
	}
	return EventGraph;
}

UEdGraphNode* UBlueprintGraphLibrary::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
	if (!Graph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->GetName() == NodeId)
		{
			return Node;
		}
	}
	return nullptr;
}

UEdGraph* UBlueprintGraphLibrary::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}
	return nullptr;
}

UEdGraphNode* UBlueprintGraphLibrary::FindNodeByIdAcrossGraphs(UBlueprint* Blueprint, const FString& NodeId)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	auto Search = [&NodeId](const TArray<UEdGraph*>& Graphs) -> UEdGraphNode*
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph) continue;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->GetName() == NodeId)
				{
					return Node;
				}
			}
		}
		return nullptr;
	};

	if (UEdGraphNode* Found = Search(Blueprint->UbergraphPages)) return Found;
	if (UEdGraphNode* Found = Search(Blueprint->FunctionGraphs)) return Found;
	if (UEdGraphNode* Found = Search(Blueprint->MacroGraphs)) return Found;
	return nullptr;
}

UEdGraph* UBlueprintGraphLibrary::ResolveGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (GraphName.IsEmpty())
	{
		return GetEventGraph(Blueprint);
	}
	return FindGraphByName(Blueprint, GraphName);
}

UEdGraphNode* UBlueprintGraphLibrary::FindNodeInGraphOrAcross(UBlueprint* Blueprint, const FString& GraphName, const FString& NodeId)
{
	// GraphName given -> scope the lookup to that graph (disambiguates node-id
	// collisions between the event graph and function/macro graphs). Otherwise
	// search every graph so function-graph nodes resolve without a name.
	if (!GraphName.IsEmpty())
	{
		UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
		return Graph ? FindNodeById(Graph, NodeId) : nullptr;
	}
	return FindNodeByIdAcrossGraphs(Blueprint, NodeId);
}

bool UBlueprintGraphLibrary::TypeNameToPinType(const FString& TypeName, FEdGraphPinType& OutPinType)
{
	if (TypeName.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (TypeName.Equals(TEXT("int"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (TypeName.Equals(TEXT("float"), ESearchCase::IgnoreCase) ||
			 TypeName.Equals(TEXT("double"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = TEXT("double");
	}
	else if (TypeName.Equals(TEXT("string"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (TypeName.Equals(TEXT("name"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (TypeName.Equals(TEXT("text"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (TypeName.Equals(TEXT("Transform"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (TypeName.StartsWith(TEXT("/")))
	{
		// Class path (e.g., "/Script/Engine.Actor")
		UClass* Class = FindObject<UClass>(nullptr, *TypeName);
		if (!Class)
		{
			Class = LoadObject<UClass>(nullptr, *TypeName);
		}
		if (Class)
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutPinType.PinSubCategoryObject = Class;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] Could not find class: %s"), *TypeName);
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] Unknown type name: %s"), *TypeName);
		return false;
	}

	return true;
}

// =============================================================================
// ASSET CREATION
// =============================================================================

UBlueprint* UBlueprintGraphLibrary::CreateNewBlueprint(
	const FString& Path,
	const FString& Name,
	UClass* ParentClass)
{
	if (!ParentClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] CreateNewBlueprint: ParentClass is null"));
		return nullptr;
	}

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] CreateNewBlueprint: Failed to create package at %s"), *PackagePath);
		return nullptr;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*Name),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None
	);

	if (Blueprint)
	{
		FAssetRegistryModule::AssetCreated(Blueprint);
		Blueprint->MarkPackageDirty();
		UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Created Blueprint: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] CreateNewBlueprint: FKismetEditorUtilities::CreateBlueprint failed for %s"), *PackagePath);
	}

	return Blueprint;
}

// =============================================================================
// NODE CREATION
// =============================================================================

FString UBlueprintGraphLibrary::AddEventNode(
	UBlueprint* Blueprint,
	const FString& EventName,
	float PosX,
	float PosY)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddEventNode: Blueprint is null"));
		return FString();
	}

	UEdGraph* EventGraph = GetEventGraph(Blueprint);
	if (!EventGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddEventNode: No event graph found"));
		return FString();
	}

	UK2Node_Event* EventNode = NewObject<UK2Node_Event>(EventGraph);
	EventNode->EventReference.SetExternalMember(FName(*EventName), Blueprint->ParentClass);

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = EventNode;
	UEdGraphPin* NullPin = nullptr;
	// PerformAction duplicates the template and adds the copy to the graph.
	// The returned node is the actual graph node with pins allocated.
	UEdGraphNode* CreatedNode = Action.PerformAction(EventGraph, NullPin, FGraphNodePosition(PosX, PosY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added event node: %s -> %s"), *EventName, *CreatedNode->GetName());
	return CreatedNode->GetName();
}

FString UBlueprintGraphLibrary::AddCallFunctionNode(
	UBlueprint* Blueprint,
	UClass* TargetClass,
	const FString& FunctionName,
	float PosX,
	float PosY,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCallFunctionNode: Blueprint is null"));
		return FString();
	}
	if (!TargetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCallFunctionNode: TargetClass is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCallFunctionNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	UFunction* Function = TargetClass->FindFunctionByName(FName(*FunctionName));
	if (!Function)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCallFunctionNode: Function '%s' not found on class '%s'"),
			*FunctionName, *TargetClass->GetName());
		return FString();
	}

	// Method A: UBlueprintFunctionNodeSpawner (recommended for function calls)
	UBlueprintFunctionNodeSpawner* Spawner = UBlueprintFunctionNodeSpawner::Create(Function);
	if (!Spawner)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCallFunctionNode: Failed to create node spawner"));
		return FString();
	}

	UEdGraphNode* Node = Spawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(PosX, PosY));
	if (!Node)
	{
		// Fallback: Method C (direct creation)
		UE_LOG(LogTemp, Warning, TEXT("[BlueprintGraphLib] Spawner->Invoke failed, using direct creation fallback"));

		UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(Graph);
		FuncNode->FunctionReference.SetExternalMember(FName(*FunctionName), TargetClass);
		Graph->AddNode(FuncNode, true, false);
		FuncNode->AllocateDefaultPins();
		FuncNode->NodePosX = static_cast<int32>(PosX);
		FuncNode->NodePosY = static_cast<int32>(PosY);
		Node = FuncNode;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added function call node: %s::%s -> %s"),
		*TargetClass->GetName(), *FunctionName, *Node->GetName());
	return Node->GetName();
}

FString UBlueprintGraphLibrary::AddVariableGetNode(
	UBlueprint* Blueprint,
	const FString& VariableName,
	float PosX,
	float PosY,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableGetNode: Blueprint is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableGetNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(Graph);
	Node->VariableReference.SetSelfMember(FName(*VariableName));

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = Node;
	UEdGraphPin* NullPin = nullptr;
	// PerformAction duplicates the template into the graph and renames the duplicate
	// if the template's name collides with an existing node. Return the inserted
	// node's name (post-rename), not the template's pre-insert name.
	UEdGraphNode* CreatedNode = Action.PerformAction(Graph, NullPin, FGraphNodePosition(PosX, PosY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	const FString FinalName = CreatedNode ? CreatedNode->GetName() : Node->GetName();
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added variable get node: %s -> %s"), *VariableName, *FinalName);
	return FinalName;
}

FString UBlueprintGraphLibrary::AddVariableSetNode(
	UBlueprint* Blueprint,
	const FString& VariableName,
	float PosX,
	float PosY,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableSetNode: Blueprint is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableSetNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	UK2Node_VariableSet* Node = NewObject<UK2Node_VariableSet>(Graph);
	Node->VariableReference.SetSelfMember(FName(*VariableName));

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = Node;
	UEdGraphPin* NullPin = nullptr;
	UEdGraphNode* CreatedNode = Action.PerformAction(Graph, NullPin, FGraphNodePosition(PosX, PosY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	const FString FinalName = CreatedNode ? CreatedNode->GetName() : Node->GetName();
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added variable set node: %s -> %s"), *VariableName, *FinalName);
	return FinalName;
}

FString UBlueprintGraphLibrary::AddBranchNode(
	UBlueprint* Blueprint,
	float PosX,
	float PosY,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddBranchNode: Blueprint is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddBranchNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	UK2Node_IfThenElse* Node = NewObject<UK2Node_IfThenElse>(Graph);

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = Node;
	UEdGraphPin* NullPin = nullptr;
	UEdGraphNode* CreatedNode = Action.PerformAction(Graph, NullPin, FGraphNodePosition(PosX, PosY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	const FString FinalName = CreatedNode ? CreatedNode->GetName() : Node->GetName();
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added branch node -> %s"), *FinalName);
	return FinalName;
}

FString UBlueprintGraphLibrary::AddCastNode(
	UBlueprint* Blueprint,
	UClass* TargetClass,
	float PosX,
	float PosY,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCastNode: Blueprint is null"));
		return FString();
	}
	if (!TargetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCastNode: TargetClass is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCastNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	// Impure dynamic cast (default): exec in/out + Cast Failed exec, "Object" input,
	// "As<ClassName>" output. TargetType must be set before AllocateDefaultPins so the
	// typed output pin is generated.
	UK2Node_DynamicCast* Node = NewObject<UK2Node_DynamicCast>(Graph);
	Node->TargetType = TargetClass;

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = Node;
	UEdGraphPin* NullPin = nullptr;
	UEdGraphNode* CreatedNode = Action.PerformAction(Graph, NullPin, FGraphNodePosition(PosX, PosY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	const FString FinalName = CreatedNode ? CreatedNode->GetName() : Node->GetName();
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added cast node -> %s (cast to %s)"),
		*FinalName, *TargetClass->GetName());
	return FinalName;
}

FString UBlueprintGraphLibrary::AddMakeArrayNode(
	UBlueprint* Blueprint,
	int32 NumInputs,
	float PosX,
	float PosY,
	const FString& GraphName,
	const FString& ElementType)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddMakeArrayNode: Blueprint is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddMakeArrayNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	UK2Node_MakeArray* Node = NewObject<UK2Node_MakeArray>(Graph);
	Node->NumInputs = FMath::Max(1, NumInputs);

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = Node;
	UEdGraphPin* NullPin = nullptr;
	UEdGraphNode* CreatedNode = Action.PerformAction(Graph, NullPin, FGraphNodePosition(PosX, PosY));

	// PerformAction allocates default pins from the template's NumInputs, but a
	// duplicated template can come back with the default single pin - force the
	// pin set to match the requested count.
	UK2Node_MakeArray* MadeNode = Cast<UK2Node_MakeArray>(CreatedNode);
	if (MadeNode)
	{
		if (MadeNode->NumInputs != FMath::Max(1, NumInputs))
		{
			MadeNode->NumInputs = FMath::Max(1, NumInputs);
			MadeNode->ReconstructNode();
		}
	}

	// Resolve the element type up front when asked. Otherwise the pins stay wildcards
	// and the FIRST connection resolves them - which reconstructs the node and throws
	// away any defaults already set on the element pins.
	if (MadeNode && !ElementType.IsEmpty())
	{
		FEdGraphPinType ResolvedType;
		if (TypeNameToPinType(ElementType, ResolvedType))
		{
			// PropagatePinType() is protected, so apply the types directly: the output
			// pin becomes an array of T, every input pin becomes a plain T.
			for (UEdGraphPin* Pin : MadeNode->Pins)
			{
				if (!Pin) continue;
				Pin->PinType = ResolvedType;
				Pin->PinType.ContainerType = (Pin->Direction == EGPD_Output)
					? EPinContainerType::Array
					: EPinContainerType::None;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[BlueprintGraphLib] AddMakeArrayNode: unknown ElementType '%s' - pins left as wildcard"),
				*ElementType);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	const FString FinalName = CreatedNode ? CreatedNode->GetName() : Node->GetName();
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added make-array node -> %s (%d inputs)"),
		*FinalName, FMath::Max(1, NumInputs));
	return FinalName;
}

FString UBlueprintGraphLibrary::AddVariableGetNodeForClass(
	UBlueprint* Blueprint,
	const FString& VariableName,
	UClass* OwnerClass,
	float PosX,
	float PosY,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableGetNodeForClass: Blueprint is null"));
		return FString();
	}
	if (!OwnerClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableGetNodeForClass: OwnerClass is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableGetNodeForClass: Graph '%s' not found"), *GraphName);
		return FString();
	}

	if (!FindFProperty<FProperty>(OwnerClass, FName(*VariableName)))
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariableGetNodeForClass: '%s' not found on %s"),
			*VariableName, *OwnerClass->GetName());
		return FString();
	}

	// SetExternalMember (rather than SetSelfMember) is what produces the target pin.
	UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(Graph);
	Node->VariableReference.SetExternalMember(FName(*VariableName), OwnerClass);

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = Node;
	UEdGraphPin* NullPin = nullptr;
	UEdGraphNode* CreatedNode = Action.PerformAction(Graph, NullPin, FGraphNodePosition(PosX, PosY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	const FString FinalName = CreatedNode ? CreatedNode->GetName() : Node->GetName();
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added external variable get '%s' on %s -> %s"),
		*VariableName, *OwnerClass->GetName(), *FinalName);
	return FinalName;
}

FString UBlueprintGraphLibrary::AddCustomEventNode(
	UBlueprint* Blueprint,
	const FString& EventName,
	float PosX,
	float PosY)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCustomEventNode: Blueprint is null"));
		return FString();
	}

	UEdGraph* EventGraph = GetEventGraph(Blueprint);
	if (!EventGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddCustomEventNode: No event graph found"));
		return FString();
	}

	UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(EventGraph);
	Node->CustomFunctionName = FName(*EventName);

	FEdGraphSchemaAction_K2NewNode Action;
	Action.NodeTemplate = Node;
	UEdGraphPin* NullPin = nullptr;
	UEdGraphNode* CreatedNode = Action.PerformAction(EventGraph, NullPin, FGraphNodePosition(PosX, PosY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added custom event node: %s -> %s"), *EventName, *CreatedNode->GetName());
	return CreatedNode->GetName();
}

FString UBlueprintGraphLibrary::AddSpawnActorNode(
	UBlueprint* Blueprint,
	UClass* ActorClass,
	float PosX,
	float PosY,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddSpawnActorNode: Blueprint is null"));
		return FString();
	}
	if (!ActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddSpawnActorNode: ActorClass is null"));
		return FString();
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddSpawnActorNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	// Manual creation order — DO NOT use FGraphNodeCreator here.
	// K2Node_SpawnActorFromClass::PostPlacedNewNode references the ScaleMethod pin
	// via FindPinChecked, but FGraphNodeCreator::Finalize calls PostPlacedNewNode
	// BEFORE AllocateDefaultPins, so the pin doesn't exist yet and the check fires.
	// We allocate pins first, then call PostPlacedNewNode ourselves.
	UK2Node_SpawnActorFromClass* Node = NewObject<UK2Node_SpawnActorFromClass>(Graph);
	Node->CreateNewGuid();
	Node->NodePosX = static_cast<int32>(PosX);
	Node->NodePosY = static_cast<int32>(PosY);
	Graph->AddNode(Node, /*bSelectNewNode*/ true, /*bFromUI*/ false);
	Node->AllocateDefaultPins();
	Node->PostPlacedNewNode();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Ask the schema to generate defaults for every pin where it can.
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 0)
		{
			Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
		}
	}

	// SpawnTransform is a by-ref struct pin. The K2 compile-time expansion of this
	// node moves only WIRES (not literal DefaultValue strings) into the expanded
	// BeginDeferred/FinishSpawning calls, so an unconnected pin always trips the
	// "by-ref param must have an input wired" check. Auto-spawn the canonical
	// "Make Transform" node (KismetMathLibrary::MakeTransform call) and wire its
	// ReturnValue into SpawnTransform.
	//
	// Note: FTransform isn't a BlueprintType-flagged struct, so a raw K2Node_MakeStruct
	// errors with "The structure Make Transform is not a BlueprintType". We must use
	// the canonical KismetMathLibrary::MakeTransform UFunction instead.
	if (UEdGraphPin* TransformPin = Node->FindPin(TEXT("SpawnTransform")))
	{
		if (TransformPin->LinkedTo.Num() == 0)
		{
			static const FName MakeTransformFuncName(TEXT("MakeTransform"));
			UFunction* MakeTransformFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(MakeTransformFuncName);
			if (MakeTransformFunc)
			{
				UK2Node_CallFunction* MakeXfNode = NewObject<UK2Node_CallFunction>(Graph);
				MakeXfNode->CreateNewGuid();
				MakeXfNode->FunctionReference.SetExternalMember(MakeTransformFuncName, UKismetMathLibrary::StaticClass());
				MakeXfNode->NodePosX = Node->NodePosX - 250;
				MakeXfNode->NodePosY = Node->NodePosY + 50;
				Graph->AddNode(MakeXfNode, /*bSelectNewNode*/ false, /*bFromUI*/ false);
				MakeXfNode->AllocateDefaultPins();
				MakeXfNode->PostPlacedNewNode();

				if (UEdGraphPin* MakeReturnPin = MakeXfNode->GetReturnValuePin())
				{
					Schema->TryCreateConnection(MakeReturnPin, TransformPin);
				}
			}
		}
	}

	// Pin the class via the schema so PinDefaultValueChanged fires on the K2 node;
	// that callback materializes the dynamic ExposeOnSpawn property pins via ReconstructNode.
	if (UEdGraphPin* ClassPin = Node->GetClassPin())
	{
		Schema->TrySetDefaultObject(*ClassPin, ActorClass);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added SpawnActor node for %s -> %s"),
		*ActorClass->GetName(), *Node->GetName());
	return Node->GetName();
}

// =============================================================================
// PIN OPERATIONS
// =============================================================================

bool UBlueprintGraphLibrary::ConnectPins(
	UBlueprint* Blueprint,
	const FString& SourceNodeId,
	const FString& SourcePinName,
	const FString& TargetNodeId,
	const FString& TargetPinName,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] ConnectPins: Blueprint is null"));
		return false;
	}

	UEdGraphNode* SourceNode = FindNodeInGraphOrAcross(Blueprint, GraphName, SourceNodeId);
	if (!SourceNode)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] ConnectPins: Source node '%s' not found"), *SourceNodeId);
		return false;
	}

	UEdGraphNode* TargetNode = FindNodeInGraphOrAcross(Blueprint, GraphName, TargetNodeId);
	if (!TargetNode)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] ConnectPins: Target node '%s' not found"), *TargetNodeId);
		return false;
	}

	UEdGraphPin* SourcePin = SourceNode->FindPin(FName(*SourcePinName));
	if (!SourcePin)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] ConnectPins: Pin '%s' not found on node '%s'"),
			*SourcePinName, *SourceNodeId);
		return false;
	}

	UEdGraphPin* TargetPin = TargetNode->FindPin(FName(*TargetPinName));
	if (!TargetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] ConnectPins: Pin '%s' not found on node '%s'"),
			*TargetPinName, *TargetNodeId);
		return false;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	bool bResult = Schema->TryCreateConnection(SourcePin, TargetPin);

	if (bResult)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Connected %s.%s -> %s.%s"),
			*SourceNodeId, *SourcePinName, *TargetNodeId, *TargetPinName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BlueprintGraphLib] ConnectPins: TryCreateConnection failed for %s.%s -> %s.%s"),
			*SourceNodeId, *SourcePinName, *TargetNodeId, *TargetPinName);
	}

	return bResult;
}

bool UBlueprintGraphLibrary::SetPinDefaultValue(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& PinName,
	const FString& Value,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] SetPinDefaultValue: Blueprint is null"));
		return false;
	}

	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] SetPinDefaultValue: Node '%s' not found"), *NodeId);
		return false;
	}

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] SetPinDefaultValue: Pin '%s' not found on node '%s'"),
			*PinName, *NodeId);
		return false;
	}

	// A wildcard pin cannot hold a default: resolving its type reconstructs the node
	// and discards the value. Setting one used to "succeed" and then silently vanish,
	// so refuse it outright rather than reporting a write that won't survive.
	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[BlueprintGraphLib] SetPinDefaultValue: pin '%s' on '%s' is a WILDCARD - its type must be "
			     "resolved first or the default is discarded. Connect the node's output to a typed pin, or "
			     "create the node with an explicit element type (see AddMakeArrayNode's ElementType)."),
			*PinName, *NodeId);
		return false;
	}

	// Class and object pins use DefaultObject, not DefaultValue
	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class ||
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass ||
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *Value);
		if (Obj)
		{
			Pin->DefaultObject = Obj;
			UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Set pin default object: %s.%s = %s"), *NodeId, *PinName, *Value);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] SetPinDefaultValue: Could not load object '%s'"), *Value);
			return false;
		}
	}
	else
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		Schema->TrySetDefaultValue(*Pin, Value);
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Set pin default: %s.%s = %s"), *NodeId, *PinName, *Value);
	return true;
}

bool UBlueprintGraphLibrary::BreakPinLinks(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& PinName,
	const FString& GraphName,
	const FString& TargetNodeId,
	const FString& TargetPinName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] BreakPinLinks: Blueprint is null"));
		return false;
	}

	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] BreakPinLinks: Node '%s' not found"), *NodeId);
		return false;
	}

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] BreakPinLinks: Pin '%s' not found on '%s'"), *PinName, *NodeId);
		return false;
	}

	const bool bSpecificTarget = !TargetNodeId.IsEmpty();
	bool bBrokeAny = false;

	if (!bSpecificTarget)
	{
		bBrokeAny = Pin->LinkedTo.Num() > 0;
		Pin->BreakAllPinLinks();
	}
	else
	{
		// Copy: BreakSinglePinLink mutates LinkedTo while we iterate.
		TArray<UEdGraphPin*> Links = Pin->LinkedTo;
		for (UEdGraphPin* Linked : Links)
		{
			if (!Linked || !Linked->GetOwningNode()) continue;
			if (Linked->GetOwningNode()->GetName() != TargetNodeId) continue;
			if (!TargetPinName.IsEmpty() && Linked->GetName() != TargetPinName) continue;

			Pin->BreakLinkTo(Linked);
			bBrokeAny = true;
		}
	}

	if (bBrokeAny)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Broke links on %s.%s"), *NodeId, *PinName);
	}
	return bBrokeAny;
}

bool UBlueprintGraphLibrary::RemoveNode(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] RemoveNode: Blueprint is null"));
		return false;
	}

	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] RemoveNode: Node '%s' not found"), *NodeId);
		return false;
	}

	if (!Node->CanUserDeleteNode())
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] RemoveNode: Node '%s' cannot be deleted (entry/result node?)"), *NodeId);
		return false;
	}

	Node->BreakAllNodeLinks();

	UEdGraph* OwningGraph = Node->GetGraph();
	if (!OwningGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] RemoveNode: Node '%s' has no owning graph"), *NodeId);
		return false;
	}

	OwningGraph->RemoveNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Removed node %s"), *NodeId);
	return true;
}

bool UBlueprintGraphLibrary::ReconstructNode(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] ReconstructNode: Blueprint is null"));
		return false;
	}

	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] ReconstructNode: Node '%s' not found"), *NodeId);
		return false;
	}

	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Reconstructed node %s"), *NodeId);
	return true;
}

// =============================================================================
// FUNCTION GRAPHS
// =============================================================================

bool UBlueprintGraphLibrary::AddFunctionGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionGraph: Blueprint is null"));
		return false;
	}
	if (FindGraphByName(Blueprint, GraphName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BlueprintGraphLib] AddFunctionGraph: Graph '%s' already exists"), *GraphName);
		return false;
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*GraphName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionGraph: CreateNewGraph failed"));
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph<UClass>(
		Blueprint, NewGraph, /*bIsUserCreated*/ true, /*SignatureFromObject*/ nullptr);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added function graph: %s"), *GraphName);
	return true;
}

bool UBlueprintGraphLibrary::AddFunctionParameter(
	UBlueprint* Blueprint,
	const FString& FunctionGraphName,
	const FString& ParamName,
	const FString& TypeName,
	bool bIsInput,
	const FString& DefaultValue)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionParameter: Blueprint is null"));
		return false;
	}
	UEdGraph* Graph = FindGraphByName(Blueprint, FunctionGraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionParameter: Graph '%s' not found"), *FunctionGraphName);
		return false;
	}

	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_FunctionResult* Result = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (auto* E = Cast<UK2Node_FunctionEntry>(Node))      Entry = E;
		else if (auto* R = Cast<UK2Node_FunctionResult>(Node)) Result = R;
	}
	if (!Entry)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionParameter: No FunctionEntry in graph '%s'"), *FunctionGraphName);
		return false;
	}

	FEdGraphPinType PinType;
	if (!TypeNameToPinType(TypeName, PinType))
	{
		return false;
	}

	if (bIsInput)
	{
		// Function inputs are output pins on the FunctionEntry node
		UEdGraphPin* NewPin = Entry->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output, /*bUseUniqueName*/ true);
		if (!NewPin)
		{
			UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionParameter: CreateUserDefinedPin failed for input '%s'"), *ParamName);
			return false;
		}
		if (!DefaultValue.IsEmpty())
		{
			NewPin->DefaultValue = DefaultValue;
			NewPin->AutogeneratedDefaultValue = DefaultValue;
		}
	}
	else
	{
		// Function outputs are input pins on the FunctionResult node — create one if missing
		if (!Result)
		{
			FGraphNodeCreator<UK2Node_FunctionResult> Creator(*Graph);
			Result = Creator.CreateNode();
			Result->FunctionReference.SetSelfMember(Graph->GetFName());
			Result->NodePosX = Entry->NodePosX + 400;
			Result->NodePosY = Entry->NodePosY;
			Creator.Finalize();
		}
		UEdGraphPin* NewPin = Result->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Input, /*bUseUniqueName*/ true);
		if (!NewPin)
		{
			UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionParameter: CreateUserDefinedPin failed for output '%s'"), *ParamName);
			return false;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added function parameter: %s::%s (%s, %s)"),
		*FunctionGraphName, *ParamName, *TypeName, bIsInput ? TEXT("input") : TEXT("output"));
	return true;
}

// =============================================================================
// VARIABLES
// =============================================================================

FString UBlueprintGraphLibrary::AddFunctionOverride(
	UBlueprint* Blueprint,
	const FString& FunctionName)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionOverride: Blueprint is null"));
		return FString();
	}
	if (!Blueprint->ParentClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionOverride: Blueprint has no parent class"));
		return FString();
	}

	const FName FuncName(*FunctionName);
	UFunction* Func = Blueprint->ParentClass->FindFunctionByName(FuncName);
	if (!Func)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionOverride: '%s' not found on parent class %s"),
			*FunctionName, *Blueprint->ParentClass->GetName());
		return FString();
	}

	// Already overridden as a function graph?
	if (UEdGraph* Existing = FindGraphByName(Blueprint, FunctionName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BlueprintGraphLib] AddFunctionOverride: '%s' already overridden"), *FunctionName);
		return Existing->GetName();
	}

	// Events (BlueprintImplementableEvent / BlueprintNativeEvent with no return value)
	// are overridden by placing a K2Node_Event in the event graph, not by creating a
	// function graph. This is the same split the My Blueprint > Override dropdown uses.
	if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Func))
	{
		UEdGraph* EventGraph = GetEventGraph(Blueprint);
		if (!EventGraph)
		{
			UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionOverride: no event graph"));
			return FString();
		}

		// Don't create a duplicate event node.
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				if (EventNode->EventReference.GetMemberName() == FuncName)
				{
					UE_LOG(LogTemp, Warning, TEXT("[BlueprintGraphLib] AddFunctionOverride: event '%s' already present"), *FunctionName);
					return EventNode->GetName();
				}
			}
		}

		UK2Node_Event* NewEvent = NewObject<UK2Node_Event>(EventGraph);
		NewEvent->EventReference.SetExternalMember(FuncName, CastChecked<UClass>(Func->GetOuter()));
		NewEvent->bOverrideFunction = true;

		FEdGraphSchemaAction_K2NewNode Action;
		Action.NodeTemplate = NewEvent;
		UEdGraphPin* NullPin = nullptr;
		UEdGraphNode* CreatedNode = Action.PerformAction(EventGraph, NullPin, FGraphNodePosition(0.f, 0.f));

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		const FString FinalName = CreatedNode ? CreatedNode->GetName() : NewEvent->GetName();
		UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Overrode event '%s' -> %s"), *FunctionName, *FinalName);
		return FinalName;
	}

	// Function override: a new graph carrying the parent's signature.
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FuncName,
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddFunctionOverride: CreateNewGraph failed for '%s'"), *FunctionName);
		return FString();
	}

	// bIsUserCreated=false + the signature class is what makes this an OVERRIDE rather
	// than a new user function that merely shares the name.
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(
		Blueprint,
		NewGraph,
		/*bIsUserCreated=*/false,
		CastChecked<UClass>(Func->GetOuter()));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Overrode function '%s' (graph created)"), *FunctionName);
	return NewGraph->GetName();
}

bool UBlueprintGraphLibrary::AddVariable(
	UBlueprint* Blueprint,
	const FString& Name,
	const FString& TypeName,
	bool bInstanceEditable)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariable: Blueprint is null"));
		return false;
	}

	FEdGraphPinType PinType;
	if (!TypeNameToPinType(TypeName, PinType))
	{
		return false;
	}

	bool bResult = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), PinType);
	if (!bResult)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] AddVariable: Failed to add variable '%s' of type '%s'"),
			*Name, *TypeName);
		return false;
	}

	if (bInstanceEditable)
	{
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, FName(*Name), false);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Added variable: %s (%s, editable=%d)"),
		*Name, *TypeName, bInstanceEditable);
	return true;
}

// =============================================================================
// COMPILE / SAVE
// =============================================================================

bool UBlueprintGraphLibrary::CompileAndSaveBlueprint(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] CompileAndSaveBlueprint: Blueprint is null"));
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	bool bSuccess = (Blueprint->Status != BS_Error);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] CompileAndSaveBlueprint: Compilation failed for %s"),
			*Blueprint->GetName());
		return false;
	}

	UPackage* Package = Blueprint->GetOutermost();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(
		Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Blueprint, *PackageFilename, SaveArgs);
	}

	UE_LOG(LogTemp, Display, TEXT("[BlueprintGraphLib] Compiled and saved: %s"), *Blueprint->GetName());
	return true;
}

// =============================================================================
// INTROSPECTION
// =============================================================================

TArray<FString> UBlueprintGraphLibrary::GetNodeIds(UBlueprint* Blueprint, const FString& GraphName)
{
	TArray<FString> Result;

	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetNodeIds: Blueprint is null"));
		return Result;
	}

	UEdGraph* Graph = ResolveGraph(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetNodeIds: Graph '%s' not found"), *GraphName);
		return Result;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			Result.Add(Node->GetName());
		}
	}

	return Result;
}

TArray<FString> UBlueprintGraphLibrary::GetGraphNames(UBlueprint* Blueprint)
{
	TArray<FString> Result;
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetGraphNames: Blueprint is null"));
		return Result;
	}

	auto Collect = [&Result](const TArray<UEdGraph*>& Graphs, const TCHAR* Kind)
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph)
			{
				Result.Add(FString::Printf(TEXT("%s|%s"), *Graph->GetName(), Kind));
			}
		}
	};

	Collect(Blueprint->UbergraphPages, TEXT("Ubergraph"));
	Collect(Blueprint->FunctionGraphs, TEXT("Function"));
	Collect(Blueprint->MacroGraphs,    TEXT("Macro"));
	return Result;
}

TArray<FString> UBlueprintGraphLibrary::GetPinNames(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& GraphName)
{
	TArray<FString> Result;

	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetPinNames: Blueprint is null"));
		return Result;
	}

	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetPinNames: Node '%s' not found"), *NodeId);
		return Result;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			Result.Add(Pin->GetName());
		}
	}

	return Result;
}

TArray<FString> UBlueprintGraphLibrary::GetPinDetails(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& GraphName)
{
	TArray<FString> Result;

	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetPinDetails: Blueprint is null"));
		return Result;
	}

	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetPinDetails: Node '%s' not found"), *NodeId);
		return Result;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			FString Direction = (Pin->Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
			FString Category = Pin->PinType.PinCategory.ToString();
			Result.Add(FString::Printf(TEXT("%s|%s|%s"), *Pin->GetName(), *Direction, *Category));
		}
	}

	return Result;
}

FString UBlueprintGraphLibrary::GetNodeTitle(UBlueprint* Blueprint, const FString& NodeId, const FString& GraphName)
{
	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		return FString();
	}
	return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
}

FString UBlueprintGraphLibrary::GetFunctionReference(UBlueprint* Blueprint, const FString& NodeId)
{
	UEdGraphNode* Node = FindNodeByIdAcrossGraphs(Blueprint, NodeId);
	if (!Node)
	{
		return FString();
	}

	UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
	if (!CallNode)
	{
		return FString();
	}

	const FMemberReference& Ref = CallNode->FunctionReference;
	FString MemberName = Ref.GetMemberName().ToString();
	FString ParentName;
	if (UClass* ParentClass = Ref.GetMemberParentClass())
	{
		ParentName = ParentClass->GetName();
	}
	else if (Ref.IsSelfContext() && Blueprint && Blueprint->GeneratedClass)
	{
		ParentName = Blueprint->GeneratedClass->GetName();
	}

	if (ParentName.IsEmpty())
	{
		return MemberName;
	}
	return FString::Printf(TEXT("%s::%s"), *ParentName, *MemberName);
}

FString UBlueprintGraphLibrary::GetEventReference(UBlueprint* Blueprint, const FString& NodeId)
{
	UEdGraphNode* Node = FindNodeByIdAcrossGraphs(Blueprint, NodeId);
	if (!Node)
	{
		return FString();
	}

	if (UK2Node_CustomEvent* CustomNode = Cast<UK2Node_CustomEvent>(Node))
	{
		return CustomNode->CustomFunctionName.ToString();
	}

	if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		const FMemberReference& Ref = EventNode->EventReference;
		FString MemberName = Ref.GetMemberName().ToString();
		FString ParentName;
		if (UClass* ParentClass = Ref.GetMemberParentClass())
		{
			ParentName = ParentClass->GetName();
		}
		if (ParentName.IsEmpty())
		{
			return MemberName;
		}
		return FString::Printf(TEXT("%s::%s"), *ParentName, *MemberName);
	}

	return FString();
}

FString UBlueprintGraphLibrary::GetVariableReference(UBlueprint* Blueprint, const FString& NodeId)
{
	UEdGraphNode* Node = FindNodeByIdAcrossGraphs(Blueprint, NodeId);
	if (!Node)
	{
		return FString();
	}

	UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node);
	if (!VarNode)
	{
		return FString();
	}

	return VarNode->VariableReference.GetMemberName().ToString();
}

FString UBlueprintGraphLibrary::GetMacroReference(UBlueprint* Blueprint, const FString& NodeId)
{
	UEdGraphNode* Node = FindNodeByIdAcrossGraphs(Blueprint, NodeId);
	if (!Node)
	{
		return FString();
	}

	UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node);
	if (!MacroNode)
	{
		return FString();
	}

	UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
	FString MacroName = MacroGraph ? MacroGraph->GetName() : FString();

	FString LibraryName;
	if (UBlueprint* LibraryBP = MacroNode->GetSourceBlueprint())
	{
		LibraryName = LibraryBP->GetName();
	}

	if (LibraryName.IsEmpty())
	{
		return MacroName;
	}
	return FString::Printf(TEXT("%s::%s"), *LibraryName, *MacroName);
}

TArray<FString> UBlueprintGraphLibrary::GetPinConnections(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& PinName,
	const FString& GraphName)
{
	TArray<FString> Result;

	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetPinConnections: Node '%s' not found"), *NodeId);
		return Result;
	}

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin)
	{
		// Missing pin is a normal "no connections" answer for an introspection call;
		// callers may probe a set of candidate pin names without enumerating first.
		return Result;
	}

	for (UEdGraphPin* Linked : Pin->LinkedTo)
	{
		if (!Linked || !Linked->GetOwningNode()) continue;
		Result.Add(FString::Printf(TEXT("%s.%s"),
			*Linked->GetOwningNode()->GetName(), *Linked->GetName()));
	}

	return Result;
}

FString UBlueprintGraphLibrary::GetPinDefaultValue(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const FString& PinName,
	const FString& GraphName)
{
	UEdGraphNode* Node = FindNodeInGraphOrAcross(Blueprint, GraphName, NodeId);
	if (!Node)
	{
		return FString();
	}

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin)
	{
		return FString();
	}

	const bool bIsObjectLikePin =
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class ||
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass ||
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject;

	if (bIsObjectLikePin && Pin->DefaultObject)
	{
		return Pin->DefaultObject->GetPathName();
	}

	if (!Pin->DefaultValue.IsEmpty())
	{
		return Pin->DefaultValue;
	}
	if (!Pin->AutogeneratedDefaultValue.IsEmpty())
	{
		return Pin->AutogeneratedDefaultValue;
	}
	return FString();
}

TArray<FString> UBlueprintGraphLibrary::GetFunctionGraphNames(UBlueprint* Blueprint)
{
	TArray<FString> Result;
	if (!Blueprint)
	{
		return Result;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			Result.Add(Graph->GetName());
		}
	}
	return Result;
}

TArray<FString> UBlueprintGraphLibrary::GetNodeIdsInGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	TArray<FString> Result;

	UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlueprintGraphLib] GetNodeIdsInGraph: Graph '%s' not found"), *GraphName);
		return Result;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			Result.Add(Node->GetName());
		}
	}
	return Result;
}

TArray<FString> UBlueprintGraphLibrary::GetComponentTemplateNames(UBlueprint* Blueprint)
{
	TArray<FString> Result;
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return Result;
	}

	for (USCS_Node* SCSNode : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (SCSNode)
		{
			Result.Add(SCSNode->GetVariableName().ToString());
		}
	}
	return Result;
}

FString UBlueprintGraphLibrary::GetComponentTemplateClass(UBlueprint* Blueprint, const FString& ComponentName)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return FString();
	}

	const FName Target(*ComponentName);
	for (USCS_Node* SCSNode : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (SCSNode && SCSNode->GetVariableName() == Target)
		{
			if (SCSNode->ComponentTemplate)
			{
				return SCSNode->ComponentTemplate->GetClass()->GetName();
			}
			if (SCSNode->ComponentClass)
			{
				return SCSNode->ComponentClass->GetName();
			}
			return FString();
		}
	}
	return FString();
}

FString UBlueprintGraphLibrary::FormatPinType(const FEdGraphPinType& T)
{
	FString Base;
	if (T.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		Base = T.PinSubCategoryObject.IsValid()
			? FString::Printf(TEXT("struct:%s"), *T.PinSubCategoryObject->GetName())
			: TEXT("struct");
	}
	else if (T.PinCategory == UEdGraphSchema_K2::PC_Object ||
	         T.PinCategory == UEdGraphSchema_K2::PC_Interface ||
	         T.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		Base = T.PinSubCategoryObject.IsValid()
			? FString::Printf(TEXT("%s:%s"), *T.PinCategory.ToString(), *T.PinSubCategoryObject->GetPathName())
			: T.PinCategory.ToString();
	}
	else if (T.PinCategory == UEdGraphSchema_K2::PC_Class ||
	         T.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		Base = T.PinSubCategoryObject.IsValid()
			? FString::Printf(TEXT("%s:%s"), *T.PinCategory.ToString(), *T.PinSubCategoryObject->GetPathName())
			: T.PinCategory.ToString();
	}
	else if (T.PinCategory == UEdGraphSchema_K2::PC_Byte && T.PinSubCategoryObject.IsValid())
	{
		Base = FString::Printf(TEXT("enum:%s"), *T.PinSubCategoryObject->GetName());
	}
	else if (T.PinCategory == UEdGraphSchema_K2::PC_Real && !T.PinSubCategory.IsNone())
	{
		// "double" or "float"
		Base = T.PinSubCategory.ToString();
	}
	else
	{
		Base = T.PinCategory.ToString();
	}

	if (T.IsArray()) return FString::Printf(TEXT("TArray<%s>"), *Base);
	if (T.IsSet())   return FString::Printf(TEXT("TSet<%s>"), *Base);
	if (T.IsMap())
	{
		FString ValueType = T.PinValueType.TerminalCategory.ToString();
		if (T.PinValueType.TerminalSubCategoryObject.IsValid())
		{
			ValueType = FString::Printf(TEXT("%s:%s"), *ValueType, *T.PinValueType.TerminalSubCategoryObject->GetName());
		}
		return FString::Printf(TEXT("TMap<%s,%s>"), *Base, *ValueType);
	}
	return Base;
}

TArray<FString> UBlueprintGraphLibrary::GetMacroGraphNames(UBlueprint* Blueprint)
{
	TArray<FString> Result;
	if (!Blueprint)
	{
		return Result;
	}
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph)
		{
			Result.Add(Graph->GetName());
		}
	}
	return Result;
}

TArray<FString> UBlueprintGraphLibrary::GetMemberVariableNames(UBlueprint* Blueprint)
{
	TArray<FString> Result;
	if (!Blueprint)
	{
		return Result;
	}
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		Result.Add(Var.VarName.ToString());
	}
	return Result;
}

FString UBlueprintGraphLibrary::GetMemberVariableType(UBlueprint* Blueprint, const FString& VariableName)
{
	if (!Blueprint)
	{
		return FString();
	}
	const FName Target(*VariableName);
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == Target)
		{
			return FormatPinType(Var.VarType);
		}
	}
	return FString();
}

FString UBlueprintGraphLibrary::GetFunctionMetadata(UBlueprint* Blueprint, const FString& FunctionGraphName)
{
	UEdGraph* Graph = FindGraphByName(Blueprint, FunctionGraphName);
	if (!Graph)
	{
		return FString();
	}

	UK2Node_FunctionEntry* Entry = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(Node))
		{
			Entry = E;
			break;
		}
	}
	if (!Entry)
	{
		return FString();
	}

	FString Category = Entry->MetaData.Category.ToString();

	const int32 Flags = Entry->GetExtraFlags();
	FString Access = TEXT("Public");
	if (Flags & FUNC_Private)         Access = TEXT("Private");
	else if (Flags & FUNC_Protected)  Access = TEXT("Protected");

	const bool bPure  = (Flags & FUNC_BlueprintPure) != 0;
	const bool bConst = (Flags & FUNC_Const) != 0;

	return FString::Printf(TEXT("%s|%s|%s|%s"),
		*Category, *Access,
		bPure  ? TEXT("Pure")  : TEXT("Impure"),
		bConst ? TEXT("Const") : TEXT("NonConst"));
}

TArray<FString> UBlueprintGraphLibrary::GetFunctionParameters(UBlueprint* Blueprint, const FString& FunctionGraphName)
{
	TArray<FString> Result;
	UEdGraph* Graph = FindGraphByName(Blueprint, FunctionGraphName);
	if (!Graph)
	{
		return Result;
	}

	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_FunctionResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (auto* E = Cast<UK2Node_FunctionEntry>(Node))     Entry = E;
		else if (auto* R = Cast<UK2Node_FunctionResult>(Node)) ResultNode = R;
	}

	const FName ExecPin = UEdGraphSchema_K2::PN_Execute;
	const FName ThenPin = UEdGraphSchema_K2::PN_Then;

	// Inputs flow OUT of the entry node (into the function body), so look at output pins
	if (Entry)
	{
		for (UEdGraphPin* Pin : Entry->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output) continue;
			if (Pin->PinName == ThenPin) continue;
			Result.Add(FString::Printf(TEXT("%s|Input|%s|%s"),
				*Pin->PinName.ToString(),
				*FormatPinType(Pin->PinType),
				*Pin->DefaultValue));
		}
	}

	// Outputs flow INTO the result node, so look at input pins
	if (ResultNode)
	{
		for (UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input) continue;
			if (Pin->PinName == ExecPin) continue;
			Result.Add(FString::Printf(TEXT("%s|Output|%s|"),
				*Pin->PinName.ToString(),
				*FormatPinType(Pin->PinType)));
		}
	}

	return Result;
}

TArray<FString> UBlueprintGraphLibrary::GetOverridableFunctions(UBlueprint* Blueprint)
{
	TArray<FString> Result;
	if (!Blueprint || !Blueprint->ParentClass)
	{
		return Result;
	}

	// Collect already-overridden event names from all event graphs.
	TSet<FName> Overridden;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_Event* E = Cast<UK2Node_Event>(Node))
			{
				Overridden.Add(E->EventReference.GetMemberName());
			}
		}
	}

	for (TFieldIterator<UFunction> It(Blueprint->ParentClass); It; ++It)
	{
		UFunction* Func = *It;
		if (!Func) continue;
		if (Func->HasAnyFunctionFlags(FUNC_BlueprintEvent) &&
		    !Overridden.Contains(Func->GetFName()))
		{
			Result.Add(Func->GetName());
		}
	}

	Result.Sort();
	return Result;
}
