// Copyright Omar Abdelwahed 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintGraphLibrary.generated.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
struct FEdGraphPinType;

/**
 * Blueprint function library for programmatic Blueprint graph manipulation.
 * Wraps C++ graph APIs (AddNode, ConnectPins, AllocateDefaultPins) that are
 * not UFUNCTIONs and therefore not accessible from the Python API.
 *
 * All functions are static and BlueprintCallable, auto-reflecting to Python via:
 *   import unreal
 *   lib = unreal.BlueprintGraphLibrary
 *
 * Node IDs are UEdGraphNode::GetName() strings. Pin names are internal names
 * ("execute", "then", "ReturnValue"), not display names.
 */
UCLASS()
class UBlueprintGraphLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// =========================================================================
	// ASSET CREATION
	// =========================================================================

	/** Create a new Blueprint asset.
	 * @param Path Content path (e.g., "/Game/Blueprints")
	 * @param Name Blueprint name (e.g., "BP_MyActor")
	 * @param ParentClass Parent class (e.g., AActor::StaticClass())
	 * @return The created Blueprint, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Asset")
	static UBlueprint* CreateNewBlueprint(
		const FString& Path,
		const FString& Name,
		UClass* ParentClass);

	// =========================================================================
	// NODE CREATION (returns node ID string for pin connections)
	// =========================================================================

	/** Add an event node (BeginPlay, Tick, etc.).
	 * @param Blueprint Target Blueprint
	 * @param EventName Event function name (e.g., "ReceiveBeginPlay")
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddEventNode(
		UBlueprint* Blueprint,
		const FString& EventName,
		float PosX = 0.f,
		float PosY = 0.f);

	/** Add a function call node.
	 * @param Blueprint Target Blueprint
	 * @param TargetClass Class that owns the function
	 * @param FunctionName Function name (e.g., "PrintString")
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddCallFunctionNode(
		UBlueprint* Blueprint,
		UClass* TargetClass,
		const FString& FunctionName,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""));

	/** Add a variable Get node.
	 * @param Blueprint Target Blueprint
	 * @param VariableName Name of the Blueprint variable
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddVariableGetNode(
		UBlueprint* Blueprint,
		const FString& VariableName,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""));

	/** Add a variable Set node.
	 * @param Blueprint Target Blueprint
	 * @param VariableName Name of the Blueprint variable
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddVariableSetNode(
		UBlueprint* Blueprint,
		const FString& VariableName,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""));

	/** Add a Branch (if/else) node.
	 * @param Blueprint Target Blueprint
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddBranchNode(
		UBlueprint* Blueprint,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""));

	/** Add a Cast (dynamic cast) node to a target class.
	 * The node is impure (has exec + Cast Failed pins). Input pin "Object",
	 * output data pin "As<ClassName>", exec out "then" / "CastFailed".
	 * @param Blueprint Target Blueprint
	 * @param TargetClass Class to cast to (e.g., /Script/Engine.Actor or a BP class /Game/BP/BP_X.BP_X_C)
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @param GraphName Optional graph to add to (empty = event graph)
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddCastNode(
		UBlueprint* Blueprint,
		UClass* TargetClass,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""));

	/** Add a Make Array node.
	 * Input pins are named "[0]", "[1]", ... and the output pin is "Array".
	 *
	 * Pass ElementType to type the pins immediately. Leave it empty and the pins stay
	 * wildcards until the first connection resolves them — and resolving the type
	 * reconstructs the node, which DISCARDS any pin default values set beforehand.
	 * So if you intend to set element defaults (rather than wire them), always pass
	 * ElementType; otherwise the defaults silently vanish.
	 *
	 * @param Blueprint Target Blueprint
	 * @param NumInputs Number of input pins to create (clamped to >= 1)
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @param GraphName Optional graph to add to (empty = event graph)
	 * @param ElementType Optional element type ("name", "int", "float", "string",
	 *        "bool", "Vector", a struct/class path, ...). Empty = wildcard.
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddMakeArrayNode(
		UBlueprint* Blueprint,
		int32 NumInputs = 1,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""),
		const FString& ElementType = TEXT(""));

	/** Add a variable Get node for a variable owned by ANOTHER class.
	 * Unlike AddVariableGetNode (which is self-scoped), this produces a node with a
	 * target pin typed to OwnerClass, so you can read a member off another object
	 * (e.g. a component reference obtained from GetComponentByClass + a cast).
	 * @param Blueprint Target Blueprint
	 * @param VariableName Name of the variable on OwnerClass
	 * @param OwnerClass Class that declares the variable
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @param GraphName Optional graph to add to (empty = event graph)
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddVariableGetNodeForClass(
		UBlueprint* Blueprint,
		const FString& VariableName,
		UClass* OwnerClass,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""));

	/** Add a Custom Event node.
	 * @param Blueprint Target Blueprint
	 * @param EventName Custom event name
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddCustomEventNode(
		UBlueprint* Blueprint,
		const FString& EventName,
		float PosX = 0.f,
		float PosY = 0.f);

	/** Add a SpawnActor node (the canonical UK2Node_SpawnActorFromClass).
	 * Creates the single-node form rather than the BeginDeferredActorSpawnFromClass +
	 * FinishSpawningActor workaround. ReconstructNode is called so that any
	 * ExposeOnSpawn properties on ActorClass appear as input pins.
	 * @param Blueprint Target Blueprint
	 * @param ActorClass The class to spawn (e.g., loaded BP class /Game/BP/BP_NPC.BP_NPC_C)
	 * @param PosX Node X position in graph
	 * @param PosY Node Y position in graph
	 * @return Node ID string, or empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static FString AddSpawnActorNode(
		UBlueprint* Blueprint,
		UClass* ActorClass,
		float PosX = 0.f,
		float PosY = 0.f,
		const FString& GraphName = TEXT(""));

	// =========================================================================
	// PIN OPERATIONS
	// =========================================================================

	/** Connect two pins between nodes.
	 * @param Blueprint Target Blueprint
	 * @param SourceNodeId Source node ID (from Add*Node return value)
	 * @param SourcePinName Source pin internal name (e.g., "then")
	 * @param TargetNodeId Target node ID
	 * @param TargetPinName Target pin internal name (e.g., "execute")
	 * @return true if connection succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Pins")
	static bool ConnectPins(
		UBlueprint* Blueprint,
		const FString& SourceNodeId,
		const FString& SourcePinName,
		const FString& TargetNodeId,
		const FString& TargetPinName,
		const FString& GraphName = TEXT(""));

	/** Set the default value of a pin.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @param PinName Pin internal name
	 * @param Value String representation of the value
	 * @return true if value was set
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Pins")
	static bool SetPinDefaultValue(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& PinName,
		const FString& Value,
		const FString& GraphName = TEXT(""));

	/** Break links on a pin.
	 * With TargetNodeId/TargetPinName empty, breaks ALL links on the pin.
	 * Otherwise breaks only the link to that specific pin. Needed to splice a node
	 * out of an exec chain (connect-only APIs cannot rewire).
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID owning the pin
	 * @param PinName Pin internal name
	 * @param GraphName Optional graph scope (empty = search all graphs)
	 * @param TargetNodeId Optional specific link target node
	 * @param TargetPinName Optional specific link target pin
	 * @return true if at least one link was broken
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Pins")
	static bool BreakPinLinks(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& PinName,
		const FString& GraphName = TEXT(""),
		const FString& TargetNodeId = TEXT(""),
		const FString& TargetPinName = TEXT(""));

	/** Remove a node from a graph, breaking all of its links first.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID to remove
	 * @param GraphName Optional graph scope (empty = search all graphs)
	 * @return true if the node was found and removed
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static bool RemoveNode(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& GraphName = TEXT(""));

	/** Reconstruct a node, refreshing its pins.
	 * Required after setting a class pin on a node whose output type is driven by
	 * that pin (DeterminesOutputType, e.g. GetComponentByClass / GetAgent) — until
	 * reconstructed the output pin keeps its base type and typed connections fail.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID to reconstruct
	 * @param GraphName Optional graph scope (empty = search all graphs)
	 * @return true if the node was found and reconstructed
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Nodes")
	static bool ReconstructNode(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& GraphName = TEXT(""));

	// =========================================================================
	// VARIABLES
	// =========================================================================

	/** Add a new user-defined function graph to a Blueprint.
	 * The graph is created with default FunctionEntry node and no parameters.
	 * Use AddFunctionParameter to add inputs/outputs.
	 * @param Blueprint Target Blueprint
	 * @param GraphName Name of the new function graph
	 * @return true if the function graph was created
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Functions")
	static bool AddFunctionGraph(
		UBlueprint* Blueprint,
		const FString& GraphName);

	/** Add a parameter (input or output) to an existing function graph.
	 * If adding the first output parameter, a FunctionResult node is created automatically.
	 * @param Blueprint Target Blueprint
	 * @param FunctionGraphName Name of the function graph
	 * @param ParamName Parameter name
	 * @param TypeName Type: "bool", "int", "float", "string", "Vector", "Rotator", "Transform", or a class path
	 * @param bIsInput If true, parameter is an input. If false, an output.
	 * @param DefaultValue Default value literal for input parameters (empty = no default)
	 * @return true if the parameter was added
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Functions")
	static bool AddFunctionParameter(
		UBlueprint* Blueprint,
		const FString& FunctionGraphName,
		const FString& ParamName,
		const FString& TypeName,
		bool bIsInput = true,
		const FString& DefaultValue = TEXT(""));

	/** Create an override of a parent-class function or event.
	 * Mirrors the editor's My Blueprint -> Override dropdown:
	 *  - functions that can be placed as events (BlueprintImplementableEvent /
	 *    BlueprintNativeEvent with no return) get a K2Node_Event in the event graph;
	 *  - everything else gets a new function graph carrying the parent's signature.
	 * @param Blueprint Target Blueprint
	 * @param FunctionName Name of the parent function to override
	 * @return Node ID of the created event node, or the graph name for function
	 *         overrides. Empty on failure (already overridden / not found).
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Functions")
	static FString AddFunctionOverride(
		UBlueprint* Blueprint,
		const FString& FunctionName);

	/** Add a variable to a Blueprint.
	 * @param Blueprint Target Blueprint
	 * @param Name Variable name
	 * @param TypeName Type: "bool", "int", "float", "string", "Vector", "Rotator", or a class path
	 * @param bInstanceEditable If true, variable is editable per instance
	 * @return true if variable was added
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Variables")
	static bool AddVariable(
		UBlueprint* Blueprint,
		const FString& Name,
		const FString& TypeName,
		bool bInstanceEditable = true);

	// =========================================================================
	// COMPILE / SAVE
	// =========================================================================

	/** Compile and save a Blueprint.
	 * @param Blueprint The Blueprint to compile and save
	 * @return true if compilation succeeded without errors
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Compile")
	static bool CompileAndSaveBlueprint(UBlueprint* Blueprint);

	// =========================================================================
	// INTROSPECTION
	// =========================================================================

	/** Get all node IDs in a graph.
	 * NOTE: with GraphName empty this returns ONLY the event graph's nodes. A Blueprint
	 * can have several ubergraph pages (e.g. an extra input-actions page) whose nodes
	 * are invisible here — pass GraphName, or use GetGraphNames to enumerate first.
	 * @param Blueprint Target Blueprint
	 * @param GraphName Optional graph name (empty = event graph)
	 * @return Array of node ID strings
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetNodeIds(
		UBlueprint* Blueprint,
		const FString& GraphName = TEXT(""));

	/** Get every graph on a Blueprint, including extra ubergraph pages.
	 * GetFunctionGraphNames lists only true functions, so additional ubergraph pages
	 * are otherwise undiscoverable and searches silently return false negatives.
	 * @param Blueprint Target Blueprint
	 * @return Array of "GraphName|Type" where Type is "Ubergraph", "Function", or "Macro"
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetGraphNames(UBlueprint* Blueprint);

	/** Get all pin names on a node.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @return Array of pin internal names
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetPinNames(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& GraphName = TEXT(""));

	/** Get detailed pin info for a node.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @return Array of strings in format "PinName|Direction|Type"
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetPinDetails(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& GraphName = TEXT(""));

	/** Get the human-readable list-view title of a node.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @return Node title string (e.g., "Print String", "Event BeginPlay")
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetNodeTitle(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& GraphName = TEXT(""));

	/** Get the function called by a K2Node_CallFunction node.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @return Format "MemberParent::MemberName", or empty string for non-call nodes
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetFunctionReference(
		UBlueprint* Blueprint,
		const FString& NodeId);

	/** Get the event/function being implemented by a K2Node_Event or K2Node_CustomEvent.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @return Format "MemberParent::MemberName" for events, "CustomEventName" for custom events, empty otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetEventReference(
		UBlueprint* Blueprint,
		const FString& NodeId);

	/** Get the variable name referenced by a K2Node_VariableGet or K2Node_VariableSet.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @return Variable name, or empty string for non-variable nodes
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetVariableReference(
		UBlueprint* Blueprint,
		const FString& NodeId);

	/** Get the macro graph identifier referenced by a K2Node_MacroInstance.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @return Format "MacroLibrary::MacroName" (or just "MacroName" if no library), empty for non-macro nodes
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetMacroReference(
		UBlueprint* Blueprint,
		const FString& NodeId);

	/** Get the pins this pin is connected to.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @param PinName Pin internal name
	 * @param GraphName Optional graph scope. Node IDs collide across a Blueprint's graphs
	 *        (every function graph has a "K2Node_FunctionEntry_0"), so an unscoped read
	 *        can silently return a different graph's node — always pass this when the
	 *        Blueprint has more than one graph.
	 * @return Array of "OtherNodeId.OtherPinName" strings (empty if pin is not connected)
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetPinConnections(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& PinName,
		const FString& GraphName = TEXT(""));

	/** Get the default literal value of an unconnected input pin.
	 * @param Blueprint Target Blueprint
	 * @param NodeId Node ID
	 * @param PinName Pin internal name
	 * @return DefaultValue string for value pins, or DefaultObject path for object/class pins. Empty if no default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetPinDefaultValue(
		UBlueprint* Blueprint,
		const FString& NodeId,
		const FString& PinName,
		const FString& GraphName = TEXT(""));

	/** Get the names of user-defined function graphs on a Blueprint.
	 * @param Blueprint Target Blueprint
	 * @return Array of function graph names (excludes event graphs and macros)
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetFunctionGraphNames(UBlueprint* Blueprint);

	/** Get all node IDs in a named graph (function graph, event graph, or macro graph).
	 * @param Blueprint Target Blueprint
	 * @param GraphName Name of the graph (e.g., "MyFunction", "EventGraph")
	 * @return Array of node ID strings
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetNodeIdsInGraph(
		UBlueprint* Blueprint,
		const FString& GraphName);

	/** Get the names of components on a Blueprint's Simple Construction Script (SCS).
	 * @param Blueprint Target Blueprint
	 * @return Array of component variable names
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetComponentTemplateNames(UBlueprint* Blueprint);

	/** Get the class name of an SCS component template.
	 * @param Blueprint Target Blueprint
	 * @param ComponentName Component variable name
	 * @return Class name (e.g., "StaticMeshComponent"), or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetComponentTemplateClass(
		UBlueprint* Blueprint,
		const FString& ComponentName);

	/** Get the names of macro graphs defined on a Blueprint.
	 * @param Blueprint Target Blueprint
	 * @return Array of macro graph names
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetMacroGraphNames(UBlueprint* Blueprint);

	/** Get the names of member variables declared on a Blueprint (NewVariables).
	 * @param Blueprint Target Blueprint
	 * @return Array of variable names
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetMemberVariableNames(UBlueprint* Blueprint);

	/** Get the type of a member variable as a richer string.
	 * Format: "category" for primitives, "struct:Name", "object:/Path", "class:/Path",
	 * "enum:Name". Container variables get "TArray<...>" / "TSet<...>" / "TMap<K,V>" prefix.
	 * @param Blueprint Target Blueprint
	 * @param VariableName Variable name
	 * @return Type string, or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetMemberVariableType(
		UBlueprint* Blueprint,
		const FString& VariableName);

	/** Get function-graph metadata.
	 * Format: "Category|Access|Pure|Const" where
	 *   Access in {"Public","Protected","Private"},
	 *   Pure in {"Pure","Impure"},
	 *   Const in {"Const","NonConst"}.
	 * @param Blueprint Target Blueprint
	 * @param FunctionGraphName Name of the function graph
	 * @return Metadata string, or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static FString GetFunctionMetadata(
		UBlueprint* Blueprint,
		const FString& FunctionGraphName);

	/** Get a function graph's input and output parameters.
	 * Each entry is "ParamName|Direction|Type|DefaultValue" where
	 * Direction is "Input"/"Output". Default value is empty for outputs.
	 * @param Blueprint Target Blueprint
	 * @param FunctionGraphName Name of the function graph
	 * @return Array of parameter strings
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetFunctionParameters(
		UBlueprint* Blueprint,
		const FString& FunctionGraphName);

	/** Get the names of parent-class BlueprintEvent functions that are NOT yet
	 * overridden in this Blueprint's event graph. Useful for discovering
	 * which events the BP could implement (BeginPlay, Tick, etc. on Actor).
	 * @param Blueprint Target Blueprint
	 * @return Array of overridable function names
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Graph|Introspection")
	static TArray<FString> GetOverridableFunctions(UBlueprint* Blueprint);

private:
	static UEdGraph* GetEventGraph(UBlueprint* Blueprint);
	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName);
	static UEdGraphNode* FindNodeById(UEdGraph* Graph, const FString& NodeId);
	static UEdGraphNode* FindNodeByIdAcrossGraphs(UBlueprint* Blueprint, const FString& NodeId);
	// Resolve a target graph: empty GraphName -> event graph, else the named graph.
	static UEdGraph* ResolveGraph(UBlueprint* Blueprint, const FString& GraphName);
	// Find a node: scoped to GraphName if given, else searched across all graphs.
	static UEdGraphNode* FindNodeInGraphOrAcross(UBlueprint* Blueprint, const FString& GraphName, const FString& NodeId);
	static bool TypeNameToPinType(const FString& TypeName, FEdGraphPinType& OutPinType);
	static FString FormatPinType(const FEdGraphPinType& T);
};
