#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class LoadTree : public UWRTActionNode {
    public:
    LoadTree(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTActionNode(name, config) { }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("file", UwrtPortNecessity::PORT_REQUIRED, "full path of the file to load")
        };
    }

    /**
     * @brief Called when the node runs for the first time. If it returns RUNNING, node becomes async
     * @return NodeStatus status of the node after execution
     */
    BT::NodeStatus onStart() override {
        std::string file = tryGetRequiredInput<std::string>("file", "");
        if(file.empty())
        {
            getLogger()->error("Could not load another file because none was specified");
            return BT::NodeStatus::FAILURE;
        }

        std::string resolved = resolvePackageUri(file);

        getLogger()->info("Loading additional behavior tree file " + resolved);
        TreeFactoryStore::getFactory()->registerBehaviorTreeFromFile(resolved);
        getLogger()->info("Additional tree file " + resolved + " was successfully loaded");
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
