#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class SetParameter : public UWRTActionNode {
    public:
    SetParameter(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTActionNode(name, config) { }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("key", UwrtPortNecessity::PORT_REQUIRED, "name of the parameter to modify"),
            UwrtInput("value", UwrtPortNecessity::PORT_REQUIRED, "new value of the parameter")
        };
    }

    /**
     * @brief Called when the node runs for the first time. If it returns RUNNING, node becomes async
     * @return NodeStatus status of the node after execution
     */
    BT::NodeStatus onStart() override {
        std::string
            key = tryGetRequiredInput<std::string>("key", ""),
            value = tryGetRequiredInput<std::string>("value", "");

        if(key.empty())
        {
            getLogger()->error("cannnot set parameter because no key was specified");
            return BT::NodeStatus::FAILURE;
        }

        if(value.empty())
        {
            getLogger()->error("cannot set parameter " + key + " because no value was specified");
            return BT::NodeStatus::FAILURE;
        }

        bool success = false;

        if(!TreeParameterStore::hasParameter(key))
        {
            success = TreeParameterStore::addParameter(key, "");
            if(!success)
            {
                getLogger()->error("Failed to add parameter with key " + key);
                return BT::NodeStatus::FAILURE;
            }
        }

        std::string err;
        success = TreeParameterStore::updateParameter(key, value, err);
        if(!success)
        {
            getLogger()->error("Failed to set parameter with key " + key + " to value " + value + ": " + err);
            return BT::NodeStatus::FAILURE;
        }

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
