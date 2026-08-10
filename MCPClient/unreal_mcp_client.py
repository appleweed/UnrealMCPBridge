# unreal_mcp_client.py
import socket
import json
import re
from mcp.server.fastmcp import FastMCP, Context

# Add a startup handler to connect to Unreal
from contextlib import asynccontextmanager
from collections.abc import AsyncIterator

@asynccontextmanager
async def app_lifespan(server: FastMCP) -> AsyncIterator[str]:
    """Connect to Unreal Engine when the MCP server starts"""
    connected = connect_to_unreal()
    if connected:
        yield "Connected to Unreal Engine"
    else:
        yield "Warning: Not connected to Unreal Engine. Make sure the plugin is running."

# Use the lifespan with the FastMCP instance
mcp = FastMCP("Unreal Engine", lifespan=app_lifespan)

# Configure socket connection
HOST = '127.0.0.1'
PORT = 9000
socket_client = None

# Connect to the Unreal Engine socket server
def connect_to_unreal():
    global socket_client
    try:
        socket_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        socket_client.connect((HOST, PORT))
        print(f"Connected to Unreal Engine on {HOST}:{PORT}")
        return True
    except Exception as e:
        print(f"Failed to connect to Unreal Engine: {e}")
        socket_client = None
        return False

# Send a command to Unreal Engine and get the response
def send_command(command, params=None):

    if socket_client is None:
        if not connect_to_unreal():
            return {"status": "error", "message": "Not connected to Unreal Engine"}
    
    if params is None:
        params = {}
    
    message = {
        "command": command,
        "params": params
    }
    
    try:
        # Send the message
        socket_client.sendall(json.dumps(message).encode('utf-8'))

        # Receive the response - accumulate chunks until valid JSON
        socket_client.settimeout(60.0)
        buffer = b''
        try:
            while True:
                chunk = socket_client.recv(65536)
                if not chunk:
                    break
                buffer += chunk

                # Try to parse accumulated buffer as JSON
                try:
                    decoded = buffer.decode('utf-8', 'replace')
                    stripped = decoded.strip('\'"\n\r')
                    replaced = stripped.replace('\\\'', '')
                    response = json.loads(replaced)
                    socket_client.settimeout(None)
                    return response
                except json.JSONDecodeError:
                    continue
        except socket.timeout:
            socket_client.settimeout(None)
            if buffer:
                decoded = buffer.decode('utf-8', 'replace')
                return {"status": "error", "message": f"Timeout - partial response: {decoded[:500]}"}
            return {"status": "error", "message": "Timeout waiting for response from Unreal Engine"}

        socket_client.settimeout(None)
        if buffer:
            decoded = buffer.decode('utf-8', 'replace')
            return {"status": "error", "message": f"Connection closed - partial: {decoded[:500]}"}
        return {"status": "error", "message": "Connection closed by Unreal Engine"}

    except Exception as e:
        print(f"Error sending command to Unreal: {e}")
        # Try to reconnect
        if connect_to_unreal():
            return send_command(command, params)
        return {"status": "error", "message": f"Communication error: {e}"}

# Tools
#@mcp.tool()
#def get_project_name() -> str:
#    """
#    Get the current Unreal Engine project name.
#    """
#    result = send_command("get_project_name")
#    if result.get("status") == "success":
#        return result.get("result", "Unknown Project")
#    else:
#        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_actors() -> str:
    """List all actors in the current level"""
    print(f"get_actors")
    result = send_command("get_actors")
    if result.get("status") == "success":
        actors = result.get("result", [])
        
        response = "# Actors in the current level\n\n"
        for actor in actors:
            response += f"- {actor.get('name')} ({actor.get('class')})\n"
            response += f"  Location: {actor.get('location')}\n"
        
        return response
    else:
        return f"get_actors error: " + json.dumps(result) #f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_actor_details(actor_name: str) -> str:
    """
    Get details for a specific actor by name.

    Args:
        actor_name: Name of the actor to retrieve details
    """
    result = send_command("get_actor_details", {"actor_name": actor_name})
    if result.get("status") == "success":
        actor = result.get("result", {})
        
        response = f"# Actor: {actor_name}\n\n"
        for key, value in actor.items():
            response += f"- {key}: {value}\n"
        
        return response
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def spawn_actor(asset_path: str, location_x: float = 0, location_y: float = 0, location_z: float = 0, rotation_x: float = 0, rotation_y: float = 0, rotation_z: float = 0, scale_x: float = 0, scale_y: float = 0, scale_z: float = 0) -> str:
    """
    Spawn an actor in the current level.
    
    Args:
        asset_path: Path to the asset on disk
        location_x: X coordinate for actor placement
        location_y: Y coordinate for actor placement
        location_z: Z coordinate for actor placement
        rotation_x: Rotation around X-axis
        rotation_y: Rotation around Y-axis
        rotation_z: Rotation around Z-axis
        scale_x: Scale of actor in X-direction
        scale_y: Scale of actor in Y-direction
        scale_z: Scale of actor in Z-direction
    """
    result = send_command("spawn_actor", {
        "asset_path": asset_path,
        "location_x": location_x,
        "location_y": location_y,
        "location_z": location_z,
        "rotation_x": rotation_x,
        "rotation_y": rotation_y,
        "rotation_z": rotation_z,
        "scale_x": scale_x,
        "scale_y": scale_y,
        "scale_z": scale_z
    })
    
    if result.get("status") == "success":
        return result.get("result", "Actor created successfully")
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def modify_actor(actor_name: str, property_name: str, property_value: str) -> str:
    """
    Modify a property of an existing actor.
    
    Args:
        actor_name: Name of the actor to modify
        property_name: Name of the property to change
        property_value: New value for the property (will be converted to appropriate type)
    """
    result = send_command("modify_actor", {
        "actor_name": actor_name,
        "property_name": property_name,
        "property_value": property_value
    })
    
    if result.get("status") == "success":
        return result.get("result", "Actor modified successfully")
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_selected_actors() -> str:
    """Get the currently selected actors in the editor"""
    result = send_command("get_selected_actors")
    
    if result.get("status") == "success":
        actors = result.get("result", [])
        
        if not actors:
            return "No actors are currently selected."
        
        response = "# Currently Selected Actors\n\n"
        for actor in actors:
            response += f"- {actor.get('name')} ({actor.get('class')})\n"
            response += f"  Location: {actor.get('location')}\n"
        
        return response
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def set_material(actor_name: str, material_path: str) -> str:
    """
    Apply a material to a static mesh actor.
    
    Args:
        actor_name: Name of the actor to modify
        material_path: Path to the material asset (e.g., '/Game/Materials/M_Basic')
    """
    result = send_command("set_material", {
        "actor_name": actor_name,
        "material_path": material_path
    })
    
    if result.get("status") == "success":
        return result.get("result", "Material applied successfully")
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def delete_all_static_mesh_actors() -> str:
    """Delete all static mesh actors in the scene"""
    result = send_command("delete_all_static_mesh_actors")
    if result.get("status") == "success":
        response = result.get("result")
        return response
    else:
        return json.dumps(result)

