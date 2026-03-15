#include <rclcpp/rclcpp.hpp>
#include "behaviortree/behaviortree.hpp"
#include "behaviortree/UWRTLogger.hpp"
#include "behaviortree_cpp/loggers/bt_cout_logger.h"

//
// BT RUNNER EXECUTABLE
// A simple command-line tool to run a behavior tree and a little more
//

using namespace BT;
using namespace std::chrono_literals;

void displayHelp();
void listNodes(const std::shared_ptr<BehaviorTreeFactory>& factory);
void listPackages(const std::string& indexFile);
void listPlugins(const std::string& indexFile);

int main(int argc, char **argv)
{
    //
    // initialize ROS
    //
    rclcpp::init(argc, argv);


    //
    // Parse Args
    //

    std::string 
        treePath = "",
        treeName = "",
        nodeName = "bt",
        indexFile = "";

    bool 
        wantCoutLogger = false,
        wantListNodes = false,
        wantListPackages = false,
        wantListPlugins = false;

    for(int i = 1; i < argc; i++)
    {
        std::string arg(argv[i]);
        if(arg == "--help" || arg == "-h")
        {
            displayHelp();
            return 0;
        } else if(arg == "-n" || arg == "--node-name")
        {
            if(i + 1 >= argc)
            {
                std::cout << "Not enought arguments for option -n (--node-name)" << std::endl;
                return 1;
            }

            nodeName = argv[i + 1];
            i++;
        } else if(arg == "-c" || arg == "--cout")
        {
            wantCoutLogger = true;
        } else if(arg == "-p" || arg == "--path")
        {
            if(i + 1 >= argc)
            {
                std::cout << "Not enought arguments for option -p (--path)" << std::endl;
                return 1;
            }

            treePath = argv[i + 1];
            i++;
        } else if(arg == "-i" || arg == "--index-file")
        {
            if(i + 1 >= argc)
            {
                std::cout << "Not enought arguments for option -i (--index-file)" << std::endl;
                return 1;
            }

            indexFile = argv[i + 1];
            i++;
        } else if(arg == "-l" || arg == "--list-nodes")
        {
            wantListNodes = true;
        } else if(arg == "-k" || arg == "--list-packages")
        {
            wantListPackages = true;
        } else if(arg == "-u" || arg == "--list-plugins")
        {
            wantListPlugins = true;
        } else if(!treeName.empty())
        {
            std::cout << "Multiple tree names specified\n\n" << std::endl;
            displayHelp();
            return 1;
        } else
        {
            treeName = argv[i];
        }

    }

    //
    // check if user wants a one-off task
    //
    if(wantListPackages)
    {
        listPackages(indexFile);
        return 0;
    }

    if(wantListPlugins)
    {
        listPlugins(indexFile);
        return 0;
    }

    //
    // Create a ros node
    //
    rclcpp::Node::SharedPtr rosnode = std::make_shared<rclcpp::Node>(nodeName);

    //
    // Now start the tree
    //

    NodeStatus nodeStatus = NodeStatus::SUCCESS;
    // create the behavior tree factory context
    auto factory = std::make_shared<BehaviorTreeFactory>();
    
    // load our plugins from ament index
    registerPluginsForFactory(factory, indexFile);

    // if list nodes is desired, now do so and exit
    if(wantListNodes)
    {
        listNodes(factory);
        return 0;
    }
    
    // create the tree to run
    Tree tree;

    if(treeName.empty())
    {
        std::cout << "Please specify a tree.\n\n";
        displayHelp();
        return 1;
    }

    if(treePath.empty())
    {
        // if we get here, then the path was never specified by the user. This means
        // that they just want to run a standalone tree.
        tree = factory->createTreeFromFile(treeName);
    } else 
    {
        // load a tree (or project file) and create a tree by name out of that
        factory->registerBehaviorTreeFromFile(treePath);
        tree = factory->createTree(treeName);
    }    
    
    initRosForTree(tree, rosnode);

    //
    // set up loggers
    //
    std::shared_ptr<StdCoutLogger> coutLogger;
    if(wantCoutLogger)
    {
        coutLogger = std::make_shared<StdCoutLogger>(tree);
    }

    std::shared_ptr<UwrtLogger> uwrtLogger = std::make_shared<UwrtLogger>(tree, rosnode);

    //
    // run the tree
    //

    // set up idle sleep rate
    rclcpp::Rate loop_rate(30ms);

    // start ticking the tree with feedback
    // keep executing tick until it returns either SUCCESS or FAILURE
    auto tickStatus = NodeStatus::RUNNING;
    while (!isStatusCompleted(tickStatus))
    {
        tickStatus = tree.tickOnce();
        loop_rate.sleep();
    }

    std::string resultStr = "SUCCESS";
    switch(tickStatus) {
        case NodeStatus::FAILURE:
            resultStr = "FAILURE";
            break;
        case NodeStatus::IDLE:
            resultStr = "IDLE";
            break;
        case NodeStatus::RUNNING:
            resultStr = "RUNNING";
            break;
        default:
            resultStr = "SUCCESS";
            break;
    }

    RCLCPP_INFO(rosnode->get_logger(), "Tree ended with status %s", resultStr.c_str());

    //
    // shutdown and exit
    //
    rclcpp::shutdown();
    return (int) nodeStatus - (int) NodeStatus::SUCCESS;
}


