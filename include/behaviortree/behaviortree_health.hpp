#pragma once

#include "behaviortree/behaviortree.hpp"
// #include <behaviortree/tinyxml2.h>

//
// macros
//

// stdout escape sequences for graphics
#define TERM_ESC "\x1b[0"
#define TERM_END_GRAPHICS "m"

// stdout graphics escape sequences for style
#define TERM_NONE
#define TERM_BOLD ";1"
#define TERM_FAINT ";2"
#define TERM_ITALIC ";3"

// stdout graphics escape sequences for color
#define TERM_BLACK "0"
#define TERM_RED "1"
#define TERM_GREEN "2"
#define TERM_YELLOW "3"
#define TERM_BLUE "4"
#define TERM_MAGENTA "5"
#define TERM_CYAN "6"
#define TERM_WHITE "7"
#define TERM_COLOR_DEFAULT "9"

// stdout style macros
#define TERM_COLOR(fg, bg) ";3" fg ";4" bg
#define TERM_STYLE(text_style, color) TERM_ESC text_style color TERM_END_GRAPHICS
#define TERM_RESET TERM_STYLE(TERM_NONE, TERM_NONE)


//
// types
//
typedef std::unordered_map<std::string, BT::TreeNodeManifest> NodeManifests;

// forward-declarations for tinyxml. without these, the tinyxml types pop a warning about library visibility
namespace tinyxml2
{
    class XMLElement;
    class XMLDocument;
}


//
// Issues
//
struct HealthError
{
    HealthError(bool error, const std::string message)
     : error(error),
       message(message) { }

    bool error;
    std::string message;
};


enum AutonomyIssueSeverity
{
    ISSUE_INFO,
    ISSUE_WARN,
    ISSUE_ERROR
};


class AutonomyIssue
{
    public:
    typedef std::shared_ptr<AutonomyIssue> Ptr;

    static std::string fileAndLine(const std::string& file, const tinyxml2::XMLElement *element);

    AutonomyIssue(
        const AutonomyIssueSeverity& severity,
        const std::string& file,
        int line,
        const std::string& type,
        const std::string& description);

    AutonomyIssueSeverity severity() const;
    std::string file() const;
    int line() const;
    std::string type() const;
    std::string description() const;
    std::string issue(bool colorize = true) const;
    
    virtual bool fixable() = 0;
    virtual std::string solution() = 0;
    virtual HealthError fix() = 0;

    private:
    const AutonomyIssueSeverity _severity;
    const std::string _file;
    const int _line;
    const std::string _type;
    const std::string _description;
};


class UnfixableAutonomyIssue : public AutonomyIssue
{
    public:
    UnfixableAutonomyIssue(
        const AutonomyIssueSeverity& severity,
        const std::string& file,
        int line,
        const std::string& type,
        const std::string& description)
     : AutonomyIssue(severity, file, line, type, description) { }

    bool fixable()
    {
        return false;
    }

    std::string solution() 
    {
        return "unfixable";
    }

    HealthError fix()
    {
        return HealthError(true, "Issue cannot be automatically fixed.");
    }
};

//
// Detectors
//
class AutonomyIssueDetector
{
    public:
    typedef std::shared_ptr<AutonomyIssueDetector> Ptr;

    virtual HealthError detect() = 0;
    std::vector<AutonomyIssue::Ptr> issues() const;

    protected:
    void addIssue(const AutonomyIssue::Ptr& issue);
    HealthError addSubdetector(const AutonomyIssueDetector::Ptr& detector);

    private:
    std::vector<AutonomyIssue::Ptr> _issues;
    std::vector<AutonomyIssueDetector::Ptr> _subdetectors;
};


/**
 * Detects runtime issues for the entire autonomy system, including sync, all trees, and XML nodes.
 */
class AutonomySystemIssueDetector : public AutonomyIssueDetector
{
    public:
    HealthError detect() override;
};

//defined in AutonomySyncIssueDetector.cpp
class AutonomyNodeModelIssue : public AutonomyIssue
{
    public:
    AutonomyNodeModelIssue(
        AutonomyIssueSeverity severity,
        const std::string& file, 
        int line, 
        const std::string& nodeId, 
        bool fixableInXml,
        const std::string& description,
        const std::shared_ptr<const BT::BehaviorTreeFactory>& factory);

    bool fixable() override;
    std::string solution() override;
    HealthError fix() override;

    private:
    const std::string _nodeId;
    const bool _fixableInXml;
    const std::shared_ptr<const BT::BehaviorTreeFactory> _factory;
};

/**
 * Detects issues in information sync between code and XML
 */
class AutonomySyncIssueDetector : public AutonomyIssueDetector
{
    public:
    static std::string portDirectionToString(const BT::PortDirection& direction);
    static BT::PortDirection stringToPortDirection(const std::string& str);
    static BT::NodeType stringToNodeType(const std::string& str);

    AutonomySyncIssueDetector(
        const std::string& file,
        std::shared_ptr<const BT::BehaviorTreeFactory> factory);

    HealthError detect() override;
    NodeManifests palette() const;

    private:
    tinyxml2::XMLElement *detectTreeNodesModel(tinyxml2::XMLDocument& xmlDoc);
    bool detectIdAndTypeIssues(const char *xmlId, const char *xmlType, tinyxml2::XMLElement *nodeElement);
    bool detectPortIssues(const char *xmlId, tinyxml2::XMLElement* nodeElement, bool isSubtree);

