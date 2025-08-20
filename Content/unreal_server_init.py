import unreal
import json
import sys
import traceback

class MCPUnrealBridge:

    @staticmethod
    def get_actors():
        """Get all actors in the current level"""
        result = []
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) #unreal.EditorActorSubsystem().get_editor_subsystem()
        actors = actor_subsystem.get_all_level_actors()
        
        for actor in actors:
            result.append({
                "name": actor.get_name(),
                "class": actor.get_class().get_name(),
                "location": str(actor.get_actor_location())
            })
        
        return json.dumps({"status": "success", "result": result})

    @staticmethod
    def get_actor_details(actor_name):
        """Get details for a specific actor by name."""
        result = {}
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) #unreal.EditorActorSubsystem().get_editor_subsystem()
        actors = actor_subsystem.get_all_level_actors()
        
        for actor in actors:
            if actor.get_name() == actor_name:
                result = {
                    "name": actor.get_name(),
                    "class": actor.get_class().get_name(),
                    "location": str(actor.get_actor_location())
                }
                break

        if not result:
            result = f"Actor not found: {actor_name}"
        
        return json.dumps({"status": "success", "result": result})
    
    @staticmethod
    def spawn_actor(asset_path, location_x=0, location_y=0, location_z=0, rotation_x=0, rotation_y=0, rotation_z=0, scale_x=0, scale_y=0, scale_z=0):
        """Spawn a new actor in the level"""
        try:

            # Find the class reference
            class_obj = unreal.load_asset(asset_path)
            if not class_obj:
                return json.dumps({"status": "error", "message": f"Asset '{asset_path}' not found"})
            
            # Create the actor
            location = unreal.Vector(float(location_x), float(location_y), float(location_z))
            rotation = unreal.Rotator(rotation_x, rotation_y, rotation_z)
            actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            actor = actor_subsystem.spawn_actor_from_object(class_obj, location, rotation)
            
            if actor:
                actor.set_actor_scale3d(unreal.Vector(scale_x, scale_y, scale_z))
                return json.dumps({
                    "status": "success", 
                    "result": f"Created {asset_path} actor named '{actor.get_name()}' at location ({location_x}, {location_y}, {location_z})"
                })
            else:
                return json.dumps({ "status": "error", "message" : "Failed to create actor" })
        except Exception as e :
            return json.dumps({ "status": "error", "message" : str(e) })

    @staticmethod
    def modify_actor(actor_name, property_name, property_value):
        """Modify a property of an existing actor"""
        try:
            actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) #unreal.EditorActorSubsystem().get_editor_subsystem()
            actors = actor_subsystem.get_all_level_actors()

            for actor in actors:
                if actor.get_name() == actor_name:
                    # Try to determine property type and convert value
                    current_value = getattr(actor, property_name, None)
                    if current_value is not None:
                        if isinstance(current_value, float):
                            setattr(actor, property_name, float(property_value))
                        elif isinstance(current_value, int):
                            setattr(actor, property_name, int(property_value))
                        elif isinstance(current_value, bool):
                            setattr(actor, property_name, property_value.lower() in['true', 'yes', '1'])
                        elif isinstance(current_value, unreal.Vector):
                            # Assuming format like "X,Y,Z"
                            x, y, z = map(float, property_value.split(','))
                            setattr(actor, property_name, unreal.Vector(x, y, z))
                        else:
                            # Default to string
                            setattr(actor, property_name, property_value)

                        return json.dumps({
                            "status": "success",
                            "result" : f"Modified {property_name} on {actor_name} to {property_value}"
                        })
                    else:
                        return json.dumps({
                            "status": "error",
                            "message" : f"Property {property_name} not found on {actor_name}"
                        })

            return json.dumps({ "status": "error", "message" : f"Actor '{actor_name}' not found" })
        except Exception as e :
            return json.dumps({ "status": "error", "message" : str(e) })

    @staticmethod
    def get_selected_actors():
        """Get the currently selected actors in the editor"""
        try:
            actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) #unreal.EditorActorSubsystem().get_editor_subsystem()
            selected_actors = actor_subsystem.get_selected_level_actors()

            result = []
            for actor in selected_actors:
                result.append({
                    "name": actor.get_name(),
                    "class" : actor.get_class().get_name(),
                    "location" : str(actor.get_actor_location())
                })

            return json.dumps({ "status": "success", "result": result })
        except Exception as e:
            return json.dumps({ "status": "error", "message": str(e) })

    @staticmethod
    def set_material(actor_name, material_path):
        """Apply a material to a static mesh actor"""
        try:
            actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) #unreal.EditorActorSubsystem().get_editor_subsystem()
            actors = actor_subsystem.get_all_level_actors()

            # Find the actor
            target_actor = None
            for actor in actors :
                if actor.get_name() == actor_name :
                    target_actor = actor
                    break

            if not target_actor:
                return json.dumps({ "status": "error", "message" : f"Actor '{actor_name}' not found" })

            # Check if it's a static mesh actor
            if not target_actor.is_a(unreal.StaticMeshActor) :
                return json.dumps({
                    "status": "error",
                    "message": f"Actor '{actor_name}' is not a StaticMeshActor"
                })

            # Load the material
            material = unreal.load_object(None, material_path)
            if not material:
                return json.dumps({
                    "status": "error",
                    "message": f"Material '{material_path}' not found"
                })

            # Get the static mesh component
            static_mesh_component = target_actor.get_component_by_class(unreal.StaticMeshComponent)
            if not static_mesh_component:
                return json.dumps({
                    "status": "error",
                    "message" : f"No StaticMeshComponent found on actor '{actor_name}'"
                })

            # Set the material
            static_mesh_component.set_material(0, material)
            return json.dumps({
                "status": "success",
                "result" : f"Applied material '{material_path}' to actor '{actor_name}'"
            })
        except Exception as e :
            return json.dumps({ "status": "error", "message" : str(e) })

    @staticmethod
    def delete_all_static_mesh_actors():
        """Delete all static mesh actors in the scene"""

        try:

            # Get the editor subsystem
            editor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

            # Get all actors in the current level
            try:
                all_actors = editor_subsystem.get_all_level_actors()
            except DeprecationWarning as dw:
                # do nothing
                pass

            # Filter for StaticMeshActors
            static_mesh_actors = [actor for actor in all_actors if isinstance(actor, unreal.StaticMeshActor)]

            # Print how many StaticMeshActors were found
            #print(f"Found {len(static_mesh_actors)} StaticMeshActors")
                
            # Delete all StaticMeshActors
            delete_count = 0
            for actor in static_mesh_actors:
                actor_name = actor.get_actor_label()
                success = editor_subsystem.destroy_actor(actor)
                if success:
                    delete_count = delete_count + 1
                    #print(f"Deleted StaticMeshActor: {actor_name}")
                #else:
                    #print(f"Failed to delete StaticMeshActor: {actor_name}")

            return json.dumps({
                "status": "success",
                "result" : f"Found {len(static_mesh_actors)} StaticMeshActors. Deleted {delete_count} StaticMeshActors."
            })

        except Exception as e:
            return json.dumps({ "status": "error", "message": str(e) })

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

    @staticmethod
    def get_content_dir():
        """Get the content directory"""
        try:
            #plugins_dir = unreal.Paths.project_plugins_dir()
            #saved_dir = unreal.Paths.project_saved_dir()
            #config_dir = unreal.Paths.project_config_dir()
            content_dir = unreal.Paths.project_content_dir()
            return json.dumps({
                "status": "success",
                "result" : f"{content_dir}"
            })
        except Exception as e:
            return json.dumps({ "status": "error", "message": str(e) })

    @staticmethod
    def find_basic_shapes():
        """Search for basic shapes for building"""
        
        try:
            # Search for the asset
            asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
            assets = asset_registry.get_assets_by_path('/Engine/BasicShapes', recursive=True)

            # Find 
            tile_asset_paths = []
            for asset in assets:
                if asset.get_class().get_name() == 'StaticMesh':
                    tile_asset_paths.append(str(asset.package_name))

            if not tile_asset_paths:
                return json.dumps({ "status": "error", "message": f"Could not find basic shapes." })
            else:
                return json.dumps({
                    "status": "success",
                    "result" : tile_asset_paths
                })

        except Exception as e:
            return json.dumps({ "status": "error", "message": str(e) })

    @staticmethod
    def find_assets(asset_name):
        """Search for specific assets by name, like Floor, Wall, Door"""
        
        try:
            # Search for the asset
            asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
            assets = asset_registry.get_assets_by_path('/Game', recursive=True)

            # Find 
            tile_asset_paths = []
            asset_name_lower = asset_name.lower()
            for asset in assets:
                this_name = str(asset.asset_name).lower()
                if asset_name_lower in this_name:
                    asset_data = { "asset_class": str(asset.asset_class_path.asset_name), "asset_path": str(asset.package_name)}
                    tile_asset_paths.append(asset_data)

            if not tile_asset_paths:
                return json.dumps({ "status": "error", "message": f"Could not find {asset_name} asset." })
            else:
                return json.dumps({
                    "status": "success",
                    "result" : tile_asset_paths
                })

        except Exception as e:
            return json.dumps({ "status": "error", "message": str(e) })

    @staticmethod
    def get_asset(asset_path):
        # Define the correct path to the asset

        # Try to load the asset
        try:
            #print("Loading asset...")
            static_mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
            #print("Asset loaded: " + str(static_mesh != None))
            
            if static_mesh:
                #print("Asset class: " + static_mesh.__class__.__name__)
                
                # Try to spawn an actor
                try:
                    bounds = static_mesh.get_bounds()
                    bounds_min = bounds.origin - bounds.box_extent
                    bounds_max = bounds.origin + bounds.box_extent
                    width = bounds.box_extent.x * 2
                    depth = bounds.box_extent.y * 2
                    height = bounds.box_extent.z * 2
    
                    # Print dimensions
                    #print(f"Asset: SM_Env_Tiles_05")
                    #print(f"Bounds Min: {bounds_min}")
                    #print(f"Bounds Max: {bounds_max}")
                    #print(f"Dimensions (width × depth × height): {width} × {depth} × {height}")
                    #print(f"Volume: {bounds.box_extent.x * 2 * bounds.box_extent.y * 2 * bounds.box_extent.z * 2}")

                    result = {"width": width, "depth": depth, "height": height, "origin_x": bounds.origin.x, "origin_y": bounds.origin.y, "origin_z": bounds.origin.z}

                    return json.dumps({
                        "status": "success",
                        "result" : result
                    })

                except Exception as e:
                    return json.dumps({ "status": "error", "message": str(e) })
            else:
                return json.dumps({ "status": "error", "message": f"Asset could not be loaded." })
        except Exception as e:
            return json.dumps({ "status": "error", "message": f"Error loading asset: {str(e)}" })

    @staticmethod
    def create_grid(asset_path, grid_width, grid_length):
        import math

        try:

            # Load the static mesh
            floor_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
            if not floor_asset:
                return json.dumps({ "status": "error", "message": f"Failed to load static mesh: {asset_path}" })

            # Grid dimensions
            width = int(grid_width)
            length = int(grid_length)

            # Get asset dimensions
            bounds = floor_asset.get_bounds()
            tile_width = bounds.box_extent.x * 2
            tile_length = bounds.box_extent.y * 2

            # Create grid of floor tiles
            tiles_created = 0
            for x in range(width):
                for y in range(length):
                    # Calculate position
                    location = unreal.Vector(x * tile_width, y * tile_length, 0)
                    
                    # Create actor using EditorLevelLibrary
                    try:
                        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
                            floor_asset,
                            location,
                            unreal.Rotator(0, 0, 0)
                        )
                    except DeprecationWarning as dw:
                        # do nothing
                        pass
                    
                    if actor:
                        tiles_created += 1
                        actor.set_actor_label(f"FloorTile_{x}_{y}")

            center_x = width // 2
            center_y = length // 2
            position_x = center_x * tile_width
            position_y = center_y * tile_length
            return json.dumps({
                "status": "success",
                "result" : f"Successfully created grid centered at tile location: ({center_x}, {center_y}) and world location: ({position_x}, {position_y}, 0.0))."
            })

        except Exception as e:
            return json.dumps({ "status": "error", "message": f"Error loading asset: {str(e)}" })

    @staticmethod
    def create_town(town_center_x=1250, town_center_y=1250, town_width=7000, town_height=7000):
        """
        Create a town using supplied assets with customizable size and position
        
        Args:
            town_center_x (float): X coordinate of the town center
            town_center_y (float): Y coordinate of the town center
            town_width (float): Total width of the town area
            town_height (float): Total height of the town area
        """

        import random
        import math

        #print(f"Starting to build fantasy town at ({town_center_x}, {town_center_y}) with size {town_width}x{town_height}...")

        # Base paths for our assets
        model_base_path = "/Game/HandPaintedEnvironment/Assets/Models"

        # Helper function to load an asset
        def load_asset(asset_name):
            full_path = f"{model_base_path}/{asset_name}.{asset_name}"
            return unreal.EditorAssetLibrary.load_asset(full_path)

        # Track placed objects for collision detection
        placed_objects = []

        # Helper function to check for collision with existing objects
        def check_collision(new_bounds, tolerance=10.0):
            """Check if the new object bounds overlap with any existing objects"""
            for obj_bounds in placed_objects:
                # Check if the bounds overlap in X, Y dimensions with some tolerance
                if (new_bounds["min_x"] - tolerance <= obj_bounds["max_x"] and 
                    new_bounds["max_x"] + tolerance >= obj_bounds["min_x"] and
                    new_bounds["min_y"] - tolerance <= obj_bounds["max_y"] and
                    new_bounds["max_y"] + tolerance >= obj_bounds["min_y"]):
                    return True
            return False

        # Helper function to place an actor with collision detection
        def place_actor(static_mesh, x, y, z=0, rotation_z=0, scale=(1.0, 1.0, 1.0), name=None, max_attempts=5):
            if not static_mesh:
                #print(f"Cannot place actor: Static mesh is invalid")
                return None
                
            editor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

            # Get the bounds of the mesh
            bounds = static_mesh.get_bounds()
            bounds_min = bounds.origin - bounds.box_extent
            bounds_max = bounds.origin + bounds.box_extent
            
            # Calculate width, depth, height
            width = bounds.box_extent.x * scale[0]
            depth = bounds.box_extent.y * scale[1]
            height = bounds.box_extent.z * scale[2]
            
            # Try multiple positions if collision occurs
            for attempt in range(max_attempts):
                # Add small variation to position for retry attempts after the first
                x_offset = 0
                y_offset = 0
                if attempt > 0:
                    jitter_range = 20.0 * attempt  # Increase jitter with each attempt
                    x_offset = random.uniform(-jitter_range, jitter_range)
                    y_offset = random.uniform(-jitter_range, jitter_range)
                
                pos_x = x + x_offset
                pos_y = y + y_offset
                
                # Calculate the bounds of the actor at this position with rotation
                rad_rotation = math.radians(rotation_z)
                sin_rot = math.sin(rad_rotation)
                cos_rot = math.cos(rad_rotation)
                
                # Calculate the 4 corners of the bounding box after rotation
                corners = [
                    (pos_x - width/2, pos_y - depth/2),  # Bottom-left
                    (pos_x + width/2, pos_y - depth/2),  # Bottom-right
                    (pos_x + width/2, pos_y + depth/2),  # Top-right
                    (pos_x - width/2, pos_y + depth/2)   # Top-left
                ]
                
                # Rotate the corners
                rotated_corners = []
                for corner_x, corner_y in corners:
                    dx = corner_x - pos_x
                    dy = corner_y - pos_y
                    rotated_x = pos_x + dx * cos_rot - dy * sin_rot
                    rotated_y = pos_y + dx * sin_rot + dy * cos_rot
                    rotated_corners.append((rotated_x, rotated_y))
                
                # Find the extents of the rotated box
                x_values = [p[0] for p in rotated_corners]
                y_values = [p[1] for p in rotated_corners]
                
                min_x = min(x_values)
                max_x = max(x_values)
                min_y = min(y_values)
                max_y = max(y_values)
                
                new_bounds = {
                    "min_x": min_x,
                    "max_x": max_x,
                    "min_y": min_y,
                    "max_y": max_y,
                    "center_x": pos_x,
                    "center_y": pos_y
                }
                
                # Check if this position would cause a collision
                if not check_collision(new_bounds):
                    # No collision, so place the actor
                    location = unreal.Vector(pos_x, pos_y, z)
                    rotation = unreal.Rotator(0, 0, rotation_z)
                    actor = editor_subsystem.spawn_actor_from_object(static_mesh, location, rotation)
                    
                    if actor:
                        if name:
                            actor.set_actor_label(name)
                        
                        # Apply scale if needed
                        if scale != (1.0, 1.0, 1.0):
                            actor.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
                        
                        # Record the placed object
                        placed_objects.append(new_bounds)
                        
                        #if attempt > 0:
                        #    print(f"Placed {name} at alternate position after {attempt+1} attempts")
                        return actor
            
            #print(f"Failed to place {name} after {max_attempts} attempts due to collisions")
            return None

        # Helper function to get mesh dimensions
        def get_mesh_dimensions(static_mesh, scale=(1.0, 1.0, 1.0)):
            if not static_mesh:
                return 0, 0, 0
                
            bounds = static_mesh.get_bounds()
            width = bounds.box_extent.x * scale[0]
            depth = bounds.box_extent.y * scale[1]
            height = bounds.box_extent.z * scale[2]
            
            return width, depth, height

        # New helper for placing continuous walls or fences
        def place_continuous_segments(mesh, start_point, end_point, z=0, rotation_offset=0, scale=(1.0, 1.0, 1.0), name_prefix="Segment", skip_indices=None):
            if not mesh:
                #print(f"Cannot place continuous segments: Mesh is invalid")
                return []
                
            if skip_indices is None:
                skip_indices = []
                
            # Get the dimensions of the mesh
            mesh_width, mesh_depth, mesh_height = get_mesh_dimensions(mesh, scale)
            
            # For walls/fences, we assume they extend primarily along one dimension (width)
            segment_length = mesh_width
            
            # Calculate distance and direction
            start_x, start_y = start_point
            end_x, end_y = end_point
            dx = end_x - start_x
            dy = end_y - start_y
            distance = math.sqrt(dx*dx + dy*dy)
            
            # Calculate the angle of the line
            angle = math.degrees(math.atan2(dy, dx))
            
            # Calculate number of segments needed to cover the distance
            # Subtract a small overlap percentage to ensure segments connect properly
            overlap_factor = 0.05  # 5% overlap to ensure no gaps
            effective_segment_length = segment_length * (1 - overlap_factor)
            num_segments = max(1, math.ceil(distance / effective_segment_length))
            
            # Place segments along the line
            placed_actors = []
            for i in range(num_segments):
                if i in skip_indices:
                    continue
                    
                # Calculate position along the line
                t = i / num_segments
                pos_x = start_x + t * dx
                pos_y = start_y + t * dy
                
                # Place the segment
                actor = place_actor(mesh, pos_x, pos_y, z, angle + rotation_offset, scale, f"{name_prefix}_{i}")
                if actor:
                    placed_actors.append(actor)
            
            return placed_actors

        try:
            # Calculate the scale factor based on town size compared to the original
            # Original size was approximately 700x700
            width_scale = 1.0 #town_width / 700.0
            height_scale = 1.0 #town_height / 700.0

            # We'll use this to scale distances and positions
            scale_factor = min(width_scale, height_scale)  # Use the smaller scale to ensure everything fits

            # STEP 1: PLACE BUILDINGS
            #print("STEP 1: Placing buildings...")

            # Load building assets
            buildings = {
                "town_hall": load_asset("Town_Hall"),
                "large_house": load_asset("Large_house"),
                "small_house": load_asset("Small_house"),
                "baker_house": load_asset("Baker_house"),
                "tavern": load_asset("Tavern"),
                "witch_house": load_asset("Witch_house"),
                "tower": load_asset("Tower"),
                "mill": load_asset("Mill"),
                "woodmill": load_asset("Woodmill"),
                "forge": load_asset("Forge"),
                "mine": load_asset("Mine")
            }

            # Check if assets loaded correctly
            #for name, asset in buildings.items():
            #    if not asset:
            #        print(f"Failed to load {name}")

            # Place central buildings - town hall
            place_actor(buildings["town_hall"], town_center_x, town_center_y, 0, 0, name="Central_TownHall")

            # Place tavern near the town center - scale the offset by our scale factor
            place_actor(buildings["tavern"], 
                        town_center_x + 800 * scale_factor, 
                        town_center_y + 600 * scale_factor, 
                        0, 135, name="Tavern")

            # Place houses around the center - scaled based on town size
            house_positions = [
                # North district
                (town_center_x - 1800 * scale_factor, town_center_y - 1900 * scale_factor, 45, "small_house", "North_House_1"),
                (town_center_x - 2100 * scale_factor, town_center_y - 2200 * scale_factor, 30, "baker_house", "North_BakerHouse"),
                (town_center_x - 1300 * scale_factor, town_center_y - 2000 * scale_factor, 15, "small_house", "North_House_2"),
                
                # East district
                (town_center_x + 2200 * scale_factor, town_center_y - 1300 * scale_factor, 270, "large_house", "East_LargeHouse"),
                (town_center_x + 2400 * scale_factor, town_center_y + 1200 * scale_factor, 300, "small_house", "East_House_1"),
                (town_center_x + 1900 * scale_factor, town_center_y - 1700 * scale_factor, 315, "small_house", "East_House_2"),
                
                # South district
                (town_center_x + 1300 * scale_factor, town_center_y + 2100 * scale_factor, 180, "large_house", "South_LargeHouse"),
                (town_center_x - 1200 * scale_factor, town_center_y + 1950 * scale_factor, 135, "small_house", "South_House_1"),
                
                # West district
                (town_center_x - 2000 * scale_factor, town_center_y + 1100 * scale_factor, 90, "large_house", "West_LargeHouse"),
                (town_center_x - 2300 * scale_factor, town_center_y + 1500 * scale_factor, 45, "small_house", "West_House_1")
            ]

            # Place the houses
            for x, y, rot, house_type, name in house_positions:
                place_actor(buildings[house_type], x, y, 0, rot, name=name)

            # Place special buildings
            # Tower
            place_actor(buildings["tower"], 
                        town_center_x - 3000 * scale_factor, 
                        town_center_y - 2900 * scale_factor, 
                        0, 45, name="North_Tower")

            # Witch's house in a more secluded area
            place_actor(buildings["witch_house"], 
                        town_center_x + 3000 * scale_factor, 
                        town_center_y + 2900 * scale_factor, 
                        0, 215, name="WitchHouse")

            # Mill near water (imaginary river)
            mill_x = town_center_x + 2900 * scale_factor
            mill_y = town_center_y - 3000 * scale_factor
            mill = place_actor(buildings["mill"], mill_x, mill_y, 0, 270, name="Watermill")
            
            # Mill wings - need to be attached to the mill
            mill_wings = load_asset("Mill_wings")
            if mill and mill_wings:
                place_actor(mill_wings, mill_x, mill_y, 0, 270, name="Watermill_Wings")

            # Woodmill in a wooded area
            woodmill_x = town_center_x - 2900 * scale_factor
            woodmill_y = town_center_y - 2400 * scale_factor
            place_actor(buildings["woodmill"], woodmill_x, woodmill_y, 0, 135, name="Woodmill")
            
            woodmill_saw = load_asset("Woodmill_Saw")
            if woodmill_saw:
                place_actor(woodmill_saw, woodmill_x, woodmill_y, 0, 135, name="Woodmill_Saw")

            # Forge
            place_actor(buildings["forge"], 
                        town_center_x + 1600 * scale_factor, 
                        town_center_y - 1500 * scale_factor, 
                        0, 330, name="Forge")

            # Mine at the edge of town
            place_actor(buildings["mine"], 
                        town_center_x - 2900 * scale_factor, 
                        town_center_y + 2900 * scale_factor, 
                        0, 135, name="Mine")

            #print("Buildings placed successfully")

            # STEP 2: ADD NATURE ELEMENTS
            #print("STEP 2: Adding trees, plants, and rocks...")

            # Load nature assets
            nature = {
                "tree_1": load_asset("Tree_1"),
                "tree_2": load_asset("Tree_2"),
                "tree_4": load_asset("Tree_4"),
                "pine_tree": load_asset("Pine_tree"),
                "pine_tree_2": load_asset("Pine_tree_2"),
                "bush_1": load_asset("Bush_1"),
                "bush_2": load_asset("Bush_2"),
                "fern": load_asset("Fern"),
                "flowers_1": load_asset("Flowers_1"),
                "flowers_2": load_asset("Flowers_2"),
                "plant": load_asset("Plant"),
                "rock_1": load_asset("Rock_1"),
                "rock_2": load_asset("Rock_2"),
                "rock_3": load_asset("Rock_3"),
                "rock_4": load_asset("Rock_4"),
                "stump": load_asset("Stump"),
                "log": load_asset("Log"),
                "mushroom_1": load_asset("Mushroom_1"),
                "mushroom_2": load_asset("Mushroom_2")
            }

            # Create a forest area near the witch's house
            forest_center_x = town_center_x + 2900 * scale_factor
            forest_center_y = town_center_y + 2200 * scale_factor
            forest_radius = 1500 * scale_factor

            # Determine number of trees based on forest area
            num_trees = int(500 * scale_factor)
            
            # Add trees to the forest
            for i in range(num_trees):
                # Calculate random position within the forest area
                angle = random.uniform(0, 2 * math.pi)
                distance = random.uniform(0, forest_radius)
                x = forest_center_x + distance * math.cos(angle)
                y = forest_center_y + distance * math.sin(angle)
                
                # Choose a random tree type
                tree_type = random.choice(["tree_1", "tree_2", "tree_4", "pine_tree", "pine_tree_2"])
                rot = random.uniform(0, 360)
                scale = random.uniform(0.8, 1.2)
                
                tree = place_actor(nature[tree_type], x, y, 0, rot, (scale, scale, scale), f"Forest_Tree_{i}")
                
                # Add some undergrowth near trees if the tree was placed successfully
                if tree and random.random() < 0.6:
                    undergrowth_type = random.choice(["bush_1", "bush_2", "fern", "mushroom_1", "mushroom_2"])
                    offset_x = random.uniform(-100, 100)
                    offset_y = random.uniform(-100, 100)
                    undergrowth_scale = random.uniform(0.7, 1.0)
                    place_actor(nature[undergrowth_type], 
                                x + offset_x, 
                                y + offset_y, 
                                0, 
                                random.uniform(0, 360), 
                                (undergrowth_scale, undergrowth_scale, undergrowth_scale), 
                                f"Forest_Undergrowth_{i}")

            # Add scattered trees around town - scale the number by town size
            scattered_trees = int(100 * scale_factor)
            for i in range(scattered_trees):
                angle = random.uniform(0, 2 * math.pi)
                distance = random.uniform(3000, 3000) * scale_factor
                x = town_center_x + distance * math.cos(angle)
                y = town_center_y + distance * math.sin(angle)
                
                # Avoid placing in the forest area
                forest_dist = math.sqrt((x - forest_center_x)**2 + (y - forest_center_y)**2)
                if forest_dist < forest_radius:
                    continue
                
                tree_type = random.choice(["tree_1", "tree_2", "tree_4"])
                rot = random.uniform(0, 360)
                scale = random.uniform(0.9, 1.1)
                
                place_actor(nature[tree_type], x, y, 0, rot, (scale, scale, scale), f"Town_Tree_{i}")

            # Add some rocks scattered around
            num_rocks = int(15 * scale_factor)
            for i in range(num_rocks):
                angle = random.uniform(0, 2 * math.pi)
                distance = random.uniform(3000, 3000) * scale_factor
                x = town_center_x + distance * math.cos(angle)
                y = town_center_y + distance * math.sin(angle)
                
                rock_type = random.choice(["rock_1", "rock_2", "rock_3", "rock_4"])
                rot = random.uniform(0, 360)
                scale = random.uniform(0.8, 1.5)
                
                place_actor(nature[rock_type], x, y, 0, rot, (scale, scale, scale), f"Rock_{i}")

            # Create gardens near houses
            houses = [
                (town_center_x - 2000 * scale_factor, town_center_y - 1900 * scale_factor),  # North_House_1
                (town_center_x - 1300 * scale_factor, town_center_y - 2000 * scale_factor), # North_House_2
                (town_center_x + 2400 * scale_factor, town_center_y + 1200 * scale_factor), # East_House_1
                (town_center_x - 1200 * scale_factor, town_center_y + 1950 * scale_factor),  # South_House_1
            ]

            for idx, (house_x, house_y) in enumerate(houses):
                garden_x = house_x + random.uniform(200, 300)
                garden_y = house_y + random.uniform(200, 300)
                
                # Place flowers and plants in the garden
                plants_per_garden = int(5 * scale_factor)
                for j in range(plants_per_garden):
                    plant_x = garden_x + random.uniform(-100, 100)
                    plant_y = garden_y + random.uniform(-100, 100)
                    plant_type = random.choice(["flowers_1", "flowers_2", "plant", "bush_1"])
                    rot = random.uniform(0, 360)
                    
                    place_actor(nature[plant_type], plant_x, plant_y, 0, rot, name=f"Garden_{idx}_Plant_{j}")

            #print("Nature elements added successfully")

            # STEP 3: CREATE FENCES
            #print("STEP 3: Building fences...")

            # Load fence assets
            fences = {
                "fence": load_asset("Fence"),
                "fence_1": load_asset("Fence_1"),
                "fence_2": load_asset("Fence_2"),
                "stone_fence": load_asset("Stone_fence")
            }

            # Define garden perimeters to fence
            gardens = [
                {
                    "center": (town_center_x - 1800 * scale_factor, town_center_y - 1900 * scale_factor),  # North_House_1
                    "size": (30 * scale_factor, 30 * scale_factor),
                    "fence_type": "fence",
                    "name": "North_House_1_Garden"
                },
                {
                    "center": (town_center_x + 2400 * scale_factor, town_center_y + 1200 * scale_factor),  # East_House_1
                    "size": (25 * scale_factor, 30 * scale_factor),
                    "fence_type": "fence_1",
                    "name": "East_House_1_Garden"
                },
                {
                    "center": (town_center_x + 1300 * scale_factor, town_center_y + 2200 * scale_factor),  # South_LargeHouse
                    "size": (35 * scale_factor, 35 * scale_factor),
                    "fence_type": "fence_2",
                    "name": "South_LargeHouse_Garden"
                }
            ]

            # For each garden, create a fence perimeter
            for garden in gardens:
                center_x, center_y = garden["center"]
                width, height = garden["size"]
                fence_type = garden["fence_type"]
                name_prefix = garden["name"]
                
                # Calculate the corner points of the garden
                half_width = width / 2
                half_height = height / 2
                
                corners = [
                    (center_x - half_width, center_y - half_height),  # Bottom-left
                    (center_x + half_width, center_y - half_height),  # Bottom-right
                    (center_x + half_width, center_y + half_height),  # Top-right
                    (center_x - half_width, center_y + half_height)   # Top-left
                ]
                
                # Build the perimeter with continuous segments
                for i in range(4):
                    start_point = corners[i]
                    end_point = corners[(i + 1) % 4]
                    
                    # For each side, determine if we need a gate
                    skip_indices = []
                    if i == 0:  # Usually front of the garden has a gate
                        skip_indices = [1]  # Skip middle segment for a gate
                    
                    # Place continuous fence segments
                    place_continuous_segments(
                        fences[fence_type],
                        start_point,
                        end_point,
                        0,  # z coordinate
                        90,  # rotation offset (fences usually need to be rotated perpendicular to the line)
                        (1.0, 1.0, 1.0),  # scale
                        f"{name_prefix}_Fence_{i}",
                        skip_indices
                    )

            # Create a fence around the town center square
            town_square = {
                "center": (town_center_x, town_center_y),
                "size": (90 * scale_factor, 90 * scale_factor),
                "fence_type": "stone_fence",
                "name": "Town_Square"
            }

            center_x, center_y = town_square["center"]
            width, height = town_square["size"]
            fence_type = town_square["fence_type"]
            name_prefix = town_square["name"]

            # Calculate the corner points
            half_width = width / 2
            half_height = height / 2

            corners = [
                (center_x - half_width, center_y - half_height),  # Bottom-left
                (center_x + half_width, center_y - half_height),  # Bottom-right
                (center_x + half_width, center_y + half_height),  # Top-right
                (center_x - half_width, center_y + half_height)   # Top-left
            ]

            # Build the perimeter with continuous segments and gates
            for i in range(4):
                start_point = corners[i]
                end_point = corners[(i + 1) % 4]
                
                # Create entrance gates on all sides
                skip_indices = [1]  # Skip middle segment for a gate
                
                # Place continuous stone fence segments
                place_continuous_segments(
                    fences[fence_type],
                    start_point,
                    end_point,
                    0,  # z coordinate
                    90,  # rotation offset
                    (1.0, 1.0, 1.0),  # scale
                    f"{name_prefix}_Fence_{i}",
                    skip_indices
                )

            #print("Fences created successfully")

            # STEP 4: BUILD WALLS
            #print("STEP 4: Building walls...")

            # Load wall assets
            walls = {
                "wall_1": load_asset("Wall_1"),
                "wall_2": load_asset("Wall_2")
            }

            # Get wall dimensions for alternating walls
            wall1_width, _, _ = get_mesh_dimensions(walls["wall_1"])
            wall2_width, _, _ = get_mesh_dimensions(walls["wall_2"])

            # Define the town perimeter for walls - use the provided town width and height
            town_perimeter = {
                "center": (town_center_x, town_center_y),
                "size": (town_width, town_height),
                "name": "Town_Wall"
            }

            center_x, center_y = town_perimeter["center"]
            width, height = town_perimeter["size"]
            name_prefix = town_perimeter["name"]

            # Calculate the corner points
            half_width = (width / 2) - wall1_width
            half_height = (height / 2) - wall1_width

            corners = [
                (center_x - half_width, center_y - half_height),  # Bottom-left
                (center_x + half_width, center_y - half_height),  # Bottom-right
                (center_x + half_width, center_y + half_height),  # Top-right
                (center_x - half_width, center_y + half_height)   # Top-left
            ]

            # Build the perimeter with continuous wall segments
            # For each side, first measure the total length and determine how many of each wall type to use
            for i in range(4):
                start_point = corners[i]
                end_point = corners[(i + 1) % 4]
                
                # Calculate side length
                start_x, start_y = start_point
                end_x, end_y = end_point
                side_length = math.sqrt((end_x - start_x)**2 + (end_y - start_y)**2)
                
                # Determine where to place gates - we'll place 1-2 gates on each side based on length
                # Calculate how many walls would fit on this side
                effective_wall1_length = wall1_width * 0.95  # Account for slight overlap
                num_wall1_segments = math.ceil(side_length / effective_wall1_length)
                
                # Identify gate positions - which segments to skip
                gate_skip_indices = []
                if num_wall1_segments > 8:
                    # Place two gates if the wall is long enough
                    gate_skip_indices = [num_wall1_segments // 3, 2 * num_wall1_segments // 3]
                else:
                    # Otherwise just one gate in the middle
                    gate_skip_indices = [num_wall1_segments // 2]
                
                # Place the wall segments using primarily wall_1 with gates
                wall1_actors = place_continuous_segments(
                    walls["wall_1"],
                    start_point,
                    end_point,
                    0,  # z coordinate
                    0,   # no rotation offset for walls
                    (1.0, 1.0, 1.0),  # scale
                    f"{name_prefix}_Wall1_{i}",
                    gate_skip_indices
                )
                
                # For each gate position, place a tower
                for gate_idx in gate_skip_indices:
                    t = gate_idx / num_wall1_segments
                    pos_x = start_x + t * (end_x - start_x)
                    pos_y = start_y + t * (end_y - start_y)
                    
                    # Calculate angle
                    angle = math.degrees(math.atan2(end_y - start_y, end_x - start_x))
                    
                    # Place tower at gate position
                    tower = load_asset("Tower")
                    if tower:
                        place_actor(tower, pos_x, pos_y, 0, angle + 90, name=f"Gate_Tower_{i}_{gate_idx}")

            #print("Walls built successfully")

            # STEP 5: ADD FINAL DETAILS
            #print("STEP 5: Adding final details and props...")

            # Load remaining prop assets
            props = {
                "anvil": load_asset("Anvil"),
                "barrel": load_asset("Barrel"),
                "chest": load_asset("Chest"),
                "cauldron": load_asset("Cauldron"),
                "altar": load_asset("Altar"),
                "well": load_asset("Well"),
                "trolley": load_asset("Trolley"),
                "lamppost": load_asset("Lamppost"),
                "street_light": load_asset("street_light")
            }

            # Place a well in the town center
            place_actor(props["well"], town_center_x + 150 * scale_factor, town_center_y - 150 * scale_factor, 0, 0, name="Town_Center_Well")

            # Place streetlights around the town square
            square_size = 90 * scale_factor
            light_spacing = 30 * scale_factor
            for i in range(4):
                for j in range(3):
                    if j == 1:  # Skip middle to accommodate entrances
                        continue
                        
                    # Calculate position along each side of the square
                    if i == 0:  # North side
                        x = town_center_x - square_size/2 + j * light_spacing
                        y = town_center_y - square_size/2
                        rot = 0
                    elif i == 1:  # East side
                        x = town_center_x + square_size/2
                        y = town_center_y - square_size/2 + j * light_spacing
                        rot = 90
                    elif i == 2:  # South side
                        x = town_center_x + square_size/2 - j * light_spacing
                        y = town_center_y + square_size/2
                        rot = 180
                    else:  # West side
                        x = town_center_x - square_size/2
                        y = town_center_y + square_size/2 - j * light_spacing
                        rot = 270
                        
                    light_type = "lamppost" if j % 2 == 0 else "street_light"
                    place_actor(props[light_type], x, y, 0, rot, name=f"Square_Light_{i}_{j}")

            # Add props near the forge
            forge_x = town_center_x + 600 * scale_factor
            forge_y = town_center_y - 500 * scale_factor

            place_actor(props["anvil"], forge_x + 50 * scale_factor, forge_y - 80 * scale_factor, 0, 45, name="Forge_Anvil")
            place_actor(props["barrel"], forge_x - 70 * scale_factor, forge_y - 60 * scale_factor, 0, 0, name="Forge_Barrel")

            # Add barrels and chests around the tavern
            tavern_x = town_center_x + 700 * scale_factor
            tavern_y = town_center_y + 600 * scale_factor

            for i in range(3):
                x_offset = random.uniform(-150, 150) * scale_factor
                y_offset = random.uniform(-150, 150) * scale_factor
                rot = random.uniform(0, 360)
                place_actor(props["barrel"], tavern_x + x_offset, tavern_y + y_offset, 0, rot, name=f"Tavern_Barrel_{i}")

            place_actor(props["chest"], tavern_x - 100 * scale_factor, tavern_y + 120 * scale_factor, 0, 45, name="Tavern_Chest")

            # Add altar near the witch's house
            witch_house_x = town_center_x + 1800 * scale_factor
            witch_house_y = town_center_y + 1500 * scale_factor

            place_actor(props["altar"], witch_house_x + 150 * scale_factor, witch_house_y + 100 * scale_factor, 0, 215, name="Witch_Altar")
            place_actor(props["cauldron"], witch_house_x - 80 * scale_factor, witch_house_y + 120 * scale_factor, 0, 0, name="Witch_Cauldron")

            # Add a trolley near the mine
            mine_x = town_center_x - 2200 * scale_factor
            mine_y = town_center_y + 1800 * scale_factor

            place_actor(props["trolley"], mine_x + 150 * scale_factor, mine_y - 100 * scale_factor, 0, 45, name="Mine_Trolley")

            # Create a path connecting major locations with Tile assets
            tiles = {
                "tile_1": load_asset("Tile_1"),
                "tile_2": load_asset("Tile_2"),
                "tile_3": load_asset("Tile_3"),
                "tile_4": load_asset("Tile_4"),
                "tile_5": load_asset("Tile_5"),
                "tile_6": load_asset("Tile_6"),
                "tile_7": load_asset("Tile_7")
            }

            # Define major path points - scaled based on town size
            path_points = [
                (town_center_x, town_center_y),  # Town center
                (town_center_x + 700 * scale_factor, town_center_y + 600 * scale_factor),  # Tavern
                (town_center_x + 600 * scale_factor, town_center_y - 500 * scale_factor),  # Forge
                (town_center_x - 1000 * scale_factor, town_center_y - 1700 * scale_factor),  # Tower
                (town_center_x + 1700 * scale_factor, town_center_y - 1200 * scale_factor),  # Watermill
                (town_center_x - 1800 * scale_factor, town_center_y - 400 * scale_factor),  # Woodmill
                (town_center_x - 2200 * scale_factor, town_center_y + 1800 * scale_factor),  # Mine
                (town_center_x + 1800 * scale_factor, town_center_y + 1500 * scale_factor)  # Witch house
            ]

            # Create paths between points
            for i in range(len(path_points) - 1):
                start_x, start_y = path_points[i]
                end_x, end_y = path_points[i + 1]
                
                # Calculate distance and direction
                dx = end_x - start_x
                dy = end_y - start_y
                distance = math.sqrt(dx*dx + dy*dy)
                
                # Tile spacing - scale by the town size
                tile_spacing = 100 * scale_factor
                num_tiles = int(distance / tile_spacing)
                
                # Place tiles along the path
                for j in range(num_tiles):
                    t = j / num_tiles
                    x = start_x + t * dx
                    y = start_y + t * dy
                    
                    # Add some randomness to make the path look more natural
                    offset_x = random.uniform(-20, 20) * scale_factor
                    offset_y = random.uniform(-20, 20) * scale_factor
                    
                    # Random tile type
                    tile_type = random.choice(["tile_1", "tile_2", "tile_3", "tile_4", "tile_5", "tile_6", "tile_7"])
                    rot = random.uniform(0, 360)
                    
                    place_actor(tiles[tile_type], x + offset_x, y + offset_y, 1, rot, name=f"Path_Tile_{i}_{j}")

            #print("Final details added successfully")
            #print(f"Fantasy town creation complete at ({town_center_x}, {town_center_y}) with size {town_width}x{town_height}!")

            return json.dumps({
                "status": "success",
                "result": f"Successfully created fantasy town at ({town_center_x}, {town_center_y}) with size {town_width}x{town_height}."
            })

        except Exception as e:
            return json.dumps({ "status": "error", "message": f"Error building town: {str(e)}" })

    @staticmethod
    def execute_blueprint_function(blueprint_name, function_name, arguments = ""):
        """Execute a function in a Blueprint"""
        try:
            # Find the Blueprint asset
            blueprint_asset = unreal.find_asset(blueprint_name)
            if not blueprint_asset:
                return json.dumps({
                    "status": "error",
                    "message" : f"Blueprint '{blueprint_name}' not found"
                })

            # Get the blueprint class
            blueprint_class = unreal.load_class(blueprint_asset)
            if not blueprint_class:
                return json.dumps({
                    "status": "error",
                    "message": f"Could not load class from Blueprint '{blueprint_name}'"
                })

            # Get the CDO(Class Default Object)
            cdo = unreal.get_default_object(blueprint_class)

            # Parse arguments
            parsed_args = []
            if arguments:
                for arg in arguments.split(','):
                    arg = arg.strip()
                    # Try to determine argument type
                    if arg.lower() in ['true', 'false']:
                        parsed_args.append(arg.lower() == 'true')
                    elif arg.isdigit():
                        parsed_args.append(int(arg))
                    elif arg.replace('.', '', 1).isdigit():
                        parsed_args.append(float(arg))
                    else:
                        parsed_args.append(arg)

            # Call the function
            result = getattr(cdo, function_name)(*parsed_args)
            return json.dumps({
                "status": "success",
                "result" : f"Function '{function_name}' executed. Result: {result}"
            })
        except Exception as e:
            return json.dumps({ "status": "error", "message": str(e) })

    @staticmethod
    def execute_python(code):
        """Execute arbitrary Python code in Unreal Engine"""

        try:

            # decode and extract dict object
            #decoded = chunk.decode('utf-8', 'replace')
            #stripped = decoded.strip('\'"\n\r')
            replaced = code.replace('\\\\','\\').replace('\\"','"').replace("\\'","'").replace('f"\n', 'f"').replace('f\'\n', 'f\'').replace('\n"', '"').replace('\n\'', '\'')
            #response = json.loads(replaced)

            # save to file first
            #code_file = open('code.py', 'w')
            #code_file.write(code)
            #code_file.close()

            # read back
            #code_file = open('code.py', 'r')
            #code_text = code_file.read()
            #code_file.close()

            # check for syntax errors
            try:
                code_obj = compile(replaced, '<string>', 'exec')
                
            except SyntaxError as se:
                return json.dumps({
                    "status": "error",
                    "message" : f"Syntax Error executing Python code: {str(se)}",
                    "traceback" : traceback.format_exc()
                })
            except Exception as ex:
                return json.dumps({
                    "status": "error",
                    "message" : f"Error executing Python code: {str(ex)}",
                    "traceback" : traceback.format_exc()
                })

            try:

                # Create output capture file
                output_file = open('output.txt', 'w')
                error_file = open('error.txt', 'w')

                # Store original stdout and stderr
                original_stdout = sys.stdout
                original_stderr = sys.stderr

                # Redirect stdout and stderr
                sys.stdout = output_file
                sys.stderr = error_file

                # Create a local dictionary for execution
                locals_dict = { 'unreal': unreal, 'result': None }

                # Execute the compiled code
                exec(code_obj, globals(), locals_dict)

            except AttributeError as ae:
                return json.dumps({
                    "status": "error",
                    "message" : f"Attribute Error in code: {str(ae)}",
                    "traceback" : traceback.format_exc()
                })

            except Exception as ee:
                return json.dumps({
                    "status": "error",
                    "message" : f"Python exec() error: {str(ee)}",
                    "traceback" : traceback.format_exc()
                })

            finally:
                
                # close output
                output_file.close()
                error_file.close()

                # Restore original stdout and stderr
                sys.stdout = original_stdout
                sys.stderr = original_stderr

            # read output
            read_output = open('output.txt', 'r')
            result = read_output.read()
            read_output.close()

            # read errors
            read_error = open('error.txt', 'r')
            error_text = read_error.read()
            read_error.close()

            # Return the result if it was set
            if 'result' in locals_dict and locals_dict['result'] is not None:
                return json.dumps({
                    "status": "success",
                    "result" : str(locals_dict['result'])
                })
            elif error_text and len(error_text) > 0:
                return json.dumps({
                    "status": "error",
                    "result": error_text
                })
            elif not result:
                return json.dumps({
                    "status": "error",
                    "result": "Python code did not execute Successfully. No result set."
                })
            else:
                return json.dumps({
                    "status": "success",
                    "result": str(result)
                })

        except Exception as exc:

            # read error
            error_msg = str(exc)
            read_error = open('error.txt', 'r')
            if read_error:
                error_msg = read_error.read()
                read_error.close()
            return json.dumps({
                "status": "error",
                "message" : f"Python exec() error: {error_msg}",
                "traceback" : traceback.format_exc()
            })

# Register the bridge as a global variable
mcp_bridge = MCPUnrealBridge()
