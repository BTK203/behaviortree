#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class IsTrue : public UWRTConditionNode {
    public:
    IsTrue(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTConditionNode(name, config) {
        
    }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("value", PORT_REQUIRED,
                "Boolean; 1 for true, 0 for false")
        };
    }


    /**
     * @brief Executes the node.
     * This method will be called once by the tree and can block for as long
     * as it needs for the action to be completed. When execution completes,
     * this method must return either SUCCESS or FAILURE; it CANNOT return
     * IDLE or RUNNING.
     *
     * @return NodeStatus The result of the execution; SUCCESS or FAILURE.
     */
    BT::NodeStatus tick() override {
        bool value = tryGetRequiredInput<bool>("value", 0);
        return (value ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE);
    }
};

