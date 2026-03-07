#include "autonomy_test/autonomy_health_util_testing.hpp"

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
    printIssuesIf(fileIssueDetector, fileIssueDetector.issues().size() != 2 || err.error);
    ASSERT_FALSE(err.error);
    std::vector<AutonomyIssue::Ptr> issues = fileIssueDetector.issues();
    ASSERT_EQ(issues.size(), 2);
    printIssuesIf(fileIssueDetector, !issueVectorContains(issues, "UnspecifiedIncludeError"));
    ASSERT_TRUE(issueVectorContains(issues, "UnspecifiedIncludeError"));
    ASSERT_TRUE(issueVectorContains(issues, "NodeMismatchIssue"));
}
