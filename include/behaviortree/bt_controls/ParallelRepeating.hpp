#pragma once

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

class ParallelRepeating : public UWRTControlNode {
    public:
    ParallelRepeating(const std::string& name, const BT::NodeConfiguration& config)
    : UWRTControlNode(name, config) { }

    /**
     * @brief Declares ports needed by this node.
     * @return PortsList Needed ports.
     */
    static UwrtPortInformation portInformation() {
        return {
            UwrtInput("num_successes", PORT_REQUIRED, "Number of child node successes needed for ParallelRepeating to succeed"),
            UwrtInput("num_failures", PORT_REQUIRED, "Number of child node failures needed for ParallelRepeating to fail")
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
        std::vector<BT::TreeNode *> cs = children();

        if(status() == BT::NodeStatus::IDLE)
        {
            childStatuses_.clear();
            childStatuses_.resize(cs.size());

            successThresh = tryGetRequiredInput<int>("num_successes", 0);
            failThresh = tryGetRequiredInput<int>("num_failures", 0);

            if(successThresh < 0)
            {
                successThresh = children().size();
            }

            if(failThresh < 0)
            {
                failThresh = children().size();
            }
        }

        int
            numSuccess = 0,
            numFail = 0;

        for(size_t i = 0; i < cs.size(); i++)
        {
            // ...always execute the tick
            BT::NodeStatus stat = cs.at(i)->executeTick();

            // ...but only store the result if it is completed
            // the node may be interrupted early if all nodes succeed
            if(BT::isStatusCompleted(stat))
            {
                childStatuses_.at(i) = stat;
                switch(stat)
                {
                    case BT::NodeStatus::SUCCESS:
                        numSuccess++;
                        break;
                    case BT::NodeStatus::FAILURE:
                        numFail++;
                        break;
                    default:
                        break;
                }
            }
        }

        if(numSuccess >= successThresh)
        {
            haltChildren();
            return BT::NodeStatus::SUCCESS;
        }

        if(numSuccess >= failThresh)
        {
            haltChildren();
            return BT::NodeStatus::FAILURE;
        }

        return BT::NodeStatus::RUNNING;
    }

    private:
    std::vector<BT::NodeStatus> childStatuses_;
    int successThresh;
    int failThresh;
};
