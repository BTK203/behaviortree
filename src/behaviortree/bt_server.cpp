#include <rclcpp/rclcpp.hpp>
#include <rclcpp/executors/multi_threaded_executor.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>

#include <std_msgs/msg/bool.hpp>

#include <behaviortree/action/execute_tree.hpp>
#include <behaviortree/srv/list_trees.hpp>

#include <vector>
#include <chrono>
#include <filesystem>

#include <unistd.h>

#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"
#include "behaviortree/UWRTLogger.hpp"

/**
 * ROS2 action server that runs behavior trees.
 * Call autonomy/run_tree with the behaviortree/msg/RunTree command
 * Can use the autonomy/list_trees service to list out trees in the package
 */

#define AUTONOMY_TREE_DIR \
    std::string(__FILE__).substr(0, std::string(__FILE__).find("/behaviortree/")) + std::string("/behaviortree/trees")

using namespace BT;
using namespace std::chrono_literals;

namespace behaviortree
{
    using namespace std::placeholders;
    using ExecuteTree = behaviortree::action::ExecuteTree;
    using GoalHandleExecuteTree = rclcpp_action::ServerGoalHandle<ExecuteTree>;
    using ListTrees = behaviortree::srv::ListTrees;

    class BTServer : public rclcpp::Node
    {
    public:
        BTServer() : Node("autonomy")
        {
            // make an action server for running the autonomy trees
            actionServer = rclcpp_action::create_server<ExecuteTree>(
                this,
                "autonomy/run_tree",
                std::bind(&BTServer::handleGoal, this, _1, _2),
                std::bind(&BTServer::handleCancel, this, _1),
                std::bind(&BTServer::handleAccepted, this, _1));

            // make a service for listing all of the trees loaded / availiable
            listTreeServer = create_service<ListTrees>(
                "autonomy/list_trees",
                std::bind(&BTServer::handleService, this, _1, _2));        

            // create the behavior tree factory context
            factory = std::make_shared<BehaviorTreeFactory>();

            // load our plugins from ament index
            registerPluginsForFactory(factory, AUTONOMY_PKG_NAME);

            // load other plugins from the parameter server
            for (auto plugin : pluginPaths)
            {
                try{
                    RCLCPP_INFO_STREAM(this->get_logger(), "Registering additional plugin: " << plugin);
                    factory->registerFromPlugin(plugin);
                }
                catch(BT::RuntimeError & e){
                    RCLCPP_ERROR_STREAM(get_logger(), "Could not load plugin: " << e.what());
                }
                
            }

            // automatically add package and the ament index dir
            treeDirs.push_back(AUTONOMY_TREE_DIR);
            treeDirs.push_back(ament_index_cpp::get_package_share_directory(AUTONOMY_PKG_NAME) + "/trees");
        }

        rclcpp_action::GoalResponse handleGoal(
            const rclcpp_action::GoalUUID &uuid,
            std::shared_ptr<const ExecuteTree::Goal> goal)
        {
            RCLCPP_INFO(this->get_logger(), "Received goal request with tree name %s", goal->tree.c_str());
            (void)uuid;

            // test if the tree is running
            if (executionThread.joinable())
            {
                // tree is running, so we cannot accept another
                return rclcpp_action::GoalResponse::REJECT;
            }

            // test if the tree exists
            if(!std::filesystem::exists(goal->tree)) {
                RCLCPP_ERROR(get_logger(), "Rejecting request to run tree %s because the file does not exist.", goal->tree.c_str());
                return rclcpp_action::GoalResponse::REJECT;
            }

            return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
        }

        rclcpp_action::CancelResponse handleCancel(
            const std::shared_ptr<GoalHandleExecuteTree> goal_handle)
        {
            RCLCPP_INFO(this->get_logger(), "Received request to cancel BehaviorTree");
            (void)goal_handle;
            return rclcpp_action::CancelResponse::ACCEPT;
        }

        void handleAccepted(const std::shared_ptr<GoalHandleExecuteTree> goal_handle)
        {
            // this needs to return quickly to avoid blocking the executor, so spin up a new thread
            executionThread = std::thread{std::bind(&BTServer::execute, this, _1), goal_handle};
            executionThread.detach();
        }

