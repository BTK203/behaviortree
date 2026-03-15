#include "behaviortree/behaviortree_health.hpp"
#include <behaviortree/tinyxml2.h>

AutonomyOutputPortFormatIssue::AutonomyOutputPortFormatIssue(
   const std::string& file, 
   tinyxml2::XMLElement *node, 
   const std::string& offender)
 : AutonomyIssue(ISSUE_WARN, file, node->GetLineNum(), "OutputPortFormatWarning",
                  "Value for output port \"" + offender + "\" must have braces"),
   _file(file),
   _node(node),
   _offender(offender) { }


bool AutonomyOutputPortFormatIssue::fixable()
{
   return true;
}


std::string AutonomyOutputPortFormatIssue::solution()
{
   const char *cval = _node->Attribute(_offender.c_str());
   std::string demo = "";
   if(cval)
   {
      demo = " (" + std::string(cval) + " -> {" + std::string(cval) + "})";
   }

   return "Add curly braces around value for in output port \"" + _offender + "\"" + demo;
}


HealthError AutonomyOutputPortFormatIssue::fix()
{
   // retrieve current value
   const char *cval = _node->Attribute(_offender.c_str());
   if(!cval)
   {
      return HealthError(true, "Could not find attribute to fix");
   }

   // fix and replace
   std::string newVal = "{" + std::string(cval) + "}";
   _node->SetAttribute(_offender.c_str(), newVal.c_str());
   
   // now save
   _node->GetDocument()->SaveFile(_file.c_str());

   return HealthError(false, "");
}
