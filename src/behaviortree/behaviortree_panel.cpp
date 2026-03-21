#include "behaviortree/behaviortree_panel.hpp"

using namespace std::chrono_literals;
using namespace std::placeholders;

namespace behaviortree_rqt
{
    BehaviortreePanel::BehaviortreePanel()
    : rqt_generic::Panel(), 
      widget_(0),
      timer_(0),
      statusColor(BTPS_BLACK),
      statusText("Ready"),
      buttonStates(BTPBS_NONE_ENABLED),
      dropdownState(BTPDS_ENABLED)
    {
        setObjectName("BehaviortreePanel");
    }


    void BehaviortreePanel::initPlugin(qt_gui_cpp::PluginContext &context)
    {
        widget_ = new QWidget();
        ui_.setupUi(widget_);

        // create the item model and attach it to the panel
        stackModel_ = new QStandardItemModel();
        ui_.btStackView->setModel(stackModel_);

        if (context.serialNumber() > 1)
        {
            widget_->setWindowTitle(widget_->windowTitle() + " (" + QString::number(context.serialNumber()) + ")");
        }

        context.addWidget(widget_);

        // UI connections
        connect(ui_.settings, &QPushButton::clicked, this, &rqt_generic::Panel::editInstanceSettings);
        connect(ui_.btSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &BehaviortreePanel::selectTree);
        connect(ui_.btStart, &QPushButton::clicked, this, &BehaviortreePanel::startTask);
        connect(ui_.btStop, &QPushButton::clicked, this, &BehaviortreePanel::cancelTask);
        connect(ui_.btRefresh, &QPushButton::clicked, this, &BehaviortreePanel::refresh);
    }


    void BehaviortreePanel::shutdownPlugin()
    {
        delete timer_;
        delete stackModel_;
    }


    void BehaviortreePanel::initWithSettings(std::map<QString, QVariant>& plugin_settings, std::map<QString, QVariant>& instance_settings)
    {
        lock_.lock();

        // plugin settings
        timeoutMs_ = getOrDefault(plugin_settings, "Data Timeout MS", 100).toInt();
        int dataIntervalMs = getOrDefault(plugin_settings, "Update Interval MS", 200).toInt();

        // instance settings
        QString name = getOrDefault(instance_settings, "Name", "Behaviortree").toString();
        ui_.name->setText(name);

        std::string btNodeName = getOrDefault(instance_settings, "Bt Server Node", "/bt_server").toString().toStdString();

        auto node = rqt_generic::RqtBackendNode::getInstance();

        // create bt subscriptions and action server
        actionClient = rclcpp_action::create_client<ExecuteTree>(node, btNodeName + "/run_tree");
    
        stackSub = node->create_subscription<behaviortree::msg::TreeStack>(
            btNodeName + "/tree_stack", rclcpp::SystemDefaultsQoS(),
            std::bind(&BehaviortreePanel::stackCb, this, _1));

        refreshClient = node->create_client<behaviortree::srv::ListTrees>(btNodeName + "/list_trees");

        // delete timer if it exists
        if (timer_)
        {
            delete timer_;
            timer_ = 0;
        }

        // create and start timer
        timer_ = new QTimer(widget_);
        timer_->setInterval(std::chrono::milliseconds(dataIntervalMs));
        timer_->callOnTimeout(std::bind(&BehaviortreePanel::timerCb, this));
        timer_->start();

        lock_.unlock();
    }


    void BehaviortreePanel::selectTree(int treeIdx)
    {
        std::string treeName = ui_.btSelect->itemText(treeIdx).toStdString();

        bool found = std::find(treeList.begin(), treeList.end(), treeName) != treeList.end();
        if(found){
            // enable the start button
            buttonStates = BTPBS_START_ENABLED;
        } else {
            buttonStates = BTPBS_STOP_ENABLED;
        }
    }


