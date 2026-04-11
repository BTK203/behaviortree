#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class RetryUntilSuccessfulOrTimeout : public UWRTDecoratorNode {
    public:
    RetryUntilSuccessfulOrTimeout(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTDecoratorNode(name, config) {
        duration = 0;
    }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("num_seconds", PORT_REQUIRED,
                "Number of seconds to tick child before returning FAILURE")
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
        if(status() == BT::NodeStatus::IDLE) {
            startTime = std::chrono::system_clock::now();
            duration = tryGetRequiredInput<double>("num_seconds", 0);
        }
        
        int msElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
        if(msElapsed < duration * 1000) {
            //have not run duration yet. either succeed or retry
            BT::NodeStatus result = child()->executeTick();
            if(result == BT::NodeStatus::SUCCESS) {
                return BT::NodeStatus::SUCCESS;
            }

            return BT::NodeStatus::RUNNING; //we will get ticked again
        } 
        
        //timeElapsed > duration
        getLogger()->error("RetryUntilSuccessfulOrTimeout named \"" + this->name() = "\" timed out.");
        return BT::NodeStatus::FAILURE;
    }

    private:
    double duration;
    std::chrono::time_point<std::chrono::system_clock> startTime;
};

