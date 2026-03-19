#include "behaviortree/behaviortree_health.hpp"
#include <behaviortree/tinyxml2.h>

//
// AutonomySyncIssueDetector
//

std::string AutonomySyncIssueDetector::portDirectionToString(const BT::PortDirection& direction)
{
    switch(direction)
    {
        case BT::PortDirection::INPUT:
            return "input_port";
        case BT::PortDirection::OUTPUT:
            return "output_port";
        case BT::PortDirection::INOUT:
            return "inout_port";
    }

    return "undefined";
}


BT::PortDirection AutonomySyncIssueDetector::stringToPortDirection(const std::string& str)
{
    return (str == "input_port" ? BT::PortDirection::INPUT :
            str == "output_port" ? BT::PortDirection::OUTPUT :
            str == "inout_port" ? BT::PortDirection::INOUT : (BT::PortDirection) -1);
}


BT::NodeType AutonomySyncIssueDetector::stringToNodeType(const std::string& str)
{
    return (str == "Action" ? BT::NodeType::ACTION :
            str == "Condition" ? BT::NodeType::CONDITION : 
            str == "Control" ? BT::NodeType::CONTROL :
            str == "Decorator" ? BT::NodeType::DECORATOR :
            str == "Subtree" ? BT::NodeType::SUBTREE : BT::NodeType::UNDEFINED);
}


AutonomySyncIssueDetector::AutonomySyncIssueDetector(
    const std::string& file,
    std::shared_ptr<const BT::BehaviorTreeFactory> factory)
 : _file(file),
   _factory(factory) { }


HealthError AutonomySyncIssueDetector::detect()
{
    //nodes will be removed from this list to determine which ones only exist in code
    std::vector<std::string> factoryNodes;
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLElement *treeNodesModel = detectTreeNodesModel(xmlDoc);

    //populate list of factory nodes
    for(auto it : _factory->manifests())
    {
        factoryNodes.push_back(it.first);
    }

    if(!treeNodesModel)
    {
        //nothing to check here
        return HealthError(false, "");
    }

    //now make sure all nodes in xml match what is in code
    std::set<std::string> discoveredIds;
    for(
        tinyxml2::XMLElement *nodeElement = treeNodesModel->FirstChildElement();
        nodeElement;
        nodeElement = nodeElement->NextSiblingElement())
    {
        const char
            *xmlType = nodeElement->Name(), //this better exist or theres an issue with tinyxml
            *xmlId = nodeElement->Attribute("ID"); //this may not exist
        
        // if not true, issue will be created by detectIdAndTypeIssues
        // need this here because I want to capture subtrees in the manifest
        // because I also use this detector as a palette scanner
        if(xmlId) 
        {
            BT::TreeNodeManifest manifest;
            manifest.type = stringToNodeType(xmlType);
            _palette.insert({ xmlId, manifest });
        }

        bool isSubtree = std::string(xmlType) == "SubTree";
        if(!detectPortIssues(xmlId, nodeElement, isSubtree))
        {
            continue;
        }

        if(isSubtree)
        {
            continue;
        }
        
        if(!detectIdAndTypeIssues(xmlId, xmlType, nodeElement))
        {
            continue;
        }

        // if xmlId is already in the set then it has a duplicate
        // raise a mismatch issue on this, the class is capable of fixing this
        if(discoveredIds.count(xmlId) > 0)
        {
            addIssue(
                std::make_shared<AutonomyNodeModelIssue>(
                    ISSUE_WARN, _file, treeNodesModel->GetLineNum(), xmlId, true, 
                    "Node " + std::string(xmlId) + " has duplicate entries in the TreeNodesModel", 
                    _factory));
        }

        discoveredIds.insert(discoveredIds.end(), xmlId);

        // remove builtin nodes from factory nodes
        for(size_t i = 0; i < factoryNodes.size(); i++)
        {
            if(_factory->builtinNodes().count(factoryNodes.at(i)))
            {
                factoryNodes.erase(factoryNodes.begin() + i);
                i--;
            }
        }

        //if we get here, then node matches factory version or is built-in. remove from list
        auto it = std::find(factoryNodes.begin(), factoryNodes.end(), xmlId);
        if(it != factoryNodes.end())
        {
            factoryNodes.erase(it);
        }
    }

    if(issues().size() > 0)
    {
        return HealthError(true, "Skipping additional checks to avoid cascading errors");
    }

    // now add a mismatch issue for every node in XML that is not in code
    for(std::string node : factoryNodes)
    {
        addIssue(
            std::make_shared<AutonomyNodeModelIssue>(
                ISSUE_WARN, _file, treeNodesModel->GetLineNum(), node, true, 
                "Node " + node + " is defined in the code but was not found in the XML", 
                _factory));
    }

    return HealthError(false, "");
}