    void BehaviortreePanel::startTask()
    {
        lock_.lock();

        // disable the start button, reset tree stack view
        statusColor = BTPS_BLACK;
        statusText = "";

        // get our local rosnode
        auto node = rqt_generic::RqtBackendNode::getInstance();

        auto start = node->get_clock()->now();
        while (!actionClient->wait_for_action_server(100ms))
            if (node->get_clock()->now() - start > 1s || !rclcpp::ok())
            {
                // we have timed out waiting, re-enable the button
                ui_.btStart->setEnabled(true);
                lock_.unlock();
                return;
            }

        auto goal = ExecuteTree::Goal();
        goal.tree = ui_.btSelect->currentText().toStdString();

        // create the goal callbacks to bind to
        auto sendGoalOptions = rclcpp_action::Client<ExecuteTree>::SendGoalOptions();
        sendGoalOptions.goal_response_callback =
            std::bind(&BehaviortreePanel::taskStartCb, this, _1);
        sendGoalOptions.feedback_callback =
            std::bind(&BehaviortreePanel::taskFeedbackCb, this, _1, _2);
        sendGoalOptions.result_callback =
            std::bind(&BehaviortreePanel::taskCompleteCb, this, _1);

        // send the goal with the callbacks configured
        actionClient->async_send_goal(goal, sendGoalOptions);

        lock_.unlock();
    }


    void BehaviortreePanel::cancelTask()
    {
        actionClient->async_cancel_all_goals(std::bind(&BehaviortreePanel::cancelAccept, this, _1));
    }


    void BehaviortreePanel::refresh()
    {
        lock_.lock();

        dropdownState = BTPDS_DISABLED;

        // get our local rosnode
        auto node = rqt_generic::RqtBackendNode::getInstance();

        behaviortree::srv::ListTrees::Request::SharedPtr startReq = std::make_shared<behaviortree::srv::ListTrees::Request>();
        auto start = node->get_clock()->now();
        while (!refreshClient->wait_for_service(100ms))
            if (node->get_clock()->now() - start > 1s || !rclcpp::ok()){
                lock_.unlock();
                return;
            }

        auto refreshFutureInfo = refreshClient->async_send_request(startReq);
        refreshFuture = refreshFutureInfo.share();
        refreshFutureid = refreshFutureInfo.request_id;

        // set a timer to get the results so we dont lock the thread :)
        // clear the timer ticks
        timerTick = 0;

        statusText = "Refreshing tree list";
        statusColor = BTPS_BLACK;

        // set a oneshot timer for 1s into the future and check status
        QTimer::singleShot(250, [this]()
                            { waitForRefresh(); });

        lock_.unlock();
    }


    void BehaviortreePanel::taskStartCb(const GHExecuteTree::SharedPtr &goalHandle)
    {
        lock_.lock();

        if (!goalHandle)
        {
            // the server did not accept
            dropdownState = BTPDS_ENABLED;
            statusText = "Goal rejected";
            statusColor = BTPS_RED;
            this->treeStack = {"Goal rejected!"};
        }
        else
        {
            // the server accepted, enable the cancel
            buttonStates = BTPBS_STOP_ENABLED;
            dropdownState = BTPDS_DISABLED;
            statusText = "Tree Running";
            statusColor = BTPS_BLACK;
        }

        lock_.unlock();
    }


    void BehaviortreePanel::taskCompleteCb(const GHExecuteTree::WrappedResult &result)
    {
        lock_.lock();

        // we can re-enable the start button and disable the stop button
        buttonStates = BTPBS_START_ENABLED;
        dropdownState = BTPDS_ENABLED;

        if(result.code == rclcpp_action::ResultCode::ABORTED){
            statusText = "Tree aborted";
            statusColor = BTPS_RED;
            this->treeStack = { result.result->error };
        } else
        {
            statusText = "Ready";
            statusColor = BTPS_BLACK;
            this->treeStack = {};
        }

        lock_.unlock();
    }


    void BehaviortreePanel::taskFeedbackCb(GHExecuteTree::SharedPtr goalHandle, ExecuteTree::Feedback::ConstSharedPtr feedback)
    {
        (void) goalHandle;
        (void) feedback;
    }