void displayHelp()
{
    std::cout << "Usage: bt [-hlkuc] [-n <name>] [-i <file>] [-p <path>] <tree>\n";
    std::cout << "\n";
    std::cout << "bt allows a user to run a behavior tree on the command line.\n";
    std::cout << "\n";
    std::cout << "Positional arguments: \n";
    std::cout << "tree                      if -p specified, name of the tree to run. Otherwise, the file path\n";
    std::cout << "\n";
    std::cout << "Options: \n";
    std::cout << "-h, --help                show this help message and exit\n";
    std::cout << "-l, --list-nodes          show a list of supported nodes and exit\n";
    std::cout << "-k, --list-packages       show a list of packages which provide plugins and exit\n";
    std::cout << "-u, --list-plugins        show a list of plugin files and exit\n";
    std::cout << "-n, --node-name <name>    use this to specify the name of the ROS node\n";
    std::cout << "-i, --index-file <file>   use this to specify the name of the package index file\n";
    std::cout << "-p, --path <path>         use this to specify a project or tree file. If specified, the \n";
    std::cout << "                          positional argument will be the NAME of the tree, not the path\n";
    std::cout << "-c, --cout                enable cout logger\n";
}


void listNodes(const std::shared_ptr<BehaviorTreeFactory>& factory)
{
    for(auto manifest : factory->manifests())
    {
        std::cout << manifest.first << "\n";
    }
}


void listPackages(const std::string& indexFile)
{
    std::vector<std::string> packages;

    try
    {
        packages = getPluginPackagePrefixes(indexFile); // returns a list of package prefixes
    } catch(std::runtime_error& ex)
    {
        std::cout << "Failed to open index file " << indexFile << ". Got error: " << ex.what() << std::endl;
    }

    for(std::string pkg : packages)
    {
        size_t lastSlash = pkg.rfind('/');
        if(lastSlash == std::string::npos)
        {
            continue;
        }

        std::string pkg_name = pkg.substr(lastSlash + 1);
        std::cout << pkg_name << "\n";
    }
}


void listPlugins(const std::string& indexFile)
{
    std::vector<std::string> packages;
    try
    {
        packages = getPluginPackagePrefixes(indexFile);
    } catch(std::runtime_error& ex)
    {
        std::cout << "Failed to open index file " << indexFile << ". Got error: " << ex.what() << std::endl;
    }

    std::vector<std::string> plugins = getPluginPaths(packages);

    for(std::string path : plugins)
    {
        std::cout << path << "\n";
    }
}
