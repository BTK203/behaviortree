#include "behaviortree/behaviortree_health.hpp"

//
// AutonomyOmittedIssue
//

AutonomyOmittedIssue::AutonomyOmittedIssue(const std::string& file, const std::string& project)
 : AutonomyIssue(ISSUE_WARN, file, 0, "OmittedWarning", "File " + file + " is not present in the project " + project),
   _file(file),
   _project(project)
{ }


bool AutonomyOmittedIssue::fixable()
{
    return true;
}


std::string AutonomyOmittedIssue::solution()
{
    return "add missing file " + _file + " to project " + _project;
}


HealthError AutonomyOmittedIssue::fix()
{
    return HealthError(false, "");
}