    void BehaviortreePanel::cancelAccept(const action_msgs::srv::CancelGoal::Response::SharedPtr resp)
    {
        (void) resp;
        lock_.lock();
        statusText = "Canceled";
        statusColor = BTPS_RED;
        lock_.unlock();
    }


    void BehaviortreePanel::stackCb(const behaviortree::msg::TreeStack & msg)
    {
        lock_.lock();
        this->treeStack = msg.stack;
        lock_.unlock();
    }


    void BehaviortreePanel::updateStack(const std::vector<std::string>& stack)
    {
        // clean the tree
        stackModel_->clear();

        // get the root again
        QStandardItem* parentItem = stackModel_->invisibleRootItem();

        // add the new tree
        for(auto name : stack){
            QStandardItem * item = new QStandardItem(QString::fromStdString(name));
            parentItem->appendRow(item);
        }
    }


    void BehaviortreePanel::waitForRefresh()
    {
        lock_.lock();

        if (!refreshFuture.valid())
        {
            // clear the lookup lockout
            refreshFutureid = -1;

            ui_.status->setText("Refresh invalid");
            ui_.status->setStyleSheet("color: red");

            // prevent a re-schedule
            lock_.unlock();
            return;
        }

        // check if future has validated
        auto futureStatus = refreshFuture.wait_for(10ms);
        if (futureStatus == std::future_status::timeout && timerTick < 10)
        {
            QTimer::singleShot(250, [this]()
                               { waitForRefresh(); });

            timerTick++;
        }
        else if (futureStatus != std::future_status::timeout)
        {
            // future has come back parse the PID information
            auto response = refreshFuture.get();

            ui_.btSelect->clear();
            ui_.btSelect->addItem("None Selected");

            // keep a list of the valid trees
            treeList.clear();
            treeList.insert(treeList.end(), response->trees.begin(), response->trees.end());

            for (auto tree : response->trees)
            {
                // push these into the combo box
                ui_.btSelect->addItem(QString::fromStdString(tree));
            }

            statusText = "Ready";
            statusColor = BTPS_BLACK;
            dropdownState = BTPDS_ENABLED;
        }
        else if (timerTick == 10)
        {
            // remove the failed request
            refreshClient->remove_pending_request(refreshFutureid);
        }

        lock_.unlock();
    }


    void BehaviortreePanel::timerCb()
    {
        lock_.lock();

        switch(statusColor)
        {
            case BTPS_BLACK:
                ui_.status->setStyleSheet("color: black");
                ui_.btStackView->setStyleSheet("");
                break;
            case BTPS_RED:
                ui_.status->setStyleSheet("color: red");
                ui_.btStackView->setStyleSheet("background-color: rgb(238, 133, 133)");
                break;
        }

        ui_.status->setText(QString::fromStdString(statusText));

        switch(buttonStates)
        {
            case BTPBS_NONE_ENABLED:
                ui_.btStart->setEnabled(false);
                ui_.btStop->setEnabled(false);
                break;
            case BTPBS_START_ENABLED:
                ui_.btStart->setEnabled(true);
                ui_.btStop->setEnabled(false);
                break;
            case BTPBS_STOP_ENABLED:
                ui_.btStart->setEnabled(false);
                ui_.btStop->setEnabled(true);
                break;
            case BTPBS_BOTH_ENABLED:
                ui_.btStart->setEnabled(true);
                ui_.btStop->setEnabled(true);
                break;
        }

        switch(dropdownState)
        {
            case BTPDS_DISABLED:
                ui_.btSelect->setEnabled(false);
                break;
            case BTPDS_ENABLED:
                ui_.btSelect->setEnabled(true);
                break;
        }

        updateStack(this->treeStack);

        bool upToDate = actionClient->action_server_is_ready();
        ui_.online->setStyleSheet(QString("background-color:%1; border-radius:8px;").arg(upToDate ? "green" : "red"));

        lock_.unlock();
    }
}

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(behaviortree_rqt::BehaviortreePanel, rqt_gui_cpp::Plugin)
