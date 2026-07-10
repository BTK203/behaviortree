#include "behaviortree/behaviortree.hpp"
#include "behaviortree/uwrt_node_types.hpp"

#include "ament_index_cpp/get_package_prefix.hpp"
#include "ament_index_cpp/get_package_share_directory.hpp"
#include "ament_index_cpp/get_resources.hpp"

#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

#include <fstream>

using namespace std::chrono_literals;

//static UWRT nodes manifest
std::unordered_map<std::string, UwrtPortInformation> UwrtNodesManifest::manifest = {};

std::string getEnvVar(const char *name)
{
    const char *env = std::getenv(name);
    if (!env)
    {
        throw std::invalid_argument(name);
    }

    return std::string(env);
}


std::vector<std::string> splitString(const std::string& s, char c)
{
    std::vector<std::string> parts;
    size_t base = 0;
    while(base != std::string::npos)
    {
        size_t
            next = s.find(c, base + 1),
            num = next - base;

        if(next == std::string::npos)
        {
            num = std::string::npos;
        }

        // make sure we wont accidentally catch a c in the plugin name
        if(s[base] == c)
        {
            base++; // skip c
            num--; // take 1 less chars because we skipping one
        }

        std::string part = s.substr(base, num);
        parts.push_back(part);

        base = next;
    }

    return parts;
}


std::string resolvePackageUri(const std::string& uri)
{
    const std::string prefix = "package://";

    if (uri.rfind(prefix, 0) != 0)
    {
        return uri; // already a normal path
    }

    auto remainder = uri.substr(prefix.size());

    auto slash = remainder.find('/');
    if (slash == std::string::npos)
    {
        throw std::runtime_error("Invalid package URI: " + uri);
    }

    std::string package_name = remainder.substr(0, slash);
    std::string relative_path = remainder.substr(slash + 1);

    std::string package_share =
        ament_index_cpp::get_package_share_directory(package_name);

    return package_share + "/" + relative_path;
}


std::vector<std::string> getPluginPackagePrefixesFromIndex(const std::string& indexFile)
{
    std::vector<std::string> prefixes;

    // read the file
    std::string iContents;
    std::ifstream iFile;
    iFile.open(indexFile, std::istream::in);
    if(iFile.fail())
    {
        throw std::runtime_error(strerror(errno));
    }
    iFile >> iContents;
    iFile.close();

    // iContents is a string containing package names separated by newlines
    std::vector<std::string> pkgNames = splitString(iContents, '\n');
    
    for(std::string name : pkgNames)
    {
        if(name.empty())
        {
            continue;
        }

        std::string prefix = ament_index_cpp::get_package_prefix(name);
        prefixes.push_back(prefix);
    }

    return prefixes;
}


std::vector<std::string> getAllPluginPackagePrefixes()
{
    std::map<std::string, std::string> resrcMap = ament_index_cpp::get_resources("behaviortree");
    std::vector<std::string> paths;
    for(auto p : resrcMap)
    {
        paths.push_back(p.second);
    }

    return paths;
}


std::vector<std::string> getPluginPackagePrefixes(const std::string& indexFile)
{
    std::vector<std::string> prefixes;
    if(indexFile.empty())
    {
        prefixes = getAllPluginPackagePrefixes();
    } else 
    {
        prefixes = getPluginPackagePrefixesFromIndex(indexFile);
    }

    return prefixes;
}


std::vector<std::string> getPluginPaths(const std::vector<std::string>& prefixes)
{
    std::vector<std::string> paths;

    for(std::string pkg : prefixes)
    {
        // try to access the file <pkg>/share/ament_index/resource_index/behaviortree/<pkg_name>
        // this file will contain a semicolon-separated list of plugins to load from <pkg>/lib

        size_t lastSlash = pkg.rfind('/');
        if(lastSlash == std::string::npos)
        {
            continue;
        }

        std::string pkg_name = pkg.substr(lastSlash + 1);
        std::string rPath = pkg + "/share/ament_index/resource_index/behaviortree/" + pkg_name;
        if(!std::filesystem::exists(rPath))
        {
            std::cout << "Warning: expected to find resource " << rPath << " but it did not exist." << std::endl;
            continue;
        }

        // read in resource file
        std::string rContents;
        std::ifstream rFile;
        rFile.open(rPath, std::ifstream::in);
        rFile >> rContents;
        rFile.close();

        // read the plugin names out of the file
        std::vector<std::string> pluginNames = splitString(rContents, ';');

        // turn the plugin names into paths
        for(std::string name : pluginNames)
        {
            std::string
                filename = "lib" + name + ".so",
                filepath = pkg + "/lib/" + filename;
            
            paths.push_back(filepath);
        }
    }

    return paths;
}


