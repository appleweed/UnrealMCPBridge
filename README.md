# Unreal Engine MCP Python Bridge Plugin

This is a plugin for Unreal Engine (UE) that creates a server implementation of [Model Context Protocol (MCP)](https://modelcontextprotocol.io/introduction).
This allows MCP clients, like Anthropic's [Claude](https://claude.ai/), to access the full UE Python API.

## Use Cases

- Developing tools and workflows in Python with an agent like Claude.
- Intelligent and dynamic automation of such workflows.
- General collaborative development with an agent.

## Prerequisites

- Visual Studio 2019 or higher (if building from code).
- An AI Agent. Below, we assume Claude will be used. But any AI Agent that implements MCP should suffice.
- Unreal Engine 5 with the Python Editor Script Plugin enabled.
- Note the [Unreal Engine Python API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/?application_version=5.5).

## Installing from Fab

The easiest way to install the MCP Python Bridge plugin is from the [Fab store listing](https://www.fab.com/listings/0167ac03-47b5-4a08-b68f-5d54ab7b208e). Once you purchase the plugin, it will appear in your Library in the Epic Games Launcher. Click "Install to Engine" and choose the appropriate version of Unreal Engine.

## Installing from GitHub

1. Create a new Unreal Engine C++ project.
2. Under the project root directory, find the `Plugins` folder.
3. Clone this repo into the `Plugins` folder so that there is a new containing folder with all the project contents underneath.
4. Right-click your UE project file (ends with `.uproject`) and select "Generate Visual Studio project files". If you don't immediately see that option, first select "Show more options" and it should appear.
5. Open your new Visual Studio project and build.
6. Copy `unreal_mcp_client.py` from the 'MCPClient' folder to a location of your choice.
7. Find your `claude_desktop_config.json` configuration file.
Mac location: `~/Library/Application\ Support/Claude/claude_desktop_config.json`
Windows location: `[path_to_your_user_account]\AppData\Roaming\Claude\claude_desktop_config.json`
8. Add the `unreal-engine` server section to your config file and update the path location excluding the square brackets, below.
Mac path format: `/[path_from_step_4]/unreal_mcp_client.py`
Windows path format: `C:\\[path_from_step_4]\\unreal_mcp_client.py`
```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "python",
      "args": [
        "[mac_or_windows_format_path_to_unreal_mcp_client.py]"
      ]
    } 
  }
}
```
9. Start Unreal Engine and load your new project.
10. Enable the new `UnrealMCPBridge` plugin, and restart.
11. There should be a new toolbar button that says, "Start MCP Bridge" when you hover with your mouse.
12. Click the MCP Bridge button. A pop-up will state, "MCP Python bridge running." The Output Log will note a new socket server listening on 127.0.0.1:9000.
13. Launch Claude as an administrator.
14. Click the "Attach from MCP" plug-icon. Under "Choose an integration" are two test Prompts: `create_castle` and `create_town`. You can edit their implementations in `unreal_server_init.py` under the `Content` folder of the plugin. Be sure to restart Unreal Engine after any changes.
15. Alternatively, ask Claude to build in your project using the UE Python API.
16. A list of currently implemented tools can be found by clicking the hammer-icon to the left of the plug-icon.

## Developing New Tools and Prompts

Examine at `unreal_mcp_client.py` and you'll see how MCP defines tools and prompts using Python decorators above functions. As an example:

```
@mcp.tool()
def get_project_dir() -> str:
    """Get the top level project directory"""
    result = send_command("get_project_dir")
    if result.get("status") == "success":
        response = result.get("result")
        return response
    else:
        return json.dumps(result)
```

This sends the `get_project_dir` command to Unreal Engine for execution and returns the project level directory for the current project. Under the `Content` folder of the plugin, you will see the server-side implementation of this tool command:

```
@staticmethod
def get_project_dir():
    """Get the top level project directory"""
    try:
        project_dir = unreal.Paths.project_dir()
        return json.dumps({
            "status": "success",
            "result" : f"{project_dir}"
        })
    except Exception as e:
        return json.dumps({ "status": "error", "message": str(e) })
``` 

Follow this pattern to create new tools the appear in the Claude desktop interface under the hammer-icon.

Implementing new prompts is a matter of adding them to `unreal_mcp_client.py`. As an example, here is the `create_castle` prompt from `unreal_mcp_client.py`:

```
@mcp.prompt()
def create_castle() -> str:
    """Create a castle"""
    return f"""
Please create a castle in the current Unreal Engine project.
0. Refer to the Unreal Engine Python API when creating new python code: https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/?application_version=5.5
1. Clear all the StaticMeshActors in the scene.
2. Get the project directory and the content directory.
3. Find basic shapes to use for building structures.
4. Create a castle using these basic shapes.
"""
```

Be sure to maintain triple-quotes so the entire prompt is returned. A good way to iterate over creating prompts is simply iterating each step number with Claude until you get satisfactory results. Then combine them all into a numbered step-by-step prompt as shown.

You must restart Claude for any changes to `unreal_mcp_client.py` to take effect. Note for Windows, you might need to end the Claude process in the Task Manager to truly restart Claude.