NodeManifests AutonomySyncIssueDetector::palette() const
{
    return _palette;
}


tinyxml2::XMLElement *AutonomySyncIssueDetector::detectTreeNodesModel(tinyxml2::XMLDocument& xmlDoc)
{
    //try to open xml document
    xmlDoc.LoadFile(_file.c_str());
    if(xmlDoc.Error())
    {
        addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
                ISSUE_ERROR,
                _file,
                1,
                "XMLError", 
                xmlDoc.ErrorStr()));

        return nullptr;
    }

    tinyxml2::XMLElement *rootElement = xmlDoc.RootElement();
    if(!rootElement)
    {
        addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
                ISSUE_ERROR, 
                _file,
                1,
                "XMLError",
                "Missing BT root node"));
        
        return nullptr;
    }

    tinyxml2::XMLElement *treeNodesModel = rootElement->FirstChildElement("TreeNodesModel");
    return treeNodesModel;
}


bool AutonomySyncIssueDetector::detectIdAndTypeIssues(const char *xmlId, const char *xmlType, tinyxml2::XMLElement *nodeElement)
{
    if(!xmlId)
    {
        addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
                ISSUE_ERROR,
                _file,
                nodeElement->GetLineNum(),
                "TreeNodeModelError",
                "Node model must have ID, but it does not"));

        return false;
    }

    //check that ID is not the same as that of a builtin node
    if(_factory->builtinNodes().count(xmlId) > 0)
    {
        addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
                ISSUE_ERROR,
                _file,
                nodeElement->GetLineNum(),
                "NameError",
                "Custom node name " + std::string(xmlId) + " is the same as a builtin node"));
    }

    //check that ID is a node that exists in the code
    if(_factory->manifests().count(xmlId) == 0)
    {
        addIssue(
            std::make_shared<AutonomyNodeModelIssue>(
                ISSUE_ERROR,
                _file,
                nodeElement->GetLineNum(),
                xmlId,
                false,
                "Node \"" + std::string(xmlId) + "\" was found in XML but is not defined in code",
                _factory));
        
        return false;
    }

    //check that node types match between XML and code
    std::string factoryType = "Undefined";
    switch(_factory->manifests().at(xmlId).type)
    {
        case BT::NodeType::ACTION:
            factoryType = "Action";
            break;
        case BT::NodeType::CONDITION:
            factoryType = "Condition";
            break;
        case BT::NodeType::DECORATOR:
            factoryType = "Decorator";
            break;
        case BT::NodeType::CONTROL:
            factoryType = "Control";
            break;
        default:
            factoryType = "Undefined";
    }

    if(xmlType != factoryType)
    {
        addIssue(
            std::make_shared<AutonomyNodeModelIssue>(
                ISSUE_ERROR,
                _file,
                nodeElement->GetLineNum(),
                xmlId,
                true,
                std::string(xmlId) + " listed as type " + std::string(xmlType) + " in xml but is " + factoryType + " in code",
                _factory));

        return false;
    }

    return true;
}


