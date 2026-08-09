#pragma once

#include "behaviortree/msg/tree_parameter.hpp"

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class GetParameter : public UWRTActionNode {
    public:
    GetParameter(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTActionNode(name, config) {
        
    }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("key", UwrtPortNecessity::PORT_REQUIRED, "name of the parameter to get"),
            UwrtOutput("value", "value of the parameter")
        };
    }

    /**
     * @brief Called when the node runs for the first time. If it returns RUNNING, node becomes async
     * @return NodeStatus status of the node after execution
     */
    BT::NodeStatus onStart() override {
        // get key
        std::string key = tryGetRequiredInput<std::string>("key", "");
        if(key.empty())
        {
            getLogger()->error("Cannot get parameter because no value was specified as the key");
            return BT::NodeStatus::FAILURE;
        }

        if(!TreeParameterStore::hasParameter(key))
        {
            getLogger()->error("Cannot get parameter \"" + key + "\" because it does not exist");
            return BT::NodeStatus::FAILURE;
        }

        postOutput<std::string>("value", TreeParameterStore::getParameter(key));
        return BT::NodeStatus::SUCCESS;
    }

    /**
     * @brief Called periodically while the node status is RUNNING
     * @return NodeStatus The node status after 
     */
    BT::NodeStatus onRunning() override {
        return BT::NodeStatus::SUCCESS;
    }

    /**
     * @brief Called when the node is halted.
     */
    void onHalted() override { }
};
