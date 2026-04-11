#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class Error : public UWRTActionNode {
    public:
    Error(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTActionNode(name, config) {
        
    }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("message", UwrtPortNecessity::PORT_REQUIRED, "Message to print")
        };
    }

    /**
     * @brief Called when the node runs for the first time. If it returns RUNNING, node becomes async
     * @return NodeStatus status of the node after execution
     */
    BT::NodeStatus onStart() override {
        std::string message = tryGetRequiredInput<std::string>("message", "");
        getLogger()->error(formatStringWithBlackboard(message)); 
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
    void onHalted() override {

    }
};
