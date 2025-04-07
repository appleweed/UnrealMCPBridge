#include "PythonBridge.h"
#include "../Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PythonScriptPlugin.h"
#include "Json.h"
#include "JsonObjectConverter.h"
#include <FileHelpers.h>
#include "Interfaces/IPluginManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "Microsoft/MinimalWindowsApi.h"

FString FPythonBridge::LoadFileToString(FString AbsolutePath)
{
    FString result = "NONE";

    FFileHelper::LoadFileToString(result, *AbsolutePath);
    //Reads file content and puts it inside result
    if (result == "NONE")
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file %s"), *AbsolutePath);
    }
    return result;
}

void FPythonBridge::Initialize()
{
	
    if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("Python bridge already initialized"));
		return;
	}
	bIsInitialized = true;

    UE_LOG(LogTemp, Display, TEXT("FPythonBridge::Initialize()"));
    // Initialize the bridge and set up any necessary Python environment
    FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("UnrealMCPBridge"))->GetContentDir();
	FString InitScript = LoadFileToString(PluginDir + "/unreal_server_init.py");
	//FString Cmd = "py " + PluginDir + "/unreal_server_init.py";

    FPythonCommandEx PythonCommand;
    PythonCommand.Command = *InitScript;
    PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;

    //bool Result = FPythonScriptPlugin::Get()->ExecPythonCommand(*InitScript);
    bool Result = FPythonScriptPlugin::Get()->ExecPythonCommandEx(PythonCommand);
	if (!Result)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to execute Python script"));
	}

    UE_LOG(LogTemp, Display, TEXT("Python bridge initialized"));
}

void FPythonBridge::Shutdown()
{
    // Clean up any Python resources
    FString ShutdownScript = TEXT("del mcp_bridge");
    FPythonScriptPlugin::Get()->ExecPythonCommand(*ShutdownScript);
    UE_LOG(LogTemp, Display, TEXT("Python bridge shut down"));
}

FString FPythonBridge::ExecuteCommand(const FString& Command, TSharedPtr<FJsonObject> Params)
{
    // Build the Python command to call the appropriate method
    FString PythonDict = ParamsToPythonDict(Params);
    FString PythonScript = FString::Printf(TEXT("mcp_bridge.%s(%s)"), *Command, *PythonDict);

    // Execute the Python script and return the result
    return ExecutePythonScript(PythonScript);
}

FString FPythonBridge::ExecutePythonScript(const FString& PythonScript)
{
    FPythonCommandEx PythonCommand;
	PythonCommand.Command = *PythonScript;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;

    UE_LOG(LogTemp, Display, TEXT("[Exec Python] %s"), *PythonScript);
    FString Result;

    // Execute Python on main thread
    FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
    {
            if (FPythonScriptPlugin::Get()->ExecPythonCommandEx(PythonCommand))
            {
				for (auto& Str : PythonCommand.LogOutput)
                {
                    Result += Str.Output;
                }
                //Result = TEXT("{\"status\":\"success\",\"result\":" + TotalOutput + "}");
                if (Result.IsEmpty())
                {
                    Result = TEXT("{\"status\":\"success\", \"result\":\"Nothing returned from script execution.\"}");
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[PYTHON FAILED]"));
                Result = TEXT("{\"status\":\"error\",\"message\":\"Failed to execute Python script \"}");
            }

    }, TStatId(), NULL, ENamedThreads::GameThread);

    FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);

    UE_LOG(LogTemp, Display, TEXT("[Result Python] %s"), *Result);

	return Result;
    
}

FString FPythonBridge::ParamsToPythonDict(TSharedPtr<FJsonObject> Params)
{
    if (!Params.IsValid())
    {
        return TEXT("");
    }

    TArray<FString> ParamStrings;

    for (const auto& Pair : Params->Values)
    {
        FString ValueStr;

        if (Pair.Value->Type == EJson::String)
        {
            ValueStr = FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString());
        }
        else if (Pair.Value->Type == EJson::Number)
        {
            ValueStr = FString::Printf(TEXT("%f"), Pair.Value->AsNumber());
        }
        else if (Pair.Value->Type == EJson::Boolean)
        {
            ValueStr = Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
        }
        else
        {
            // For complex types, convert back to JSON string
            //TSharedRef<FJsonValueObject> JsonValueObj = MakeShared<FJsonValueObject>(Pair.Value->AsObject());
            //FString JsonString;
            //TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
            //FJsonSerializer::Serialize(JsonValueObj, JsonWriter);

            // Convert back to JSON string
            TSharedPtr<FJsonObject> JsonObject = Pair.Value->AsObject();
            FString JsonString;
            TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
            FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);


            ValueStr = FString::Printf(TEXT("json.loads('%s')"), *JsonString.Replace(TEXT("'"), TEXT("\\'")));
        }

        ParamStrings.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *ValueStr));
    }

    return FString::Join(ParamStrings, TEXT(", "));
}