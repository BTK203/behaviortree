#include "behaviortree_test/behaviortree_health_util_testing.hpp"

#define TREEISSUEDETECTOR_FILE "treeissuedetectortest.xml"
#define EMPTYTREE_FILE "treeissuedetector_empty_tree.xml"
#define BADNODECASES_FILE "treeissuedetector_bad_node_cases.xml"

class AutonomyTreeIssueDetectorTest : public AutonomyHealthUtilTest
{ };


TEST_F(AutonomyTreeIssueDetectorTest, TestSimpleGoodTree)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement *node = walkTree(doc, TREEISSUEDETECTOR_FILE, 
        {
            {"BehaviorTree", 0}
        });
    
    ASSERT_TRUE(node);

    AutonomyTreeIssueDetector treeIssueDetector(TREEISSUEDETECTOR_FILE, "~", node, _factory, _palette);
    HealthError err = treeIssueDetector.detect();
    printIssuesIf(treeIssueDetector, treeIssueDetector.issues().size() > 0);
    ASSERT_FALSE(err.error);
    std::vector<AutonomyIssue::Ptr> issues = treeIssueDetector.issues();
    ASSERT_EQ(issues.size(), 0);
}


TEST_F(AutonomyTreeIssueDetectorTest, TestComplexGoodTree)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement *node = walkTree(doc, TREEISSUEDETECTOR_FILE, 
        {
            {"BehaviorTree", 2}
        });
    
    ASSERT_TRUE(node);
    AutonomySyncIssueDetector syncIssueDetector(pathToTestTree(TREEISSUEDETECTOR_FILE), _factory);
    syncIssueDetector.detect();
    printIssuesIf(syncIssueDetector, syncIssueDetector.issues().size() > 1); // we are expecting a node mismatch issue from this det
    _palette = syncIssueDetector.palette();

    AutonomyTreeIssueDetector treeIssueDetector(TREEISSUEDETECTOR_FILE, "~", node, _factory, _palette);
    HealthError err = treeIssueDetector.detect();
    printIssuesIf(treeIssueDetector, treeIssueDetector.issues().size() > 0);
    ASSERT_FALSE(err.error);
    std::vector<AutonomyIssue::Ptr> issues = treeIssueDetector.issues();
    ASSERT_EQ(issues.size(), 0);
}


TEST_F(AutonomyTreeIssueDetectorTest, TestRecursiveNodeIssueDetection)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement *node = walkTree(doc, TREEISSUEDETECTOR_FILE, 
        {
            {"BehaviorTree", 1}
        });
    
    ASSERT_TRUE(node);

    _palette.insert({"Info", BT::TreeNodeManifest()});

    AutonomyTreeIssueDetector treeIssueDetector(TREEISSUEDETECTOR_FILE, "~", node, _factory, _palette);
    HealthError err = treeIssueDetector.detect();
    printIssuesIf(treeIssueDetector, treeIssueDetector.issues().size() != 2);
    ASSERT_FALSE(err.error);
    std::vector<AutonomyIssue::Ptr> issues = treeIssueDetector.issues();
    ASSERT_EQ(issues.size(), 2);
    ASSERT_TRUE(issueVectorContains(issues, "RequiredPortError"));
    ASSERT_TRUE(issueVectorContains(issues, "ScriptError"));
}


TEST_F(AutonomyTreeIssueDetectorTest, TestEmptyTree)
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement *node = walkTree(doc, EMPTYTREE_FILE, {
            {"BehaviorTree", 0}
        });
    
    ASSERT_TRUE(node);

    AutonomyTreeIssueDetector treeIssueDetector(EMPTYTREE_FILE, "~", node, _factory, _palette);
    HealthError err = treeIssueDetector.detect();
    printIssuesIf(treeIssueDetector, treeIssueDetector.issues().size() != 1);
    ASSERT_TRUE(err.error);
    std::vector<AutonomyIssue::Ptr> issues = treeIssueDetector.issues();
    if(issues.size() > 0)
    {
        printIssuesIf(treeIssueDetector, issues[0]->type() != "EmptyTreeError");
    }
    ASSERT_EQ(issues.size(), 1);
    ASSERT_EQ(issues[0]->type(), "EmptyTreeError");
}


TEST_F(AutonomyTreeIssueDetectorTest, TestBadNodeTypes)
{
    tinyxml2::XMLDocument doc;

    // control node with 0 children
    tinyxml2::XMLElement *badctrl = walkTree(doc, BADNODECASES_FILE, {
        {"BehaviorTree", 0}
    });
    ASSERT_TRUE(badctrl);
    AutonomyTreeIssueDetector badCtrlDetector(BADNODECASES_FILE, "~", badctrl, _factory, _palette);
    HealthError err = badCtrlDetector.detect();
    ASSERT_FALSE(err.error);
    std::vector<AutonomyIssue::Ptr> issues = badCtrlDetector.issues();
    printIssuesIf(badCtrlDetector, !issueVectorContains(issues, "BTControlError"));
    ASSERT_EQ(issues.size(), 1);
    ASSERT_TRUE(issueVectorContains(issues, "BTControlError"));

    // decorator node with zero children
    tinyxml2::XMLElement *baddeczero = walkTree(doc, BADNODECASES_FILE, {
        {"BehaviorTree", 1}
    });
    
    ASSERT_TRUE(baddeczero);

    AutonomyTreeIssueDetector badDecZeroDetector(BADNODECASES_FILE, "~", baddeczero, _factory, _palette);
    err = badDecZeroDetector.detect();
    ASSERT_FALSE(err.error);
    issues = badDecZeroDetector.issues();
    printIssuesIf(badDecZeroDetector, !issueVectorContains(issues, "BTDecoratorError"));
    ASSERT_EQ(issues.size(), 1);
    ASSERT_TRUE(issueVectorContains(issues, "BTDecoratorError"));

    // decorator node with multiple children
    tinyxml2::XMLElement *baddecmult = walkTree(doc, BADNODECASES_FILE, {
        {"BehaviorTree", 2}
    });
    
    ASSERT_TRUE(baddecmult);

    AutonomyTreeIssueDetector badDecMultDetector(BADNODECASES_FILE, "~", baddecmult, _factory, _palette);
    err = badDecMultDetector.detect();
    ASSERT_FALSE(err.error);
    issues = badDecMultDetector.issues();
    printIssuesIf(badDecMultDetector, !issueVectorContains(issues, "BTDecoratorError"));
    ASSERT_EQ(issues.size(), 1);
    ASSERT_TRUE(issueVectorContains(issues, "BTDecoratorError"));

    // leaf node with children
    tinyxml2::XMLElement *badleaf = walkTree(doc, BADNODECASES_FILE, {
        {"BehaviorTree", 3}
    });
    
    ASSERT_TRUE(badleaf);

    AutonomyTreeIssueDetector badLeafDetector(BADNODECASES_FILE, "~", badleaf, _factory, _palette);
    err = badLeafDetector.detect();
    ASSERT_FALSE(err.error);
    issues = badLeafDetector.issues();
    printIssuesIf(badLeafDetector, !issueVectorContains(issues, "BTLeafError"));
    ASSERT_EQ(issues.size(), 1);
    ASSERT_TRUE(issueVectorContains(issues, "BTLeafError"));
}
