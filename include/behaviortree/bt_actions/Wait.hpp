#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class Wait : public UWRTActionNode {
    public:
    Wait(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTActionNode(name, config), goalDur(0) { }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("seconds", PORT_REQUIRED,
                "Seconds to wait while spinning ROS node")
        };
    }


    /**
     * @brief Called when the node runs for the first time. If it returns RUNNING, node becomes async
     * @return NodeStatus status of the node after execution
     */
    BT::NodeStatus onStart() override {
        startTime = std::chrono::system_clock::now();
        goalDur = tryGetRequiredInput<double>("seconds", 0);
        return BT::NodeStatus::RUNNING;
    }

    /**
     * @brief Called periodically while the node status is RUNNING
     * @return NodeStatus The node status after 
     */
    BT::NodeStatus onRunning() override {
        auto timeElapsed = std::chrono::system_clock::now() - startTime;
        return (std::chrono::duration_cast<std::chrono::milliseconds>(timeElapsed).count() >= goalDur * 1000 ? 
                    BT::NodeStatus::SUCCESS : BT::NodeStatus::RUNNING);
    }

    /**
     * @brief Called when the node is halted.
     */
    void onHalted() override {

    }

    private:
    std::chrono::time_point<std::chrono::system_clock> startTime;
    double goalDur;
};
