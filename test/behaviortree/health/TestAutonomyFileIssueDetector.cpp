#include "behaviortree_test/behaviortree_health_util_testing.hpp"
#include <behaviortree/tinyxml2.h>

#define EMPTY_FILE "emptyfile.xml"
#define BADINCLUDE_FILE "treeissuedetector_bad_include.xml"

class AutonomyFileIssueDetectorTest : public AutonomyHealthUtilTest
{ };


TEST_F(AutonomyFileIssueDetectorTest, TestEmptyFile)
{
    tinyxml2::XMLDocument doc;

    AutonomyFileIssueDetector fileIssueDetector(pathToTestTree(EMPTY_FILE), _factory);
    HealthError err = fileIssueDetector.detect();
    printIssuesIf(fileIssueDetector, fileIssueDetector.issues().size() != 2);
    ASSERT_TRUE(err.error);
    std::vector<AutonomyIssue::Ptr> issues = fileIssueDetector.issues();
    ASSERT_EQ(issues.size(), 2);
    printIssuesIf(fileIssueDetector, !issueVectorContains(issues, "XMLError"));
}


TEST_F(AutonomyFileIssueDetectorTest, TestBadInclude)
{
    tinyxml2::XMLDocument doc;

    AutonomyFileIssueDetector fileIssueDetector(pathToTestTree(BADINCLUDE_FILE), _factory);
    HealthError err = fileIssueDetector.detect();
    std::vector<AutonomyIssue::Ptr> issues = fileIssueDetector.issues();
    //remove all the nodemodelwarnings
    for(size_t i = 0; i < issues.size(); i++)
    {
        if(issues.at(i)->type() == "NodeModelWarning")
        {
            issues.erase(issues.begin() + i);
            i--;
        }
    }
    printIssuesIf(fileIssueDetector, issues.size() != 1 || err.error);
    ASSERT_FALSE(err.error);
    ASSERT_EQ(issues.size(), 1);
    printIssuesIf(fileIssueDetector, !issueVectorContains(issues, "UnspecifiedIncludeError"));
    ASSERT_TRUE(issueVectorContains(issues, "UnspecifiedIncludeError"));
}