@mcp.tool()
def get_project_dir() -> str:
    """Get the top level project directory"""
    result = send_command("get_project_dir")
    if result.get("status") == "success":
        response = result.get("result")
        return response
    else:
        return json.dumps(result)

@mcp.tool()
def get_content_dir() -> str:
    """Get the content directory"""
    result = send_command("get_content_dir")
    if result.get("status") == "success":
        response = result.get("result")
        return response
    else:
        return json.dumps(result)

@mcp.tool()
def find_basic_shapes():
    """Search for basic shapes for building"""
    result = send_command("find_basic_shapes")
    if result.get("status") == "success":
        response = result.get("result")
        return json.dumps(response)
    else:
        return json.dumps(result)

@mcp.tool()
def find_assets(asset_name: str) -> str:
    """
    Search for assets by name (case-insensitive substring) across both
    /Game (project content) and /Engine (engine assets).

    Returns a JSON list of matches: [{"asset_class": ..., "asset_path": ...}, ...].
    Common substrings (e.g. "Material") can return many results — prefer a
    more specific fragment when possible. Project (/Game) hits are listed first.

    Args:
        asset_name: Substring to match against asset short names
    """
    result = send_command("find_assets", {
        "asset_name": asset_name
    })
    if result.get("status") == "success":
        response = result.get("result")
        return json.dumps(response)
    else:
        return json.dumps(result)

@mcp.tool()
def get_asset(asset_path: str) -> str:
    """
    Get the dimensions of an asset.
    
    Args:
        asset_path: Path to the asset on disk
    """
    result = send_command("get_asset", {
        "asset_path": asset_path
    })
    if result.get("status") == "success":
        response = result.get("result")
        return json.dumps(response)
    else:
        return json.dumps(result)

@mcp.tool()
def create_grid(asset_path: str, grid_width: int, grid_length: int) -> str:
    """
    Create a grid evenly spaced with the provided asset.
    
    Args:
        asset_path: Path to the tile asset on disk
        grid_width: Number of tiles in the x dimension
        grid_length: Number of tiles in the y dimension
    """
    result = send_command("create_grid", {
        "asset_path": asset_path,
        "grid_width": grid_width,
        "grid_length": grid_length
    })
    if result.get("status") == "success":
        response = result.get("result")
        return json.dumps(response)
    else:
        return json.dumps(result)

@mcp.tool()
def create_town(town_center_x: int, town_center_y: int, town_width: int, town_height: int) -> str:
    """Create a town using supplied assets

    Args:
        town_center_x: Center x position
        town_center_y: Center y position
        town_width: Width of town
        town_height: Height of town
    """
    result = send_command("create_town", {
        "town_center_x": town_center_x,
        "town_center_y": town_center_y,
        "town_width": town_width,
        "town_height": town_height
    })
    if result.get("status") == "success":
        response = result.get("result")
        return response
    else:
        return json.dumps(result)

