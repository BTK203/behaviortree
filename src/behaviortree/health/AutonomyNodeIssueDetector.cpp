#include "behaviortree/behaviortree_health.hpp"
#include <behaviortree/tinyxml2.h>

//
// AutonomyNodeIssueDetector
//

const std::set<std::string> AutonomyNodeIssueDetector::PROTECTED_NODE_NAMES = {
   "include",
   "BehaviorTree",
   "TreeNodesModel"
};

AutonomyNodeIssueDetector::AutonomyNodeIssueDetector(
    tinyxml2::XMLElement *node,
    const std::string& file,
    std::shared_ptr<const BT::BehaviorTreeFactory> factory,
    const NodeManifests& palette,
    const std::vector<std::string>& blackboardDefinitions)
 : _node(node),
   _fileName(file),
   _factory(factory),
   _palette(palette),
   _blackboardDefs(blackboardDefinitions) { }


HealthError AutonomyNodeIssueDetector::detect()
{
   HealthError err(false, "");
   std::string nodeName = _node->Name(); //should exist

   //skip processing for this node if it is a protected keyword like include
   if(PROTECTED_NODE_NAMES.count(nodeName) > 0)
   {
      return HealthError(false, "");
   }

   // does it exist in the manifests
   if(_factory->manifests().count(nodeName) == 0)
   {
      addIssue(std::make_shared<AutonomyUndefinedIssue>(_fileName, _node));
      return HealthError(true, "Aborted due to previous errors");
   }

   if(_factory->builtinNodes().count(nodeName) == 0 && _palette.count(nodeName) == 0)
   {
      addIssue(
         std::make_shared<UnfixableAutonomyIssue>(
            ISSUE_ERROR,
            _fileName,
            _node->GetLineNum(),
            "ModelError",
            "Node " + nodeName + " is not present in the TreeNodesModel"));

      return HealthError(true, "Aborted due to previous errors");
   }

   //now process individual port values for issues
   BT::PortsList btPorts = _factory->manifests().at(nodeName).ports;

   if(nodeName == "SubTree")
   {
      // subtree models are not stored in the factory. Pull from palette instead
      const char *stId = _node->Attribute("ID");
      if(stId && _palette.count(std::string(stId)) > 0)
      {
         btPorts = _palette.at(std::string(stId)).ports;
      }
   }

   // check UWRT port information if able to
   std::map<std::string, UwrtPortNecessity> portNecessities;

   if(UwrtNodesManifest::hasInformationForNode(nodeName))
   {
      UwrtPortInformation uwrtPorts = UwrtNodesManifest::lookupInformationByNodeName(nodeName);
      for(UwrtPort port : uwrtPorts)
      {
         portNecessities.insert({ std::string(port.name()), port.necessity() });
      }
   }

   // check that all node attributes are real ports known by the factory (if this is false Groot wont even open the tree)
   for(const tinyxml2::XMLAttribute *attr = _node->FirstAttribute(); attr != nullptr; attr = attr->Next())
   {
      std::string name = attr->Name();

      // some attribute names are exempt from this check
      if(name == "ID")
      {
         continue;
      }

      if(btPorts.count(name) == 0)
      {
         addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
               ISSUE_ERROR,
               _fileName,
               _node->GetLineNum(),
               "UnknownPortError",
               nodeName + " specifies value for unknown port \"" + name + "\""));
      }
   }


   //check for bad blackboard refs (this does not require uwrt ports so it is done in another loop)
   for(auto pair : btPorts)
   {
      //name and value
      std::string portName = pair.first;
      const char *portValue = _node->Attribute(portName.c_str());

      //                                                 super secret hack for default value
      UwrtPortNecessity necessity = (portName.at(0) == '_' ? PORT_OPTIONAL : PORT_REQUIRED);
      if(portNecessities.count(portName) > 0)
      {
         // if the port has a set necessity in code that will be assigned here
         necessity = portNecessities.at(portName);
      }

      // necessity. If required then the value must be provided
      if((!portValue || std::string(portValue).empty()) && necessity == PORT_REQUIRED && pair.second.direction() != BT::PortDirection::OUTPUT)
      {
         addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
               ISSUE_ERROR,
               _fileName,
               _node->GetLineNum(),
               "RequiredPortError",
               nodeName + " missing value for required port \"" + portName + "\""));
         
         continue;
      }

      //bad blackboard ref
      if(portValue && BT::TreeNode::isBlackboardPointer(portValue) && pair.second.direction() == BT::PortDirection::INPUT)
      {
         //now check that blackboard reference is good
         std::string targetPointer = std::string(BT::TreeNode::stripBlackboardPointer(portValue));
         if(std::find(_blackboardDefs.begin(), _blackboardDefs.end(), targetPointer) == _blackboardDefs.end())
         {
            addIssue(
               std::make_shared<UnfixableAutonomyIssue>(
                  ISSUE_WARN,
                  _fileName,
                  _node->GetLineNum(),
                  "PortWarning",
                  "Blackboard variable " + targetPointer + " may not be defined yet"));
            
            continue;
         }
      }

      // output ports must have braces
      if(portValue 
         && !std::string(portValue).empty() 
         && pair.second.direction() == BT::PortDirection::OUTPUT
         && !BT::TreeNode::isBlackboardPointer(portValue))
      {   
         addIssue(
            std::make_shared<AutonomyOutputPortFormatIssue>(
               _fileName,
               _node,
               portName));
      }

      if(portValue
         && !std::string(portValue).empty()
         && (pair.second.direction() == BT::PortDirection::OUTPUT
               || pair.second.direction() == BT::PortDirection::INOUT)
         && std::find(_blackboardDefs.begin(), _blackboardDefs.end(), pair.first) == _blackboardDefs.end())
      {
         std::string val = portValue;
         if(BT::TreeNode::isBlackboardPointer(val))
         {
            val = BT::TreeNode::stripBlackboardPointer(val);
         }
         _blackboardDefs.push_back(val);
      }
   }

   //if the node is a script, we have a special detector we can use
   if(nodeName == "Script")
   {
      std::shared_ptr<AutonomyScriptIssueDetector> scriptIssueDetector = 
         std::make_shared<AutonomyScriptIssueDetector>(
            _node,
            _fileName,
            _factory,
            _palette,
            _blackboardDefs);

      err = addSubdetector(scriptIssueDetector);
      if(err.error)
      {
         return err;
      }

      _blackboardDefs = scriptIssueDetector->blackboardDefinitions();
   }

   return HealthError(false, "");
}


std::vector<std::string> AutonomyNodeIssueDetector::blackboardDefinitions() const
{
   return _blackboardDefs;
}
