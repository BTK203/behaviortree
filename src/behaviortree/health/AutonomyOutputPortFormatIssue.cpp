#include "behaviortree/behaviortree_health.hpp"

AutonomyOutputPortFormatIssue::AutonomyOutputPortFormatIssue(
   const std::string& file, 
   tinyxml2::XMLElement *node, 
   const std::string& offender)
 : AutonomyIssue(ISSUE_WARN, file, node->GetLineNum(), "OutputPortFormatWarning",
                  "Output port " + offender + " must have braces") 
{ }


bool AutonomyOutputPortFormatIssue::fixable()
{
   return true;
}


std::string AutonomyOutputPortFormatIssue::solution()
{
   return "Add curly braces around value in output port";
}


HealthError AutonomyOutputPortFormatIssue::fix()
{
   return HealthError(false, "");
}
