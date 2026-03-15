#include "behaviortree/behaviortree_health.hpp"


//
// AutonomyUndefinedIssue
//

AutonomyUndefinedIssue::AutonomyUndefinedIssue(const std::string& file, tinyxml2::XMLElement *node)
 : AutonomyIssue(ISSUE_ERROR, file, node->GetLineNum(), "UndefinedError",
                  "Node " + std::string(node->Name()) + " is not implemented")
 { }


bool AutonomyUndefinedIssue::fixable()
{
   return false;
}


std::string AutonomyUndefinedIssue::solution()
{
   return "unfixable";
}


HealthError AutonomyUndefinedIssue::fix()
{
   return HealthError(false, "");
}
