#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class RunRegisteredTree : public UwrtRosEnabledActionNode {
    public:
    RunRegisteredTree(const std::string& name, const BT::NodeConfiguration& config)
    : UwrtRosEnabledActionNode(name, config) { }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("tree", UwrtPortNecessity::PORT_REQUIRED, "Name of the tree to run")
        };
    }

    void rosInit()
    { }

    /**
     * @brief Called when the node runs for the first time. If it returns RUNNING, node becomes async
     * @return NodeStatus status of the node after execution
     */
    BT::NodeStatus onStart() override {
        std::string name = tryGetRequiredInput<std::string>("tree", "");
        if(name.empty())
        {
            getLogger()->error("No tree name specified to RunRegisteredTree");
            return BT::NodeStatus::FAILURE;
        }

        std::vector<std::string> registeredTrees = TreeFactoryStore::getFactory()->registeredBehaviorTrees();
        if(std::find(registeredTrees.begin(), registeredTrees.end(), name) == registeredTrees.end())
        {
            getLogger()->error("No tree with name " + name + " for RunRegisteredTree");
            return BT::NodeStatus::FAILURE;
        }

        tree = TreeFactoryStore::getFactory()->createTree(name);
        initRosForTree(tree, rosNode());
        return BT::NodeStatus::RUNNING;
    }

    /**
     * @brief Called periodically while the node status is RUNNING
     * @return NodeStatus The node status after 
     */
    BT::NodeStatus onRunning() override {
        return tree.tickOnce();
    }

    /**
     * @brief Called when the node is halted.
     */
    void onHalted() override {
        if(status() == BT::NodeStatus::RUNNING)
        {
            tree.haltTree();
        }
    }

    private:
    BT::Tree tree;
};