        void execute(const std::shared_ptr<GoalHandleExecuteTree> goal_handle)
        {
            // prepare the result message
            ExecuteTree::Result::SharedPtr result = std::make_shared<ExecuteTree::Result>();
            treeRunning = true;

            try
            {
                // load the tree file contents in to a BT context
                Tree tree = factory->createTreeFromFile(goal_handle->get_goal()->tree);
                initRosForTree(tree, this->shared_from_this());

                // set up idle sleep rate
                rclcpp::Rate loop_rate(30ms);

                // start ticking the tree with feedback
                // keep executing tick until it returns either SUCCESS or FAILURE
                auto tickStatus = NodeStatus::RUNNING;
                while (!BT::isStatusCompleted(tickStatus))
                {
                    // always gets ticked once
                    tickStatus = tree.tickOnce();

                    // check for a cancel
                    if (goal_handle->is_canceling())
                    {
                        result->returncode = 0;
                        tree.haltTree();
                        treeRunning = false;
                        RCLCPP_INFO(get_logger(), "DoTask: Canceled current action goal");
                        break;
                    }

                    // sleep a bit while we wait
                    loop_rate.sleep();
                }

                std::string resultStr = "SUCCESS";
                switch(tickStatus) {
                    case NodeStatus::FAILURE:
                        resultStr = "FAILURE";
                        break;
                    case NodeStatus::IDLE:
                        resultStr = "IDLE";
                        break;
                    case NodeStatus::RUNNING:
                        resultStr = "RUNNING";
                        break;
                    default:
                        resultStr = "SUCCESS";
                        break;
                }

                RCLCPP_INFO(get_logger(), "Tree ended with status %s", resultStr.c_str());
                treeRunning = false;

                // wrap this party up and finish execution
                result->returncode = (int) tickStatus;
                if(tickStatus == BT::NodeStatus::SUCCESS || tickStatus == BT::NodeStatus::FAILURE)
                {
                    //tree finished on its own
                    goal_handle->succeed(result);
                } else
                {
                    //tree canceled
                    if(goal_handle->is_canceling())
                    {
                        goal_handle->canceled(result);
                    } else
                    {
                        goal_handle->abort(result);
                    }
                }

                // bail early, all other code is error checking
                return;
            }
            catch (const std::exception &e)
            {
                RCLCPP_ERROR_STREAM(get_logger(), "Error occurred while ticking tree. Aborting tree! Error: " << e.what());
            }
            catch (...)
            {
                RCLCPP_ERROR(get_logger(), "Unknown error while ticking tree. Aborting tree!");
            }

            treeRunning = false;

            // ...abort
            result->returncode = -1;
            goal_handle->abort(result);
        }

        void handleService(const ListTrees::Request::SharedPtr request,
                           ListTrees::Response::SharedPtr response)
        {
            (void)request; // empty request

            std::vector<std::string> treeFiles;

            // iterate all the search dirs
            for (auto dir : treeDirs)
            {
                // iterate the files in the search dirs and test them to see if they are xml
                for (const auto &entry : std::filesystem::directory_iterator(dir))
                {
                    std::string file = std::string(entry.path());
                    if (file.find(".xml") != std::string::npos)
                        treeFiles.push_back(file);
                }
            }

            // hand off the listing of possible files
            response->trees = treeFiles;
        }

        void killCb(const std_msgs::msg::Bool::SharedPtr msg) {
            robotKilled = msg->data;
        }

    private:        
        bool 
            treeRunning,
            robotKilled;

        // ros action and service servers
        rclcpp_action::Server<ExecuteTree>::SharedPtr actionServer;
        rclcpp::Service<ListTrees>::SharedPtr listTreeServer;

        // execution context thread for the action server
        std::thread executionThread;

        // behavior tree factory context
        std::shared_ptr<BehaviorTreeFactory> factory;

        // full tree file path vector to load
        std::vector<std::string> 
            treeDirs,
            pluginPaths;
    };
} // namespace behaviortree

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);

    // create our node context
    auto node = std::make_shared<behaviortree::BTServer>();

    //print tree directory
    std::string treeDir = AUTONOMY_TREE_DIR;
    RCLCPP_INFO(node->get_logger(), "Using tree directory at %s", treeDir.c_str());

    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();

    rclcpp::shutdown();
}
