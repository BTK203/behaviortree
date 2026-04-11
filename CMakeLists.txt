cmake_minimum_required(VERSION 3.8)
project(behaviortree)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rclpy REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(behaviortree_cpp REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(rqt_generic QUIET)

#
# FUNCTIONS
# 

# cmake function to execute a process and quit cmake execution if it fails
function(execute_process_with_error_detection)
  execute_process(COMMAND ${ARGV}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE func_out
    RESULT_VARIABLE func_result
  )

  if(${func_result}) #nonzero function result means error
    message(SEND_ERROR 
      "Error occurred during execution of process: ${cmd}\n\n"
      "Stderr shown above. Stdout shown below: \n" #stderr automatically printed by cmake/colcon, stdout has to be manually printed
      "${func_output}\n")
    message(FATAL_ERROR "Review error information above. Cmake will exit.") #will quit cmake execution
  endif()
endfunction()

# cmake function to register behaviortree plugins to be automatically discoverered by the runners
function(register_behaviortree_plugins)
  ament_index_register_resource(behaviortree CONTENT ${ARGN})
endfunction()

#create directory for auto-generated code which will include headers for custom nodes as well as source code that registers custom nodes
make_directory(${PROJECT_BINARY_DIR}/autonomy_generator)

# generate behaviortree node registrators
execute_process_with_error_detection(
  ./src/behaviortree/assistant/btassistant.py generate_registrators ${PROJECT_BINARY_DIR}/autonomy_generator
)

set(deps 
  rclcpp
  geometry_msgs
  std_msgs
  nav_msgs
  sensor_msgs
  std_srvs
  behaviortree_cpp
  tf2
  tf2_ros
  tf2_geometry_msgs
)

#
# Action message type
#

rosidl_generate_interfaces(${PROJECT_NAME}
  action/ExecuteTree.action
  srv/ListTrees.srv
  msg/TreeStack.msg)

ament_export_dependencies(rosidl_default_runtime)

# handle for local linking to rosidl targets
rosidl_get_typesupport_target(behaviortree_rosidl_target
  ${PROJECT_NAME} "rosidl_typesupport_cpp")

#
# util library
#
add_library(autonomy_utils SHARED
  src/behaviortree/util.cpp
  src/behaviortree/logging.cpp
  src/behaviortree/ROSEnabledNode.cpp
)

target_include_directories(autonomy_utils PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

ament_target_dependencies(autonomy_utils ${deps})

ament_export_targets(export_autonomy_utils)
install(TARGETS autonomy_utils
  EXPORT export_autonomy_utils
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

#
# action library
#
add_library(autonomy_actions SHARED
  ${PROJECT_BINARY_DIR}/autonomy_generator/registerActions.cpp
)

target_include_directories(autonomy_actions PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

ament_target_dependencies(autonomy_actions ${deps})
target_link_libraries(autonomy_actions autonomy_utils)

target_compile_definitions(autonomy_actions PRIVATE BT_PLUGIN_EXPORT)

install(TARGETS autonomy_actions
  DESTINATION lib)

#
# condition library
#
add_library(autonomy_conditions SHARED
  ${PROJECT_BINARY_DIR}/autonomy_generator/registerConditions.cpp
)

target_compile_definitions(autonomy_conditions PRIVATE BT_PLUGIN_EXPORT)
target_include_directories(autonomy_conditions PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

ament_target_dependencies(autonomy_conditions ${deps})
target_link_libraries(autonomy_conditions autonomy_utils)

install(TARGETS autonomy_conditions
  DESTINATION lib)

#
# decorator library
#
add_library(autonomy_decorators SHARED
  ${PROJECT_BINARY_DIR}/autonomy_generator/registerDecorators.cpp
)

target_compile_definitions(autonomy_decorators PRIVATE BT_PLUGIN_EXPORT)
target_include_directories(autonomy_decorators PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

ament_target_dependencies(autonomy_decorators ${deps})
target_link_libraries(autonomy_decorators autonomy_utils) 

install(TARGETS autonomy_decorators
  DESTINATION lib)

#
# Health
#

# library
file(GLOB health_sources RELATIVE ${PROJECT_SOURCE_DIR} src/behaviortree/health/*.cpp)
add_library(behaviortree_health
  ${health_sources})

target_include_directories(behaviortree_health PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_link_libraries(behaviortree_health autonomy_utils)

# bt checker executable
add_executable(check_bt src/behaviortree/check_bt.cpp)
target_link_libraries(check_bt autonomy_utils behaviortree_health)

install(TARGETS behaviortree_health
  DESTINATION lib/)

install(TARGETS check_bt
  DESTINATION lib/${PROJECT_NAME})


#
# Tree server node
#

add_library(bt_server_component SHARED src/behaviortree/bt_server.cpp)

target_include_directories(bt_server_component PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

ament_target_dependencies(bt_server_component
  ament_index_cpp
  rclcpp
  rclcpp_action
  rclcpp_components
  behaviortree_cpp)

target_link_libraries(bt_server_component autonomy_utils ${behaviortree_rosidl_target})

rclcpp_components_register_node(
  bt_server_component
  PLUGIN "behaviortree::BTServer"
  EXECUTABLE bt_server
)

ament_export_targets(export_bt_server_component)
install(TARGETS bt_server_component
        EXPORT export_bt_server_component
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

#
# Tree command line executable
#
add_executable(bt src/behaviortree/behaviortree.cpp)

target_include_directories(bt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(bt autonomy_utils ${behaviortree_rosidl_target})
target_compile_definitions(bt PUBLIC AUTONOMY_PKG_NAME="${PROJECT_NAME}")

ament_target_dependencies(bt
  ament_index_cpp
  rclcpp
  behaviortree_cpp
)

install(TARGETS bt
  DESTINATION lib/${PROJECT_NAME}
)

#
# rqt plugin (if able)
#

if(rqt_generic_FOUND)
  find_package(rqt_gui_cpp REQUIRED)
  find_package(qt_gui_cpp REQUIRED)
  find_package(Qt5Widgets REQUIRED)

  set(CMAKE_INCLUDE_CURRENT_DIR ON)

  set(btpanel_hdrs
    include/behaviortree/behaviortree_panel.hpp)
  set(btpanel_uis
    src/behaviortree/behaviortree_panel.ui)
  set(btpanel_srcs
    src/behaviortree/behaviortree_panel.cpp)
  
  qt5_wrap_cpp(btpanel_moc ${btpanel_hdrs})
  qt5_wrap_ui(btpanel_ui ${btpanel_uis})

  add_library(behaviortree_rqt SHARED
    ${btpanel_srcs}
    ${btpanel_moc}
    ${btpanel_ui}
  )

  target_include_directories(behaviortree_rqt PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")

  ament_target_dependencies(behaviortree_rqt
    rclcpp
    rclcpp_action
    rqt_generic
    qt_gui_cpp
    rqt_gui_cpp)

  target_link_libraries(behaviortree_rqt
    ${behaviortree_rosidl_target}
    Qt5::Widgets)

  install(
    TARGETS behaviortree_rqt
    EXPORT behaviortree_rqt
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

  install(DIRECTORY resource
    DESTINATION share/${PROJECT_NAME})
  
  pluginlib_export_plugin_description_file(rqt_gui plugin.xml)
  ament_export_libraries(behaviortree_rqt)
  ament_export_targets(behaviortree_rqt)
  ament_export_include_directories("include/${PROJECT_NAME}")
endif()

#
# other installs
#

# register plugins
register_behaviortree_plugins(autonomy_actions autonomy_conditions autonomy_decorators)

# install the launch files
install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

# Install Python executables
file(GLOB assistant_py_files RELATIVE ${PROJECT_SOURCE_DIR} src/behaviortree/assistant/*.py)
install(PROGRAMS
  ${assistant_py_files}
  DESTINATION lib/${PROJECT_NAME}
)

#install bt assistant "actions" directory
install(DIRECTORY src/behaviortree/assistant/actions
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY trees
  DESTINATION share/${PROJECT_NAME})

if(AUTONOMY_BUILD_TESTS)
  #set up GTest
  find_package(ament_cmake_gtest QUIET)

  if(ament_cmake_gtest_FOUND)
    #ament lint auto things
    find_package(ament_lint_auto REQUIRED)
    # the following line skips the linter which checks for copyrights
    # uncomment the line when a copyright and license is not present in all source files
    set(ament_cmake_copyright_FOUND TRUE)
    # the following line skips cpplint (only works in a git repo)
    # uncomment the line when this package is not in a git repo
    #set(ament_cmake_cpplint_FOUND TRUE)
    ament_lint_auto_find_test_dependencies()

    #
    # node tester
    #

    #glob all test sources together
    file(GLOB action_test_sources RELATIVE ${PROJECT_SOURCE_DIR} test/behaviortree/bt_actions/*.cpp)
    file(GLOB condition_test_sources RELATIVE ${PROJECT_SOURCE_DIR} test/behaviortree/bt_conditions/*.cpp)
    file(GLOB decorator_test_sources RELATIVE ${PROJECT_SOURCE_DIR} test/behaviortree/bt_decorators/*.cpp)
    file(GLOB tool_test_sources RELATIVE ${PROJECT_SOURCE_DIR} test/behaviortree/tools/*.cpp)

    ament_add_gtest(test_nodes
      ${action_test_sources}
      ${condition_test_sources}
      ${decorator_test_sources}
      ${tool_test_sources}
      test/behaviortree/DummyActionNode.cpp
      test/behaviortree/BtTestTool.cpp
      test/behaviortree/BtTest.cpp
      test/behaviortree/execute_tests.cpp #overrides default main() method
    )

    ament_target_dependencies(test_nodes ${deps})
    target_link_libraries(test_nodes autonomy_utils)

    target_include_directories(test_nodes PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>)
    
    install(TARGETS test_nodes
      DESTINATION lib/${PROJECT_NAME})

    #
    # health utilities tester
    #
    file(GLOB health_test_sources RELATIVE ${PROJECT_SOURCE_DIR} test/behaviortree/health/*.cpp)
    ament_add_gtest(test_health_utils
      ${health_sources}
      ${health_test_sources})

    target_link_libraries(test_health_utils autonomy_utils behaviortree_health)

    target_include_directories(test_health_utils PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>)

    ament_target_dependencies(test_health_utils
      ${deps})
    
    #executable
    install(TARGETS test_health_utils
      DESTINATION lib/${PROJECT_NAME})
    
    #test trees
    install(DIRECTORY test/behaviortree/health/health_test_trees
      DESTINATION share/${PROJECT_NAME})
  endif()
endif()

ament_package()
