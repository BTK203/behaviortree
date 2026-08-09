#include "behaviortree/behaviortree_health.hpp"
#include <behaviortree/tinyxml2.h>

AutonomyFileIssueDetector::AutonomyFileIssueDetector(const std::string& file, const std::string& project, std::shared_ptr<const BT::BehaviorTreeFactory> factory, const NodeManifests& inheritedPalette)
 : _file(file),
   _project(project),
   _factory(factory),
   _inheritedPalette(inheritedPalette) { }


AutonomyFileIssueDetector::AutonomyFileIssueDetector(const std::string& file, std::shared_ptr<const BT::BehaviorTreeFactory> factory, const NodeManifests& inheritedPalette)
 : AutonomyFileIssueDetector(file, "", factory, inheritedPalette) { }

 
HealthError AutonomyFileIssueDetector::detect()
{
    HealthError err(false, "");
    std::string cwd = _file.substr(0, _file.rfind('/'));

    // if a project was specified, check that the detector includes it
    if(!_project.empty())
    {
        err = checkFileInProject();
        if(err.error)
        {
            return err;
        }
    }

    //use syncissuedetector to scan for code issues but also parse the palette
    auto syncIssueDetector = std::make_shared<AutonomySyncIssueDetector>(_file, _factory);
    err = addSubdetector(syncIssueDetector); //will also run detection process
    if(err.error)
    {
        return err;
    }

    //now access the sync issue detector palette as our own, then merge inherited palette from parent
    _palette = syncIssueDetector->palette();
    for(const auto& entry : _inheritedPalette)
    {
        _palette.insert(entry); // insert only if not already present (file-local definitions take priority)
    }

    _xmlDoc = std::make_shared<tinyxml2::XMLDocument>();
    _xmlDoc->LoadFile(_file.c_str());
    if(_xmlDoc->Error())
    {
        addIssue(std::make_shared<UnfixableAutonomyIssue>(ISSUE_ERROR, _file, 1, "XMLError", _xmlDoc->ErrorStr()));
        return HealthError(true, "Aborted due to earlier issues");
    }

    tinyxml2::XMLElement *fileRootElement = _xmlDoc->RootElement();
    if(!fileRootElement)
    {
        addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
                ISSUE_ERROR, 
                _file,
                1,
                "XMLError",
                "Missing BT root node"));
        
        return HealthError(true, "Aborted due to earlier issues");
    }

    //process includes first to ensure that subtrees will be recognized
    for(
        tinyxml2::XMLElement *includeElement = fileRootElement->FirstChildElement("include");
        includeElement;
        includeElement = includeElement->NextSiblingElement("include"))
    {
        const char *pathAttribute = includeElement->Attribute("path");
        if(!pathAttribute)
        {
            addIssue(
                std::make_shared<UnfixableAutonomyIssue>(
                    ISSUE_ERROR,
                    _file,
                    includeElement->GetLineNum(),
                    "UnspecifiedIncludeError",
                    "Include tag does not specify a path"));
            
            continue;
        }

        std::shared_ptr<AutonomyFileIssueDetector> fileDetector = 
            std::make_shared<AutonomyFileIssueDetector>(
                cwd + "/" + pathAttribute, _project, _factory, _palette);

        err = addSubdetector(fileDetector);
        if(err.error)
        {
            return err;
        }
    }


    //now process trees. Assume our palette is correct
    for(
        tinyxml2::XMLElement *behaviorTree = fileRootElement->FirstChildElement("BehaviorTree");
        behaviorTree;
        behaviorTree = behaviorTree->NextSiblingElement("BehaviorTree"))
    {
        auto treeDetector = std::make_shared<AutonomyTreeIssueDetector>(_file, cwd, behaviorTree, _factory, _palette);
        err = addSubdetector(treeDetector); //function will run detector
        if(err.error)
        {
            return err;
        }
    }

    return HealthError(false, "");
}


NodeManifests AutonomyFileIssueDetector::palette() const
{
    return _palette;
}


std::string AutonomyFileIssueDetector::file() const
{
    return _file;
}


HealthError AutonomyFileIssueDetector::checkFileInProject()
{
    tinyxml2::XMLDocument projDoc;
    projDoc.LoadFile(_project.c_str());
    if(projDoc.Error())
    {
        addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
                ISSUE_ERROR, 
                _file, 
                1, 
                "ProjectError", 
                ("While loading autonomy project: " + std::string(projDoc.ErrorStr())).c_str()));

        return HealthError(true, "Aborted due to earlier issues");
    }

    tinyxml2::XMLElement *projRootElement = projDoc.RootElement();
    if(!projRootElement)
    {
        addIssue(
            std::make_shared<UnfixableAutonomyIssue>(
                ISSUE_ERROR,
                _project,
                1,
                "XMLError",
                "Project missing BT root node"));
        
        return HealthError(true, "Aborted due to earlier issues");
    }


    bool hasFileInProject = false;
    for(
        tinyxml2::XMLElement *includeTag = projRootElement->FirstChildElement("include");
        includeTag;
        includeTag = includeTag->NextSiblingElement("include"))
    {
        const char *path = includeTag->Attribute("path");
        if(!path)
        {
            addIssue(
                std::make_shared<UnfixableAutonomyIssue>(
                    ISSUE_ERROR,
                    _project,
                    includeTag->GetLineNum(),
                    "XMLError",
                    "Include tag missing path"));
            
            return HealthError(true, "Aborted due to earlier issues");
        }

        if(_file == path)
        {
            hasFileInProject = true;
            break;
        }
    }

    if(!hasFileInProject)
    {
        addIssue(std::make_shared<AutonomyOmittedIssue>(_file, _project));
        return HealthError(true, "Aborted due to earlier issues");
    }

    return HealthError(false, "");
}