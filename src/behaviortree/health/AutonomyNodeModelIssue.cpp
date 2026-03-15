#include "behaviortree/behaviortree_health.hpp"
#include <behaviortree/tinyxml2.h>

//
// AutonomyNodeModelIssue
//
AutonomyNodeModelIssue::AutonomyNodeModelIssue(AutonomyIssueSeverity severity, const std::string& file, 
                                                    int line, const std::string& nodeId, bool fixableInXml, 
                                                    const std::string& description, const std::shared_ptr<const BT::BehaviorTreeFactory>& factory)
 : AutonomyIssue(severity, file, line, (severity == ISSUE_WARN ? "NodeModelWarning" : "NodeModelError"), description),
   _nodeId(nodeId),
   _fixableInXml(fixableInXml),
   _factory(factory) { }


bool AutonomyNodeModelIssue::fixable()
{
    return _fixableInXml;
}


std::string AutonomyNodeModelIssue::solution()
{
    if(fixable())
    {
        return "Remove any existing " + _nodeId + " models from the palette and add a model which matches the code";
    }
    
    // else
    return "unfixable";
}


HealthError AutonomyNodeModelIssue::fix()
{
    if(!fixable())
    {
        return HealthError(true, "This issue is unfixable. The issue must be fixed in the C++ implementation");
    }

    tinyxml2::XMLDocument doc;
    doc.LoadFile(file().c_str());
    tinyxml2::XMLElement *root = doc.RootElement();
    if(!root)
    {
        return HealthError(true, "Failed to find document root");
    }

    tinyxml2::XMLElement *treeNodesModel = root->FirstChildElement("TreeNodesModel");
    if(!treeNodesModel)
    {
        return HealthError(true, "Unable to find TreeNodesModel tag");
    }

    // find a model of the node in question in the palette. We want to replace it so if it exists, delete it
    tinyxml2::XMLElement *paletteNode = treeNodesModel->FirstChildElement();
    while(paletteNode)
    {
        const char *paletteNodeId = paletteNode->Attribute("ID");
        if(paletteNodeId)
        {
            if(std::string(paletteNodeId) == _nodeId)
            {
                // found the mismatched node. now delete it
                // first advance to the next sibling to avoid undefined behavior
                tinyxml2::XMLElement *elementToDelete = paletteNode;
                paletteNode = paletteNode->NextSiblingElement();
                treeNodesModel->DeleteChild(elementToDelete);
                continue;
                // continue instead of break here so we can handle duplicates
            }
        }

        paletteNode = paletteNode->NextSiblingElement();
    }

    // now we know that the node does not exist, so we can add it to the palette using the factory model
    // first retrieve the code model
    if(_factory->manifests().count(_nodeId) != 1)
    {
        return HealthError(false, "Code implementation does not contain the node " + _nodeId);
    }

    auto manifest = _factory->manifests().at(_nodeId);
    std::string typeStr = BT::toStr(manifest.type);

    //now create the new palette model
    tinyxml2::XMLElement *newModel = doc.NewElement(typeStr.c_str());
    newModel->SetAttribute("ID", _nodeId.c_str());
    newModel->SetAttribute("editable", "true");

    // add the ports
    for(auto port : manifest.ports)
    {
        std::string portDirectionString = "";
        switch(port.second.direction())
        {
            case BT::PortDirection::INPUT:
                portDirectionString = "input_port";
                break;
            case BT::PortDirection::OUTPUT:
                portDirectionString = "output_port";
                break;
            case BT::PortDirection::INOUT:
                portDirectionString = "inout_port";
                break;
            default:
                return HealthError(true, "Unknown port direction number " + std::to_string((int) port.second.direction()));
        }

        tinyxml2::XMLElement *newPort = doc.NewElement(portDirectionString.c_str());
        newPort->SetAttribute("name", port.first.c_str());
        newPort->SetAttribute("type", port.second.typeName().c_str());
        newModel->InsertEndChild(newPort);
    }

    // add the palette model to the palette and save the document
    treeNodesModel->InsertEndChild(newModel);

    doc.SaveFile(file().c_str());

    return HealthError(false, "");
}