void registerPluginsForFactory(const std::shared_ptr<BT::BehaviorTreeFactory>& factory, const std::string& indexFile) {
    std::vector<std::string>
        prefixes = getPluginPackagePrefixes(indexFile),
        paths = getPluginPaths(prefixes);

    for(std::string plugin : paths)
    {
        factory->registerFromPlugin(plugin);
    }
}


void initRosForTree(BT::Tree& tree, rclcpp::Node::SharedPtr rosNode) {
    //initialize static variables of UwrtBtNode
    ROSEnabledNode::staticInit(rosNode);

    // give each BT node access to our ROS context

    auto visitor = [&rosNode] (BT::TreeNode *node)
    {
        if (auto uwrtNode = dynamic_cast<ROSEnabledNode *>(node))
        {
            uwrtNode->init(rosNode);
        }
    };

    tree.applyVisitor(visitor);
}


geometry_msgs::msg::Pose doTransform(const geometry_msgs::msg::Pose& relative, const geometry_msgs::msg::TransformStamped& transform) {
    geometry_msgs::msg::Pose result;
    tf2::doTransform(relative, result, transform);
    return result;
}


geometry_msgs::msg::Vector3 pointToVector3(const geometry_msgs::msg::Point& pt) {
    geometry_msgs::msg::Vector3 v;
    v.x = pt.x;
    v.y = pt.y;
    v.z = pt.z;
    return v;
}


geometry_msgs::msg::Point vector3ToPoint(const geometry_msgs::msg::Vector3& v) {
    geometry_msgs::msg::Point pt;
    pt.x = v.x;
    pt.y = v.y;
    pt.z = v.z;
    return pt;
}


double vector3Length(const geometry_msgs::msg::Vector3& v) {
    return sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
}


geometry_msgs::msg::Vector3 toRPY(const geometry_msgs::msg::Quaternion& orientation) {
    tf2::Quaternion tf2Orientation;
    tf2::fromMsg(orientation, tf2Orientation);

    geometry_msgs::msg::Vector3 rpy;
    tf2::Matrix3x3(tf2Orientation).getEulerYPR(rpy.z, rpy.y, rpy.x);
    return rpy;
}


geometry_msgs::msg::Quaternion toQuat(const geometry_msgs::msg::Vector3& rpy) {
    tf2::Quaternion tf2Quat;
    tf2Quat.setRPY(rpy.x, rpy.y, rpy.z);
    tf2Quat.normalize();

    return tf2::toMsg(tf2Quat);
}


tf2::Transform geometryMsgsToTf2Transform(const geometry_msgs::msg::TransformStamped& t)
{
    tf2::Quaternion q;
    tf2::Vector3 c;

    tf2::fromMsg(t.transform.rotation, q);
    tf2::fromMsg(t.transform.translation, c);

    tf2::Transform tf2T(q, c);
    return tf2T;
}


geometry_msgs::msg::TransformStamped tf2TransformToGeometryMsgs(const tf2::Transform& t)
{
    geometry_msgs::msg::TransformStamped geomT;
    geomT.transform.rotation = tf2::toMsg(t.getRotation());
    geomT.transform.translation = tf2::toMsg(t.getOrigin());

    return geomT;
}


double distance(const geometry_msgs::msg::Point& point1, const geometry_msgs::msg::Point& point2) {
    return sqrt(pow(point2.x - point1.x, 2) +pow(point2.y - point1.y, 2) + pow(point2.z - point1.z, 2));
}


double distance(const geometry_msgs::msg::Vector3& point1, const geometry_msgs::msg::Vector3& point2) {
    return distance(vector3ToPoint(point1), vector3ToPoint(point2));
}