bool AutonomySyncIssueDetector::detectPortIssues(const char *xmlId, tinyxml2::XMLElement* nodeElement, bool isSubtree)
{
    //get a vector of factory port names so we can keep track of which was are invalid/missing
    std::vector<std::string> factoryPortNames;
    BT::PortsList factoryPorts;

    //populate factory information only if the node is known by the factory (it may not be, for example, like a subtree)
    if(!isSubtree && _factory->manifests().count(xmlId) > 0)
    {
        factoryPorts = _factory->manifests().at(xmlId).ports;
        for(auto it : factoryPorts)
        {
            factoryPortNames.push_back(it.first);
        }
    }
    
    for(
        tinyxml2::XMLElement *portElement = nodeElement->FirstChildElement();
        portElement;
        portElement = portElement->NextSiblingElement())
    {
        //iterate through xml elements and remove matches from factory list
        const char
            *xmlPortDirection = portElement->Name(), //this better exist
            *xmlPortName = portElement->Attribute("name"); //this may not
        
        if(!xmlPortName)
        {
            //xml port does not have a name
            addIssue(
                std::make_shared<AutonomyNodeModelIssue>(
                    ISSUE_ERROR,
                    _file, 
                    portElement->GetLineNum(),
                    xmlId,
                    true, 
                    "XML port does not have a name.",
                    _factory));
            
            return false;
        }

        //check that port has a valid direction (function returns -1 in this case)
        BT::PortDirection xmlDirectionEnum = stringToPortDirection(xmlPortDirection);
        if(xmlDirectionEnum == (BT::PortDirection) -1)
        {
            addIssue(
                std::make_shared<AutonomyNodeModelIssue>(
                    ISSUE_ERROR,
                    _file,
                    portElement->GetLineNum(),
                    xmlId,
                    true,
                    "XML port direction " + std::string(xmlPortDirection) + " is invalid",
                    _factory));
            
            return false;
        }

        //add port to palette for node
        BT::PortInfo portInfo(xmlDirectionEnum);
        _palette[xmlId].ports.insert({ xmlPortName, portInfo });

        //check that any port with that name exists in the code 
        std::vector<std::string>::iterator factoryPortNameLocation = std::find(factoryPortNames.begin(), factoryPortNames.end(), xmlPortName);
        if(!isSubtree && factoryPortNameLocation == factoryPortNames.end())
        {
            //xml port not present in factory manifest
            addIssue(
                std::make_shared<AutonomyNodeModelIssue>(
                    ISSUE_ERROR,
                    _file,
                    portElement->GetLineNum(),
                    xmlId,
                    true,
                    "XML port name not found in code.",
                    _factory));
                
            return false;
        }

        //now check the the port type is correct
        
        BT::PortDirection factoryPortDirection = factoryPorts[xmlPortName].direction();
        if(!isSubtree && xmlDirectionEnum != factoryPortDirection)
        {
            addIssue(
                std::make_shared<AutonomyNodeModelIssue>(
                    ISSUE_ERROR,
                    _file,
                    portElement->GetLineNum(),
                    xmlId,
                    true,
                    "XML port with name " + std::string(xmlId) + "marked " + portDirectionToString(xmlDirectionEnum) + "in XML but is " + portDirectionToString(factoryPortDirection) + " in code.",
                    _factory));

            return false;
        }

        //if we get here, then the port is present in the manifest. remove it from the vector
        if(factoryPortNameLocation != factoryPortNames.end())
        {
            factoryPortNames.erase(factoryPortNameLocation);
        }
    }

    //any names remaining in the factory list are missing from the xml
    if(factoryPortNames.size() > 0)
    {
        std::string message = "Missing ports from the xml model: " + factoryPortNames[0];
        for(size_t i = 1; i < factoryPortNames.size(); i++)
        {
            message += ", " + factoryPortNames[i];
        }

        addIssue(
            std::make_shared<AutonomyNodeModelIssue>(
                ISSUE_ERROR, _file, nodeElement->GetLineNum(), xmlId, true, message, _factory));
        
        return false;
    }

    return true;
}
