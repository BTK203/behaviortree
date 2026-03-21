#pragma once

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <rqt_generic/rqt_generic.hpp>

#include <ui_behaviortree_panel.h>
#include <QTimer>
#include <QStandardItemModel>

#include "behaviortree/action/execute_tree.hpp"
#include "behaviortree/srv/list_trees.hpp"
#include "behaviortree/msg/tree_stack.hpp"


namespace behaviortree_rqt
{
    using ExecuteTree = behaviortree::action::ExecuteTree;
    using GHExecuteTree = rclcpp_action::ClientGoalHandle<ExecuteTree>;

    enum BTPanelStatusColor
    {
        BTPS_BLACK,
        BTPS_RED
    };

    enum BTPanelButtonStates
    {
        BTPBS_NONE_ENABLED,
        BTPBS_START_ENABLED,
        BTPBS_STOP_ENABLED,
        BTPBS_BOTH_ENABLED
    };

    enum BTPanelDropdownState
    {
        BTPDS_DISABLED,
        BTPDS_ENABLED
    };

    class BehaviortreePanel
        : public rqt_generic::Panel
    {

        Q_OBJECT

    public:
        BehaviortreePanel();
        virtual void initPlugin(qt_gui_cpp::PluginContext &context);
        virtual void shutdownPlugin();

    protected:
        void initWithSettings(std::map<QString, QVariant>& plugin_settings, std::map<QString, QVariant>& instance_settings) override;
        
    private slots:
        void selectTree(int treeIdx);
        void startTask();
        void cancelTask();
        void refresh();

    private:
        void taskStartCb(const GHExecuteTree::SharedPtr &goalHandle);
        void taskCompleteCb(const GHExecuteTree::WrappedResult &result);
        void taskFeedbackCb(GHExecuteTree::SharedPtr goalHandle, ExecuteTree::Feedback::ConstSharedPtr feedback);
        void cancelAccept(const action_msgs::srv::CancelGoal::Response::SharedPtr resp);
        void stackCb(const behaviortree::msg::TreeStack & msg);
        void updateStack(const std::vector<std::string>& stack);
        void waitForRefresh();
        void timerCb();

        // QT
        Ui::BehaviortreePanel ui_;
        QWidget *widget_;
        QStandardItemModel *stackModel_;
        QTimer *timer_;
        int timeoutMs_;

        // book-keeping
        std::vector<std::string>
            treeList,
            treeStack;

        BTPanelStatusColor statusColor;
        std::string statusText;

        BTPanelButtonStates buttonStates;
        BTPanelDropdownState dropdownState;

        // ros
        rclcpp::Subscription<behaviortree::msg::TreeStack>::SharedPtr stackSub;

        rclcpp::Client<behaviortree::srv::ListTrees>::SharedPtr refreshClient;
        std::shared_future<std::shared_ptr<behaviortree::srv::ListTrees_Response>> refreshFuture;
        int64_t refreshFutureid = -1;
        int timerTick = -1;

        rclcpp_action::Client<ExecuteTree>::SharedPtr actionClient;
    };
}
