import os
import launch
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration as LC
from ament_index_python import get_package_share_directory

def generate_launch_description():
    default_bt_config = os.path.join(
        get_package_share_directory("behaviortree"),
        "config",
        "btserver_default.yaml")
    
    return launch.LaunchDescription([
        DeclareLaunchArgument("btserver_config", default_value=default_bt_config, 
                            description="path to the bt_server parameter file"),
        
        Node(
            package="behaviortree",
            executable="bt_server",
            output="screen",
            parameters=[LC("btserver_config")]
        )
    ])