@mcp.tool()
def run_blueprint_function(blueprint_name: str, function_name: str, arguments: str = "") -> str:
    """
    Execute a function in a Blueprint.
    
    Args:
        blueprint_name: Name of the Blueprint
        function_name: Name of the function to call
        arguments: Comma-separated list of arguments (if any)
    """
    result = send_command("execute_blueprint_function", {
        "blueprint_name": blueprint_name,
        "function_name": function_name,
        "arguments": arguments
    })
    
    if result.get("status") == "success":
        return result.get("result", "Blueprint function executed successfully")
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def take_screenshot() -> str:
    """Take a screenshot of the active editor viewport. Returns the file path of the saved PNG image."""
    result = send_command("take_screenshot")
    if result.get("status") == "success":
        return result.get("result", "Screenshot taken but path unknown")
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def set_viewport_camera(location_x: float = 0, location_y: float = 0, location_z: float = 0, rotation_pitch: float = 0, rotation_yaw: float = 0, rotation_roll: float = 0) -> str:
    """
    Move the editor viewport camera to a specific location and rotation.

    Args:
        location_x: X coordinate
        location_y: Y coordinate
        location_z: Z coordinate
        rotation_pitch: Look up (+) / down (-) in degrees
        rotation_yaw: Compass heading in degrees
        rotation_roll: Tilt in degrees
    """
    result = send_command("set_viewport_camera", {
        "location_x": location_x,
        "location_y": location_y,
        "location_z": location_z,
        "rotation_pitch": rotation_pitch,
        "rotation_yaw": rotation_yaw,
        "rotation_roll": rotation_roll
    })
    if result.get("status") == "success":
        return result.get("result", "Camera moved successfully")
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def focus_viewport_on_actor(actor_name: str, distance: float = 0) -> str:
    """
    Focus the editor viewport on a named actor. Positions camera at a 45-degree overhead angle.
    Distance is auto-calculated from actor bounds (minimum 1000 units) or can be overridden.

    Args:
        actor_name: Name of the actor to focus on
        distance: Camera distance from actor (0 = auto-calculate from bounds)
    """
    result = send_command("focus_viewport_on_actor", {
        "actor_name": actor_name,
        "distance": distance
    })
    if result.get("status") == "success":
        return result.get("result", "Viewport focused successfully")
    else:
        return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def execute_python(code: str) -> str:
    """
    Execute arbitrary Python code in Unreal Engine.

    Args:
        code: Python code to execute
    """

    # need to double all double-quotes
    #escaped = re.escape(code)

    # need to enclose in triple codes for Python exec()
    clean_code = f'""{code}""'

    result = send_command("execute_python", {
        "code": clean_code
    })
    
    if result is None:
        return f"Code executed successfully"
    elif isinstance(result, str):
        return f"Error: {result}"
    elif isinstance(result, dict):
        if result.get("status") == "success":
            return result.get("result", "Code executed successfully")
        else:
            error_msg = result.get('message', 'Unknown error')
            traceback = result.get('traceback', '')
            return f"Error: {error_msg}\n\n{traceback}"
    else:
        return f"Error: {str(result)}"

@mcp.prompt()
def create_castle() -> str:
    """Create a castle"""
    return f"""
Please create a castle in the current Unreal Engine project.
0. Refer to the Unreal Engine Python API when creating new python code: https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/?application_version=5.7
1. Clear all the StaticMeshActors in the scene.
2. Get the project directory and the content directory.
3. Find basic shapes to use for building structures.
4. Create a castle using these basic shapes.
"""

@mcp.prompt()
def create_town() -> str:
    """Create a town"""
    return f"""
Please create a town in the current Unreal Engine project.
0. Refer to the Unreal Engine Python API when creating new python code: https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/?application_version=5.7
1. Clear all the StaticMeshActors in the scene.
2. Get the project directory and the content directory.
3. Find a square static mesh asset that can be used as a floor tile. Note its dimensions.
4. Build a grid with the floor tile that is at least 7500 cm x 7500 cm.
5. Create a town centered on the grid and with the same width and height as the grid.
"""

# ============================================================================
# Trace Analysis Tools
# ============================================================================

@mcp.tool()
def start_trace() -> str:
    """
    Start capturing an Unreal Insights trace to file.
    Captures CPU, GPU, Frame, Counters, Region, and Net channels.
    The Net channel records replication bandwidth, RPC volume, and packet timing —
    use it for networked-game profiling (relevancy, NetUpdateFrequency, replication conditions).
    Call stop_trace() when done to get the file path.

    UE 5.5 limitation: do NOT run while a CSV profile is active — they trip an
    engine assertion (CsvProfilerProvider.cpp:170, 'CurrentCapture'). Use them
    sequentially on UE 5.5. Concurrent capture works fine on UE 5.6 and 5.7.
    """
    result = send_command("start_trace")
    if result.get("status") == "success":
        return result.get("result", "Trace started")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def stop_trace() -> str:
    """
    Stop the current trace capture.
    Returns the absolute path to the .utrace file.
    """
    result = send_command("stop_trace")
    if result.get("status") == "success":
        return result.get("result", "Trace stopped")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def analyze_trace(trace_path: str, top_n: int = 50) -> str:
    """
    Analyze a .utrace file and return a JSON summary of performance data.
    Includes: trace duration, thread info, frame statistics, and the top N
    most expensive timing scopes sorted by total inclusive time.

    Each scope includes: name, source file/line, call count,
    total/avg/max inclusive and exclusive time in milliseconds, and type (cpu/gpu).

    Args:
        trace_path: Absolute path to the .utrace file
        top_n: Number of top scopes to return (default 50, 0 = all)
    """
    result = send_command("analyze_trace", {
        "trace_path": trace_path,
        "top_n": top_n
    })
    if result.get("status") == "success":
        return result.get("result", "{}")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_trace_spikes(trace_path: str, budget_ms: float = 33.33, max_frames: int = 20, top_scopes_per_frame: int = 10) -> str:
    """
    Find frames in a .utrace that exceeded a time budget.
    For each spike frame, returns the top timing scopes contributing to that frame.

    Args:
        trace_path: Absolute path to the .utrace file
        budget_ms: Frame time budget in ms (default 33.33 = 30fps, use 16.67 for 60fps)
        max_frames: Maximum spike frames to return (default 20, 0 = all)
        top_scopes_per_frame: Top scopes per spike frame (default 10)
    """
    result = send_command("get_trace_spikes", {
        "trace_path": trace_path,
        "budget_ms": budget_ms,
        "max_frames": max_frames,
        "top_scopes_per_frame": top_scopes_per_frame
    })
    if result.get("status") == "success":
        return result.get("result", "{}")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_trace_frame_summary(trace_path: str) -> str:
    """
    Get frame timing statistics from a .utrace file.
    Returns: frame count, avg/min/max frame time, FPS, percentiles (p50/p90/p95/p99),
    and counts of frames exceeding 60fps and 30fps budgets.

    Args:
        trace_path: Absolute path to the .utrace file
    """
    result = send_command("get_trace_frame_summary", {
        "trace_path": trace_path
    })
    if result.get("status") == "success":
        return result.get("result", "{}")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def start_pie(net_mode: str = "standalone", num_clients: int = 1) -> str:
    """
    Start a Play-In-Editor (PIE) session with optional multiplayer config.

    Args:
        net_mode: "standalone" (default), "listen_server", or "client"
        num_clients: Number of player windows to spawn (>= 1)

    Returns "True" if PIE start was requested successfully. PIE start is async —
    use is_pie_running() to confirm before issuing further commands.

    Example: start_pie("listen_server", 2) to launch a listen-server with 2 clients
    (matching the multiplayer dropdown in the UE Play toolbar).
    """
    result = send_command("start_pie", {"net_mode": net_mode, "num_clients": num_clients})
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def stop_pie() -> str:
    """Stop the active PIE session. Returns 'True' if stop was requested."""
    result = send_command("stop_pie")
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def is_pie_running() -> str:
    """Check whether a PIE session is currently running. Returns 'True' or 'False'."""
    result = send_command("is_pie_running")
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_trace_net_summary(trace_path: str, top_n: int = 20) -> str:
    """
    Analyze the Net channel of a .utrace file. Returns network profiling data
    parsed from net trace events captured during the session:

    - Per-connection packet/byte totals (outbound + inbound) and drop rate
    - Top objects by outbound bandwidth (which actors are sending the most data)
    - Top event types by outbound bandwidth (which RPCs / replicated properties dominate)
    - Game instance info (server vs client, Iris replication on/off)

    Use this to diagnose replication bottlenecks: high-bandwidth actors are candidates
    for relevancy filtering, NetUpdateFrequency tuning, or push-model replication.

    If the trace has no Net channel data (single-player editor session), returns
    {has_net_data: false, ...} cleanly with a note.

    Args:
        trace_path: Absolute path to the .utrace file
        top_n: Top N objects and event types to return (default 20, 0 = all)
    """
    result = send_command("get_trace_net_summary", {
        "trace_path": trace_path,
        "top_n": top_n,
    })
    if result.get("status") == "success":
        return result.get("result", "{}")
    return f"Error: {result.get('message', 'Unknown error')}"