    const std::string _file;
    std::shared_ptr<const BT::BehaviorTreeFactory> _factory;

    NodeManifests _palette;
};


class AutonomyFileIssueDetector : public AutonomyIssueDetector
{
    public:
    AutonomyFileIssueDetector(const std::string& file, const std::string& project, std::shared_ptr<const BT::BehaviorTreeFactory> factory, const NodeManifests& inheritedPalette = {});
    AutonomyFileIssueDetector(const std::string& file, std::shared_ptr<const BT::BehaviorTreeFactory> factory, const NodeManifests& inheritedPalette = {});
    HealthError detect() override;
    NodeManifests palette() const;
    std::string file() const;

    private:
    HealthError checkFileInProject();

    const std::string 
        _file,
        _project;
    
    std::shared_ptr<const BT::BehaviorTreeFactory> _factory;
    NodeManifests _inheritedPalette;
    NodeManifests _palette;

    // this used to be local in detect() but is now a member to keep the document in scope after detection completes. That way issues can own XMLElements that will stay valid in the solution phase
    std::shared_ptr<tinyxml2::XMLDocument> _xmlDoc;
};


class AutonomyOmittedIssue : public AutonomyIssue
{
    public:
    AutonomyOmittedIssue(const std::string& file, const std::string& project);
    bool fixable() override;
    std::string solution() override;
    HealthError fix() override;

    private:
    const std::string 
        _file,
        _project;
};

//
// This stuff defines how we analyze control nodes to detect potentially undefined blackboard entries
//

typedef std::function<std::vector<int>(size_t n)> NodeExecutionOrder;

enum NodeExecutionBlackboardLinkStatus
{
    BLACKBOARD_LINKED,
    BLACKBOARD_UNLINKED
};

struct NodeExecutionOrderWithBlackboard
{
    NodeExecutionOrderWithBlackboard(const NodeExecutionOrder& order, NodeExecutionBlackboardLinkStatus blackboardLinked)
     : order(order),
       blackboardLinked(blackboardLinked) { }

    NodeExecutionOrder order;
    NodeExecutionBlackboardLinkStatus blackboardLinked;
};

typedef std::vector<NodeExecutionOrderWithBlackboard> NodeExecutionDescription;
const std::map<std::string, NodeExecutionDescription> NODE_EXECUTION_DESCRIPTIONS();

/**
 * Detects issues in specific trees such as bad includes, overpopulated decorators, etc.
 * Invokes AutonomyNodeIssueDetector as a subdetector.
 */
class AutonomyTreeIssueDetector : public AutonomyIssueDetector
{
    public:
    AutonomyTreeIssueDetector(
        const std::string& fileName,
        const std::string& cwd,
        tinyxml2::XMLElement *root,
        std::shared_ptr<const BT::BehaviorTreeFactory> factory,
        const NodeManifests& palette);

    HealthError detect() override;
    NodeManifests palette() const;
    std::string file() const;

    protected:
    HealthError addSubdetector(const AutonomyIssueDetector::Ptr& detector);
    
    private:
    HealthError processTreeRecursive(tinyxml2::XMLElement *treeRoot, std::vector<std::string>& blackboardDefinitions);
    void mergeNewPalette(const NodeManifests& palette);

    const std::string _fileName, _cwd;
    tinyxml2::XMLElement *_rootElement;
    std::shared_ptr<const BT::BehaviorTreeFactory> _factory;
    NodeManifests _palette;
};


class AutonomyUndefinedIssue : public AutonomyIssue
{
    public:
    AutonomyUndefinedIssue(const std::string& file, tinyxml2::XMLElement *node);

    bool fixable() override;
    std::string solution() override;
    HealthError fix() override;
};


class AutonomyOutputPortFormatIssue : public AutonomyIssue
{
    public:
    AutonomyOutputPortFormatIssue(
        const std::string& file, 
        tinyxml2::XMLElement *node, 
        const std::string& offender);

    bool fixable() override;
    std::string solution() override;
    HealthError fix() override;

    private:
    const std::string _file;
    tinyxml2::XMLElement *_node;
    const std::string _offender;
};


/**
 * Detects issues in specific XML node instances, like unfilled required ports
 */
class AutonomyNodeIssueDetector : public AutonomyIssueDetector
{
    public:
    AutonomyNodeIssueDetector(
        tinyxml2::XMLElement *node,
        const std::string& file,
        std::shared_ptr<const BT::BehaviorTreeFactory> factory,
        const NodeManifests& palette,
        const std::vector<std::string>& blackboardDefinitions = {});
    
    HealthError detect() override;
    std::vector<std::string> blackboardDefinitions() const;

    protected:
    tinyxml2::XMLElement *_node;
    const std::string _fileName;
    std::shared_ptr<const BT::BehaviorTreeFactory> _factory;
    NodeManifests _palette;
    std::vector<std::string> _blackboardDefs;

    private:
    // this set will contain the names of tags like include that are not nodes and should not be treated as such
    static const std::set<std::string> PROTECTED_NODE_NAMES;
};


class AutonomyScriptIssueDetector : public AutonomyNodeIssueDetector
{
    public:
    AutonomyScriptIssueDetector(
        tinyxml2::XMLElement *node,
        const std::string& file,
        std::shared_ptr<const BT::BehaviorTreeFactory> factory,
        const NodeManifests& palette,
        const std::vector<std::string>& blackboardDefinitions = {});
    
    HealthError detect() override;
};
