#include "behaviortree/behaviortree_health.hpp"
#include <behaviortree/tinyxml2.h>

std::string AutonomyIssue::fileAndLine(const std::string& file, const tinyxml2::XMLElement *element)
{
    return file + ": " + std::to_string(element->GetLineNum());
}


AutonomyIssue::AutonomyIssue(
    const AutonomyIssueSeverity& severity,
    const std::string& file,
    int line,
    const std::string& type,
    const std::string& description)
 : _severity(severity),
   _file(file),
   _line(line),
   _type(type),
   _description(description) { }


AutonomyIssueSeverity AutonomyIssue::severity() const
{
    return _severity;
}


std::string AutonomyIssue::file() const
{
    return _file;
}


int AutonomyIssue::line() const
{
    return _line;
}


std::string AutonomyIssue::type() const
{
    return _type;
}


std::string AutonomyIssue::description() const
{
    return _description;
}


std::string AutonomyIssue::issue(bool colorize) const
{
    std::string 
        styler = "",
        termReset = "";

    if(colorize)
    {
        switch(severity())
        {
            case ISSUE_WARN:
                styler = TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_YELLOW, TERM_COLOR_DEFAULT));
                termReset = TERM_RESET;
                break;
            case ISSUE_ERROR:
                styler = TERM_STYLE(TERM_BOLD, TERM_COLOR(TERM_RED, TERM_COLOR_DEFAULT));
                termReset = TERM_RESET;
                break;
            default:
                styler = TERM_STYLE(TERM_BOLD, TERM_COLOR(TERM_WHITE, TERM_COLOR_DEFAULT));
                termReset = TERM_RESET;
        }
    }

    return styler + "[" + type() + "]" + termReset + " (" + file() + ":" + std::to_string(line()) + "): " + description();
}