# ============================================================================
# CSV Profiler Tools
# ============================================================================

@mcp.tool()
def start_csv_profile() -> str:
    """
    Start the CSV profiler for high-level performance capture.
    Captures per-frame aggregate stats: FrameTime, GameThreadTime, RenderThreadTime,
    GPUTime, DrawCalls, memory usage, and hundreds of category-level counters.
    Call stop_csv_profile() when done to get the parsed summary.

    UE 5.5 limitation: do NOT run while an Insights trace (start_trace) is active —
    they trip an engine assertion (CsvProfilerProvider.cpp:170, 'CurrentCapture').
    Use them sequentially on UE 5.5. Concurrent capture works fine on UE 5.6 and 5.7.
    """
    result = send_command("start_csv_profile")
    if result.get("status") == "success":
        return result.get("result", "CSV profiler started")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def stop_csv_profile() -> str:
    """
    Stop the CSV profiler capture.
    The CSV file needs a moment to finalize. Call get_csv_profile() after a few seconds to read the results.
    """
    result = send_command("stop_csv_profile")
    if result.get("status") == "success":
        return result.get("result", "CSV profiler stopped")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_csv_profile() -> str:
    """
    Read and parse the most recent CSV profile into a performance summary.
    Call this after stop_csv_profile() has had a few seconds to finalize.

    Returns JSON with:
    - timing: avg/min/max/p50/p95/p99 for FrameTime, GameThreadTime, RenderThreadTime, GPUTime
    - counters: DrawCalls, PrimitivesDrawn, memory usage stats
    - budget: frames over 60fps/30fps budgets with percentages, average FPS
    - trend: first-half vs second-half comparison to detect degradation

    Use this for a quick high-level performance read. Follow up with start_trace/analyze_trace
    for function-level detail on any bottlenecks found.
    """
    result = send_command("get_csv_profile")
    if result.get("status") == "success":
        return result.get("result", "{}")
    return f"Error: {result.get('message', 'Unknown error')}"

# ============================================================================
# Blueprint Graph Introspection Tools
# ============================================================================
# These wrap UBlueprintGraphLibrary read functions for walking BP graphs.
# blueprint_path is an asset path like "/Game/Blueprints/BP_NPC.BP_NPC".
# node_id is a string like "K2Node_CallFunction_27" returned by node lookup.

