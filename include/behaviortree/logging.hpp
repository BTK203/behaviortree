#pragma once
#include <rclcpp/rclcpp.hpp>
#include <string>

//
// this header contains some stuff needed to make console logging agnostic to ROS/non-ROS
// NOTE: not to be confused with UWRTLogger which captures tree state transitions. This is just for generic messages
// that you would usually give to stdout/stderr
//

class BtLogger
{
    public:
    typedef std::shared_ptr<BtLogger> SharedPtr;
    virtual void info(const std::string& msg) = 0;
    virtual void warning(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
};


class HasBtLogger
{
    public:
    static void setLogger(const BtLogger::SharedPtr logger);
    static void deinit();

    protected:
    BtLogger::SharedPtr getLogger() const;

    private:
    static BtLogger::SharedPtr logger_;
};


class StdoutStderrLogger : public BtLogger
{
    public:
    void info(const std::string& msg) override;
    void warning(const std::string& msg) override;
    void error(const std::string& msg) override;
};


class RosLogger : public BtLogger
{
    public:
    RosLogger(const rclcpp::Node::SharedPtr& node);
    void info(const std::string& msg) override;
    void warning(const std::string& msg) override;
    void error(const std::string& msg) override;

    private:
    rclcpp::Node::SharedPtr node_;
};
