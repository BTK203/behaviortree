#include "behaviortree/logging.hpp"
#include <iostream>

//
// HasBtLogger
//
void HasBtLogger::setLogger(const BtLogger::SharedPtr logger)
{
    logger_ = logger;
}

void HasBtLogger::deinit()
{
    logger_.reset();
}

BtLogger::SharedPtr HasBtLogger::getLogger() const
{
    return logger_;
}

// default logger for HasBtLogger. Programs wishing to use another logger should use HasBtLogger::setLogger() to set
BtLogger::SharedPtr HasBtLogger::logger_ = std::make_shared<StdoutStderrLogger>();

//
// StdoutStderrLogger
//

void StdoutStderrLogger::info(const std::string& msg)
{
    std::cout << "Info: " << msg << std::endl;
}


void StdoutStderrLogger::warning(const std::string& msg)
{
    std::cout << "Warning: " << msg << std::endl; 
}


void StdoutStderrLogger::error(const std::string& msg)
{
    std::cout << "ERROR: " << msg << std::endl;
}

//
// RosLogger
//

RosLogger::RosLogger(const rclcpp::Node::SharedPtr& node)
: node_(node) { }


void RosLogger::info(const std::string& msg)
{
    RCLCPP_INFO(node_->get_logger(), "%s", msg.c_str());
}


void RosLogger::warning(const std::string& msg)
{
    RCLCPP_WARN(node_->get_logger(), "%s", msg.c_str());
}


void RosLogger::error(const std::string& msg)
{
    RCLCPP_ERROR(node_->get_logger(), "%s", msg.c_str());
}