@mcp.tool()
def get_node_title(blueprint_path: str, node_id: str, graph_name: str = "") -> str:
    """
    Get the human-readable list-view title of a Blueprint node.
    Examples: "Print String", "Event BeginPlay", "Branch", "Get MyVar".

    Use after listing node IDs to identify what each node is.
    graph_name scopes node lookup to that graph (empty = search all graphs).
    """
    result = send_command("get_node_title", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_function_reference(blueprint_path: str, node_id: str) -> str:
    """
    For a K2Node_CallFunction node, return "MemberParent::MemberName".
    Returns empty string for non-call nodes.

    Use to determine which function a call node invokes.
    """
    result = send_command("get_function_reference", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_event_reference(blueprint_path: str, node_id: str) -> str:
    """
    For a K2Node_Event, return "MemberParent::MemberName" of the implemented function.
    For a K2Node_CustomEvent, return the custom event name.
    Returns empty string for other node types.
    """
    result = send_command("get_event_reference", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_variable_reference(blueprint_path: str, node_id: str) -> str:
    """
    For a K2Node_VariableGet or K2Node_VariableSet, return the variable name.
    Returns empty string for non-variable nodes.
    """
    result = send_command("get_variable_reference", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_macro_reference(blueprint_path: str, node_id: str) -> str:
    """
    For a K2Node_MacroInstance, return "MacroLibrary::MacroName".
    Returns empty string for non-macro nodes.
    """
    result = send_command("get_macro_reference", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_pin_connections(blueprint_path: str, node_id: str, pin_name: str,
                        graph_name: str = "") -> str:
    """
    Get the pins this pin is connected to.
    Returns JSON list of "OtherNodeId.OtherPinName" strings.

    This is the key function for walking a Blueprint graph - follow exec
    pins ("then", "execute") to trace control flow, or follow data pins
    to trace value flow between nodes.

    IMPORTANT: pass graph_name whenever the Blueprint has more than one graph.
    Node IDs collide across graphs (every function graph has its own
    "K2Node_FunctionEntry_0"), so an unscoped read can silently return a
    different graph's node.
    """
    result = send_command("get_pin_connections", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
        "pin_name": pin_name,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_pin_default_value(blueprint_path: str, node_id: str, pin_name: str,
                          graph_name: str = "") -> str:
    """
    Get the default literal value of an unconnected input pin.
    For object/class pins returns the asset path; for value pins
    returns the literal string. Empty if no default is set.

    Pass graph_name when the Blueprint has more than one graph - node IDs
    collide across graphs and an unscoped read may resolve to the wrong node.
    """
    result = send_command("get_pin_default_value", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
        "pin_name": pin_name,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_function_graph_names(blueprint_path: str) -> str:
    """
    List user-defined function graphs on a Blueprint (excludes event graphs and macros).
    Returns JSON list of function graph names. Use the names with
    get_node_ids_in_graph() to inspect a specific function.
    """
    result = send_command("get_function_graph_names", {
        "blueprint_path": blueprint_path,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_node_ids_in_graph(blueprint_path: str, graph_name: str) -> str:
    """
    List node IDs in a specific graph (event graph, function, or macro).
    Returns JSON list. Pair with get_function_graph_names() to walk
    user-defined functions, or pass "EventGraph" for the main event graph.
    """
    result = send_command("get_node_ids_in_graph", {
        "blueprint_path": blueprint_path,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_component_template_names(blueprint_path: str) -> str:
    """
    List components on a Blueprint's Simple Construction Script (SCS).
    Returns JSON list of component variable names.
    """
    result = send_command("get_component_template_names", {
        "blueprint_path": blueprint_path,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_component_template_class(blueprint_path: str, component_name: str) -> str:
    """
    Get the class name of an SCS component on a Blueprint.
    Example: "StaticMeshComponent", "PointLightComponent".
    """
    result = send_command("get_component_template_class", {
        "blueprint_path": blueprint_path,
        "component_name": component_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

# ----------------------------------------------------------------------------
# Blueprint Graph Read Holdovers
# ----------------------------------------------------------------------------

@mcp.tool()
def get_node_ids(blueprint_path: str, graph_name: str = "") -> str:
    """
    List node IDs in a graph. Returns JSON list.

    WARNING: with graph_name empty this returns ONLY the event graph's nodes.
    A Blueprint can have several ubergraph pages (e.g. a separate input-actions
    page) whose nodes are invisible here - a search built on the default will
    report "not found" for nodes that plainly exist. Call get_graph_names()
    first to enumerate every graph, then pass graph_name.
    """
    result = send_command("get_node_ids", {
        "blueprint_path": blueprint_path,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_graph_names(blueprint_path: str) -> str:
    """
    List EVERY graph on a Blueprint. Returns JSON list of "Name|Type" strings
    where Type is "Ubergraph", "Function", or "Macro".

    Use this before any graph-wide search. get_function_graph_names() lists only
    true functions, so extra ubergraph pages are otherwise undiscoverable and
    searches silently return false negatives.
    """
    result = send_command("get_graph_names", {"blueprint_path": blueprint_path})
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_pin_names(blueprint_path: str, node_id: str, graph_name: str = "") -> str:
    """
    List all pin names on a node. Returns JSON list of internal pin names
    (e.g., "execute", "then", "ReturnValue"). Pair with get_pin_details()
    for direction and type info. graph_name scopes node lookup to that graph
    (empty = search all graphs); pass the function graph name to read
    FunctionEntry/FunctionResult and other function-body node pins.
    """
    result = send_command("get_pin_names", {
        "blueprint_path": blueprint_path,
        "node_id": node_id, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_pin_details(blueprint_path: str, node_id: str, graph_name: str = "") -> str:
    """
    Get detailed pin info for a node.
    Returns JSON list of strings in format "PinName|Direction|Type"
    where Direction is "Input" or "Output" and Type is the pin category
    (exec, bool, int, float, struct, object, etc.). graph_name scopes node
    lookup to that graph (empty = search all graphs); pass the function graph
    name to read FunctionEntry/FunctionResult and other function-body node pins.
    """
    result = send_command("get_pin_details", {
        "blueprint_path": blueprint_path,
        "node_id": node_id, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

# ----------------------------------------------------------------------------
# Blueprint Graph Write Tools
# ----------------------------------------------------------------------------

@mcp.tool()
def create_new_blueprint(path: str, name: str, parent_class_path: str) -> str:
    """
    Create a new Blueprint asset.

    Args:
        path: Content path (e.g., "/Game/Blueprints")
        name: Blueprint name (e.g., "BP_MyActor")
        parent_class_path: Class path (e.g., "/Script/Engine.Actor", "/Script/Engine.Pawn")

    Returns the full asset path of the created Blueprint, e.g.,
    "/Game/Blueprints/BP_MyActor.BP_MyActor". Use this path with the
    other BP graph tools and call compile_and_save_blueprint() when done.
    """
    result = send_command("create_new_blueprint", {
        "path": path, "name": name, "parent_class_path": parent_class_path,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_event_node(blueprint_path: str, event_name: str, pos_x: float = 0, pos_y: float = 0) -> str:
    """
    Add an event node to a Blueprint's event graph (e.g., "ReceiveBeginPlay",
    "ReceiveTick"). Note that BeginPlay and Tick already exist on new BPs;
    use get_node_ids() + get_event_reference() to find them rather than adding duplicates.

    Returns the node ID string. IDs may shift on compile, so re-introspect rather than caching.
    """
    result = send_command("add_event_node", {
        "blueprint_path": blueprint_path,
        "event_name": event_name,
        "pos_x": pos_x, "pos_y": pos_y,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_call_function_node(blueprint_path: str, target_class_path: str, function_name: str,
                           pos_x: float = 0, pos_y: float = 0, graph_name: str = "") -> str:
    """
    Add a function call node.

    Args:
        target_class_path: Class that owns the function
            (e.g., "/Script/Engine.KismetSystemLibrary", "/Script/Engine.KismetMathLibrary")
        function_name: Internal function name (e.g., "PrintString", "K2_SetTimer")
        graph_name: Function graph to add to (empty = the event graph)

    Returns the node ID string.
    """
    result = send_command("add_call_function_node", {
        "blueprint_path": blueprint_path,
        "target_class_path": target_class_path,
        "function_name": function_name,
        "pos_x": pos_x, "pos_y": pos_y, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_variable_get_node(blueprint_path: str, variable_name: str,
                          pos_x: float = 0, pos_y: float = 0, graph_name: str = "") -> str:
    """
    Add a variable Get node. Variable must already exist on the Blueprint
    (via add_variable()). graph_name targets a function graph (empty = the event graph).
    Returns the node ID.
    """
    result = send_command("add_variable_get_node", {
        "blueprint_path": blueprint_path,
        "variable_name": variable_name,
        "pos_x": pos_x, "pos_y": pos_y, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_variable_set_node(blueprint_path: str, variable_name: str,
                          pos_x: float = 0, pos_y: float = 0, graph_name: str = "") -> str:
    """
    Add a variable Set node. Variable must already exist on the Blueprint.
    graph_name targets a function graph by name (empty = the event graph).
    Returns the node ID.
    """
    result = send_command("add_variable_set_node", {
        "blueprint_path": blueprint_path,
        "variable_name": variable_name,
        "pos_x": pos_x, "pos_y": pos_y, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_branch_node(blueprint_path: str, pos_x: float = 0, pos_y: float = 0, graph_name: str = "") -> str:
    """
    Add a Branch (if/else) node. Returns the node ID.
    Wire the bool input to the "Condition" pin and the exec outputs from "then"/"else".
    graph_name targets a function graph by name (empty = the event graph).
    """
    result = send_command("add_branch_node", {
        "blueprint_path": blueprint_path,
        "pos_x": pos_x, "pos_y": pos_y, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_make_array_node(blueprint_path: str, num_inputs: int = 1,
                        pos_x: float = 0, pos_y: float = 0, graph_name: str = "",
                        element_type: str = "") -> str:
    """
    Add a Make Array node with num_inputs input pins.

    Pins are named "[0]", "[1]", ... and the output pin is "Array".

    element_type ("name", "int", "float", "string", "bool", "Vector", a struct/class
    path, ...) types the pins immediately. STRONGLY RECOMMENDED when you plan to set
    element defaults: without it the pins stay wildcards until the first connection
    resolves them, and that resolution reconstructs the node and DISCARDS any defaults
    you already set. (set_pin_default_value now refuses wildcard pins outright rather
    than reporting a write that won't survive.)

    Left untyped, the first connection fixes the element type and later mismatches are
    rejected - useful as a built-in check when wiring parallel arrays.

    Returns the node ID.
    """
    result = send_command("add_make_array_node", {
        "blueprint_path": blueprint_path,
        "num_inputs": num_inputs,
        "pos_x": pos_x,
        "pos_y": pos_y,
        "graph_name": graph_name,
        "element_type": element_type,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_variable_get_node_for_class(blueprint_path: str, variable_name: str,
                                    owner_class_path: str, pos_x: float = 0,
                                    pos_y: float = 0, graph_name: str = "") -> str:
    """
    Add a variable Get node for a variable owned by ANOTHER class.

    Unlike add_variable_get_node (self-scoped only), this produces a node with a
    target pin typed to owner_class_path, so you can read a member off another
    object - e.g. read "MidAttack" off a component reference obtained via
    get_component_by_class + add_cast_node.

    Args:
        variable_name: Variable on owner_class_path
        owner_class_path: Declaring class (e.g. "/Game/BP/BPC_X.BPC_X_C")

    Note: prefer an interface getter when one exists - it is the public API and
    survives refactors of the underlying member.
    Returns the node ID.
    """
    result = send_command("add_variable_get_node_for_class", {
        "blueprint_path": blueprint_path,
        "variable_name": variable_name,
        "owner_class_path": owner_class_path,
        "pos_x": pos_x,
        "pos_y": pos_y,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_function_override(blueprint_path: str, function_name: str) -> str:
    """
    Override a parent-class function or event - the equivalent of the editor's
    My Blueprint > Override dropdown.

    Two cases, handled automatically:
    - Functions placeable as events (BlueprintImplementableEvent / native events)
      get a K2Node_Event in the event graph. Returns the node ID.
    - Everything else gets a new function graph carrying the parent's signature
      (entry + result nodes with the right parameters). Returns the graph name,
      which you then pass as graph_name to the add_*_node calls.

    Use get_overridable_functions() to discover valid names. Returns empty/error
    if the function is already overridden or not found on the parent class.
    """
    result = send_command("add_function_override", {
        "blueprint_path": blueprint_path,
        "function_name": function_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def break_pin_links(blueprint_path: str, node_id: str, pin_name: str,
                    graph_name: str = "", target_node_id: str = "",
                    target_pin_name: str = "") -> str:
    """
    Break links on a pin.

    With target_node_id/target_pin_name empty, breaks ALL links on the pin.
    Otherwise breaks only the link to that specific pin.

    Needed to rewire an existing graph - connect_pins() alone cannot splice a node
    OUT of an exec chain. To remove node B from A -> B -> C: break_pin_links on
    A."then", then connect_pins A."then" -> C."execute".
    """
    result = send_command("break_pin_links", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
        "pin_name": pin_name,
        "graph_name": graph_name,
        "target_node_id": target_node_id,
        "target_pin_name": target_pin_name,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def remove_node(blueprint_path: str, node_id: str, graph_name: str = "") -> str:
    """
    Remove a node from a graph, breaking all of its links first.

    Fails for nodes the user cannot delete (function entry/result nodes).
    Pass graph_name to avoid deleting a same-named node in a different graph.
    """
    result = send_command("remove_node", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def reconstruct_node(blueprint_path: str, node_id: str, graph_name: str = "") -> str:
    """
    Reconstruct a node, refreshing its pins.

    Required after set_pin_default_value() on a class pin of a node whose OUTPUT
    type is driven by that pin (DeterminesOutputType - e.g. GetComponentByClass,
    or Learning Agents' GetAgent). Until reconstructed the output pin keeps its
    base type and connecting it to a typed pin fails, which looks like the class
    pin "didn't take" when it actually did.
    """
    result = send_command("reconstruct_node", {
        "blueprint_path": blueprint_path,
        "node_id": node_id,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_cast_node(blueprint_path: str, target_class_path: str,
                  pos_x: float = 0, pos_y: float = 0, graph_name: str = "") -> str:
    """
    Add a dynamic Cast node that casts an object to target_class_path.

    Args:
        target_class_path: Class to cast to (e.g., "/Script/Engine.Actor",
            "/Script/Engine.Pawn", or a BP class "/Game/BP/BP_NPC.BP_NPC_C")
        graph_name: Function graph to add to (empty = the event graph)

    The node is impure (has exec flow). Pins:
    - execute / then (exec), CastFailed (exec output on failed cast)
    - Object (input object to cast)
    - "As<ClassName>" (output — the cast result, e.g., "AsActor", "AsBP_NPC_C")
    Returns the node ID.
    """
    result = send_command("add_cast_node", {
        "blueprint_path": blueprint_path,
        "target_class_path": target_class_path,
        "pos_x": pos_x, "pos_y": pos_y, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_custom_event_node(blueprint_path: str, event_name: str,
                          pos_x: float = 0, pos_y: float = 0) -> str:
    """
    Add a Custom Event node. Custom events are entry points callable from BP code.
    Returns the node ID.
    """
    result = send_command("add_custom_event_node", {
        "blueprint_path": blueprint_path,
        "event_name": event_name,
        "pos_x": pos_x, "pos_y": pos_y,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_spawn_actor_node(blueprint_path: str, actor_class_path: str,
                         pos_x: float = 0, pos_y: float = 0, graph_name: str = "") -> str:
    """
    Add a SpawnActor node (the canonical UK2Node_SpawnActorFromClass).
    This is the preferred way to spawn actors in Blueprints — produces a single
    node with class-specific ExposeOnSpawn property pins, instead of the
    BeginDeferredActorSpawnFromClass + FinishSpawningActor two-node workaround.

    Args:
        actor_class_path: Class path of the actor to spawn
            (e.g., "/Game/BP/BP_NPC.BP_NPC_C", "/Script/Engine.PointLight")

    Returns the node ID. The node will have pins:
    - execute / then (exec)
    - Class (input class — already pinned to actor_class_path)
    - SpawnTransform (input struct)
    - CollisionHandlingOverride (input enum)
    - Owner (input object)
    - ReturnValue (output object — the spawned actor)
    - Plus dynamic input pins for any properties on actor_class_path marked
      EditAnywhere + ExposeOnSpawn (e.g., custom BP variables).
    """
    result = send_command("add_spawn_actor_node", {
        "blueprint_path": blueprint_path,
        "actor_class_path": actor_class_path,
        "pos_x": pos_x, "pos_y": pos_y, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def connect_pins(blueprint_path: str, source_node_id: str, source_pin: str,
                 target_node_id: str, target_pin: str, graph_name: str = "") -> str:
    """
    Connect two pins between nodes (exec or data).

    Pin name examples: "then" (event/call output exec), "execute" (call input exec),
    "ReturnValue" (call output data). Use get_pin_names()/get_pin_details() to find
    real names, since they're internal (not display) names.

    graph_name scopes node lookup to that graph (empty = search all graphs). Pass the
    function graph name when wiring inside a function so node-id collisions with the
    event graph can't shadow the intended node.

    Returns "True" or "False".
    """
    result = send_command("connect_pins", {
        "blueprint_path": blueprint_path,
        "source_node_id": source_node_id,
        "source_pin": source_pin,
        "target_node_id": target_node_id,
        "target_pin": target_pin, "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def set_pin_default_value(blueprint_path: str, node_id: str, pin_name: str, value: str,
                          graph_name: str = "") -> str:
    """
    Set the default value of an unconnected input pin.
    For object/class pins, pass the asset path (e.g., "/Game/BP/BP_NPC.BP_NPC_C").
    For value pins, pass the literal string ("true", "5", "1.5", "Hello", etc.).
    graph_name scopes node lookup to that graph (empty = search all graphs).
    Returns "True" or "False".
    """
    result = send_command("set_pin_default_value", {
        "blueprint_path": blueprint_path,
        "node_id": node_id, "pin_name": pin_name, "value": value,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_function_graph(blueprint_path: str, graph_name: str) -> str:
    """
    Create a new user-defined function graph on a Blueprint.
    The function starts with an empty FunctionEntry node and no parameters.
    Use add_function_parameter() to add inputs/outputs.
    Returns "True" or "False".
    """
    result = send_command("add_function_graph", {
        "blueprint_path": blueprint_path,
        "graph_name": graph_name,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_function_parameter(blueprint_path: str, function_graph_name: str, param_name: str,
                           type_name: str, is_input: bool = True, default_value: str = "") -> str:
    """
    Add an input or output parameter to a function graph.
    If adding the first output parameter, a FunctionResult node is created automatically.

    Args:
        type_name: "bool", "int", "float", "string", "name", "text",
                   "Vector", "Rotator", "Transform", or a class path like "/Script/Engine.Actor"
        is_input: True for input parameter, False for output.
        default_value: Default value literal for input parameters (ignored for outputs).

    Returns "True" or "False".
    """
    result = send_command("add_function_parameter", {
        "blueprint_path": blueprint_path,
        "function_graph_name": function_graph_name,
        "param_name": param_name,
        "type_name": type_name,
        "is_input": is_input,
        "default_value": default_value,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def add_variable(blueprint_path: str, name: str, type_name: str, instance_editable: bool = True) -> str:
    """
    Add a member variable to a Blueprint.

    Args:
        type_name: "bool", "int", "float", "string", "name", "text",
                   "Vector", "Rotator", "Transform", or a class path
                   like "/Script/Engine.Actor"
        instance_editable: If true, variable is editable per-instance in the editor.

    Returns "True" or "False".
    """
    result = send_command("add_variable", {
        "blueprint_path": blueprint_path,
        "name": name, "type_name": type_name,
        "instance_editable": instance_editable,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def compile_and_save_blueprint(blueprint_path: str) -> str:
    """
    Compile and save a Blueprint. Call this after a series of add/connect
    operations to persist the Blueprint to disk and mark it ready to use.
    Returns "True" if compilation succeeded without errors.
    """
    result = send_command("compile_and_save_blueprint", {
        "blueprint_path": blueprint_path,
    })
    if result.get("status") == "success":
        return str(result.get("result", False))
    return f"Error: {result.get('message', 'Unknown error')}"

# ----------------------------------------------------------------------------
# Blueprint Function/Variable Metadata (TODO #14)
# ----------------------------------------------------------------------------

@mcp.tool()
def get_macro_graph_names(blueprint_path: str) -> str:
    """
    List macro graphs defined on a Blueprint.
    Returns JSON list. Use the names with get_node_ids_in_graph() to walk a macro.
    """
    result = send_command("get_macro_graph_names", {"blueprint_path": blueprint_path})
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_member_variable_names(blueprint_path: str) -> str:
    """
    List member variables declared on a Blueprint (NewVariables, distinct from
    components and from variables referenced by Get/Set nodes).
    Returns JSON list of variable names.
    """
    result = send_command("get_member_variable_names", {"blueprint_path": blueprint_path})
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_member_variable_type(blueprint_path: str, variable_name: str) -> str:
    """
    Get the type of a member variable as a richer string.
    Format: "category" for primitives, "struct:Name", "object:/Path", "class:/Path",
    "enum:Name". Container types get "TArray<...>", "TSet<...>", or "TMap<K,V>" prefix.
    """
    result = send_command("get_member_variable_type", {
        "blueprint_path": blueprint_path,
        "variable_name": variable_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_function_metadata(blueprint_path: str, function_graph_name: str) -> str:
    """
    Get function-graph metadata.
    Returns "Category|Access|Pure|Const" where
    Access in {Public, Protected, Private}, Pure in {Pure, Impure},
    Const in {Const, NonConst}.
    """
    result = send_command("get_function_metadata", {
        "blueprint_path": blueprint_path,
        "function_graph_name": function_graph_name,
    })
    if result.get("status") == "success":
        return result.get("result", "")
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_function_parameters(blueprint_path: str, function_graph_name: str) -> str:
    """
    Get a function graph's input and output parameters.
    Returns JSON list of "ParamName|Direction|Type|DefaultValue" strings.
    Direction is "Input" or "Output". DefaultValue is empty for outputs.
    """
    result = send_command("get_function_parameters", {
        "blueprint_path": blueprint_path,
        "function_graph_name": function_graph_name,
    })
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

@mcp.tool()
def get_overridable_functions(blueprint_path: str) -> str:
    """
    List parent-class BlueprintEvent functions that are NOT yet overridden in this BP.
    Useful for discovering which events the BP could implement (e.g., on an Actor:
    ReceiveBeginPlay, ReceiveTick, ReceiveActorBeginOverlap, etc.).
    Returns JSON list of function names.
    """
    result = send_command("get_overridable_functions", {"blueprint_path": blueprint_path})
    if result.get("status") == "success":
        return json.dumps(result.get("result", []))
    return f"Error: {result.get('message', 'Unknown error')}"

# Run the server
if __name__ == "__main__":
    print("Starting Unreal Engine MCP Bridge")
    mcp.run(transport='stdio')
