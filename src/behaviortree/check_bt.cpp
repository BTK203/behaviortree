#include "behaviortree/behaviortree_health.hpp"
#include <iostream>

//
// BT CHECKER EXECUTABLE
// A simple command-line tool to check for potential behavior tree errors
//

using namespace BT;

void displayHelp();

int main(int argc, char **argv)
{
    //
    // Parse args
    //
    
    std::string 
        treePath = "",
        indexFile = "";

    bool autoFix = false;
    std::vector<std::string> autoFixNames;
    
    for(int i = 1; i < argc; i++)
    {
        std::string arg(argv[i]);
        if(arg == "-h" || arg == "--help")
        {
            displayHelp();
            return 0;
        } else if(arg == "-i" || arg == "--index-file")
        {
            if(i + 1 >= argc)
            {
                std::cout << "Not enough arguments for option -i (--index-file)" << std::endl;
                return 1;
            }

            indexFile = argv[i + 1];
            i++;
        } else if(arg == "-f" || arg == "--fix-all")
        {
            std::cout << "Auto-fix is ENABLED" << std::endl;
            autoFix = true;
        } else if(!treePath.empty() && !autoFix)
        {
            std::cout << "Multiple tree paths specified\n\n" << std::endl;
            displayHelp();
            return 1;
        } else
        {
            if(autoFix)
            {
                std::string name = argv[i];
                std::cout << "Auto-fixing " << name << std::endl;
                autoFixNames.push_back(name);
            } else
            {
                treePath = argv[i];
            }
        }
    }

    if(treePath.empty())
    {
        if(autoFix)
        {
            std::cout << "Cannot use -f or --fix-all before tree name. Please specify the tree name BEFORE -f or --fix-all.\n\n";
        } else
        {
            std::cout << "Please specify a tree.\n\n";
        }
        displayHelp();
        return 1;
    }

    //
    // create factory
    //
    auto factory = std::make_shared<BehaviorTreeFactory>();
    registerPluginsForFactory(factory, indexFile);

    bool checkLoopActive = true;
    while(checkLoopActive)
    {
        // dont want to infinite loop so set condition var to false.
        // if user makes fixes, they may want to re-run the check, in which case the var will
        // be flipped back to true
        checkLoopActive = false;

        //
        // now check the tree
        //
        AutonomyFileIssueDetector detector(treePath, factory);

        //
        // print issues
        // 
        HealthError err = detector.detect();
        std::vector<AutonomyIssue::Ptr> issues = detector.issues();

        if(issues.size() > 0)
        {
            bool fixed = false; // true if any issues were fixed

            std::vector<AutonomyIssue::Ptr> fixable;
            std::cout << "Summary: " << issues.size() << " issues found (listed below):\n\n";
            for(AutonomyIssue::Ptr iss : issues)
            {
                std::string msg = iss->issue();
                std::cout << msg << "\n";
                if(iss->fixable())
                {
                    if(autoFix)
                    {
                        auto it = std::find(autoFixNames.begin(), autoFixNames.end(), iss->type());
                        
                        // if this issue contained by autoFixNames or autoFixNames is empty (fix all)
                        if(it != autoFixNames.end() || autoFixNames.empty())
                        {
                            err = iss->fix();
                            if(!err.error)
                            {
                                std::cout << TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_GREEN, TERM_COLOR_DEFAULT)) << "...Auto-fix successfully applied\n" << TERM_RESET;
                                fixed = true;
                            } else
                            {
                                std::cout << TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_RED, TERM_COLOR_DEFAULT)) << "...Auto-fix failed: " << err.message << "\n" << TERM_RESET;
                            }
                        }
                    } 

                    if(!fixed)
                    {
                        fixable.push_back(iss);
                    }
                }
            }

            if(err.error)
            {
                std::cout << TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_RED, TERM_COLOR_DEFAULT)) << "\nDetector error: " << err.message << TERM_RESET << std::endl;
            } else
            {
                std::cout << "\nDetector finished cleanly" << std::endl;
            }

            //
            // if issues are fixable, ask user if they would like to fix them
            //

            std::string response;

            if(fixable.size() > 0)
            {
                std::cout << "\n" << fixable.size() << " issues are fixable. Would you like to review them? [y/n]: ";
                
                std::cin >> response;
                if(response == "y" || response == "Y")
                {
                    for(size_t i = 0; i < fixable.size(); i++)
                    {
                        AutonomyIssue::Ptr fixIss = fixable.at(i);
                        std::cout << "\nIssue " << i + 1 << ":\n";
                        std::cout << fixIss->issue() << "\n";
                        std::cout << "  Solution: " << fixIss->solution() << "\n";
                        std::cout << "Would you like to apply the fix? [y/n]: ";
                        std::cin >> response;
                        if(response == "y" || response == "Y")
                        {
                            err = fixIss->fix();
                            if(err.error)
                            {
                                std::cout << TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_RED, TERM_COLOR_DEFAULT)) << "Unable to apply fix: " + err.message << TERM_RESET << std::endl;
                            } else
                            {
                                std::cout << TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_GREEN, TERM_COLOR_DEFAULT)) << "Fix successfully applied" << TERM_RESET << std::endl;
                                fixed = true;
                            }
                        }
                    }
                }
            }

            // if the program fixed issues, ask user if they want to rerun checks to verify
            if(fixed)
            {
                // doing something funky here. if auto-fix was active on the first loop, then the auto-fixable issues should be resolved, but we want to re-run the check automatically to verify
                // (also because if the user specified auto-fix, they most likely also want the command to be non-interactive). we don't want an infinite loop, and if any auto-checks failed to 
                // fix their issues, we want the user to know about it. so we will automatically re-run the check and then set auto-fix to false, so any further issues must be acknowledged by the user
                if(autoFix)
                {
                    std::cout << "Re-running check to verify auto-fixed issues are resolved..." << std::endl;
                    checkLoopActive = true;
                    autoFix = false;
                } else
                {
                    std::cout << "Would you like to re-run the check to verify fixes? [y/n]:";
                    std::cin >> response;
                    if(response == "y" || response == "Y")
                    {
                        std::cout << "Re-running check..." << std::endl;
                        checkLoopActive = true;
                    }
                }
            }
        } else
        {
            if(err.error)
            {
                std::cout << TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_RED, TERM_COLOR_DEFAULT)) << "Detector error: " << err.message << TERM_RESET << std::endl;
            } else
            {
                std::cout << TERM_STYLE(TERM_NONE, TERM_COLOR(TERM_GREEN, TERM_COLOR_DEFAULT)) << "No issues found" << TERM_RESET << std::endl;
            }
        }
    }
}


void displayHelp()
{
    std::cout << "Usage: check [-h] [-i <file>] <tree> [-f <names>]\n";
    std::cout << "\n";
    std::cout << "check allows a user to detect behavior tree issues before running.\n";
    std::cout << "\n";
    std::cout << "Positional arguments: \n";
    std::cout << "tree                      Path to the tree to check\n";
    std::cout << "\n";
    std::cout << "options: \n";
    std::cout << "-h, --help                show this help message and exit\n";
    std::cout << "-i, --index-file <file>   use this to specify the name of the package index file\n";
    std::cout << "-f, --fix-all <names>     specify list of issue names to automatically fix without asking. no names specified assumes all\n";
}
