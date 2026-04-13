# cmake macro to register behaviortree plugins to be automatically discoverered by the runners
macro(register_behaviortree_plugins)
  ament_index_register_resource(behaviortree CONTENT ${ARGN})
endmacro()
