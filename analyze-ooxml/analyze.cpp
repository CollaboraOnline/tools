#include <libxml/parser.h>
#include <string>
#include <string.h>
#include <dirent.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <regex>

std::unordered_map<std::string, int> counts;
std::unordered_set<std::string> enums;
// Normally all items are counted as totals, but they can be also counted only once per each document.
bool onePerDoc = false;
// If counting per document, this contains all those already counted for this document.
std::unordered_set<std::string> isInDoc;
bool verbose = false;
bool sortByCount = true;
bool namesOnly = false;
bool tagDescriptions = false;
bool html = false;
bool addSource = false;
std::multimap< std::string, std::string > tagDescriptionsMap;
std::multimap< std::string, std::string > sourceDocuments;
std::string sourceDocument;

static std::string simplify( std::string_view str )
{
    if( str.empty())
        return "";
    size_t start = 0;
    while( start < str.size() && isspace( str[ start ] ))
        ++start;
    size_t end = str.size();
    while( end > 0 && isspace( str[ end - 1 ] ))
        --end;
    return std::string( str.substr( start, end - start ));
}

void countItem( const std::string& item )
{
    if( onePerDoc )
    {
        if( isInDoc.find( item ) == isInDoc.end())
            isInDoc.insert( item );
        else
            return; // already counted for this doc
    }

    ++counts[ item ]; // default-inserts if needed
    if( addSource )
    {
        if( sourceDocuments.count( item ) < 5 )
            sourceDocuments.emplace( item, sourceDocument );
    }
}

void parse( xmlNode* node, const std::string& indent )
{
    for(; node; node = node->next )
    {
        if( node->type == XML_ELEMENT_NODE )
        {
            std::string prefix;
            if( node->ns && node->ns->prefix )
                prefix = (const char*)node->ns->prefix;
            //std::cout << indent << "E" << prefix << ( !prefix.empty() ? ":" : "" ) << node->name << std::endl;
            std::string elemName = !prefix.empty() ? prefix + ":" + (const char*)node->name : (const char*)node->name;
            countItem( elemName );
            for( xmlAttr* attr = node->properties; attr; attr = attr->next )
            {
                std::string attrPrefix;
                if( attr->ns && attr->ns->prefix )
                    attrPrefix = (const char*)attr->ns->prefix;
                std::string attrName = !attrPrefix.empty() ? attrPrefix + ":" + (const char*)attr->name
                    : (const char*)attr->name;
                attrName = elemName + "/" + attrName;
                xmlChar* val = xmlNodeListGetString( node->doc, attr->children, 1 );
                //std::cout << "A" << attr->name << "=" << val << std::endl;
                std::string attrVal( (const char*)val );
                if( enums.find( attrVal ) != enums.end())
                    countItem( attrName + "=\"" + attrVal + "\"" );
                else
                    countItem( attrName );
                xmlFree(val);
            }
            parse( node->children, indent + "  " );
        }
    }
}

void parseFile( const char* file )
{
    if( verbose )
        std::cout << file << ":" << std::endl;
    xmlDoc* doc = xmlReadFile( file, nullptr, XML_PARSE_RECOVER );
    if( doc == nullptr )
    {
        std::cerr << "Cannot open " << file << std::endl;
        abort();
    }
    xmlNode* node = xmlDocGetRootElement( doc );
    parse( node, "" );
    xmlFreeDoc( doc );
}

void checkDir( const char* dirname )
{
    DIR* dir = opendir( dirname );
    if( dir == nullptr )
    {
        std::cerr << "Cannot open " << dirname << std::endl;
        abort();
    }
    while( dirent* ent = readdir( dir ))
    {
        if( strcmp( ent->d_name, "." ) == 0 || strcmp( ent->d_name, ".." ) == 0 )
            continue;
        if( strlen( ent->d_name ) > 4 && strcmp( ent->d_name + strlen( ent->d_name ) - 4, ".xml") == 0 )
            parseFile(( std::string( dirname ) + "/" + ent->d_name ).c_str());
        else if( ent->d_type == DT_DIR )
            checkDir(( std::string( dirname ) + "/" + ent->d_name ).c_str());
    }
    closedir( dir );
}

std::string tagDescription( std::string tag )
{
    size_t slash = tag.find( '/' );
    if( slash != std::string::npos )
        tag = tag.substr( 0, slash );
    size_t colon = tag.rfind( ':' );
    if( colon != std::string::npos )
        tag = tag.substr( colon + 1 );
    auto it = tagDescriptionsMap.find( std::string( tag ));
    std::string description;
    if( it != tagDescriptionsMap.end())
    {
        std::string description = it->second;
        ++it;
        if( it != tagDescriptionsMap.end() && it->first == tag )
            description = "(?)" + description;
        return description;
    }
    return "??";
}

std::string makeSourceLink( const std::string source )
{
    static std::regex abi( "^abi([0-9])*.*" );
    static std::regex fdo( "^fdo([0-9])*.*" );
    static std::regex kde( "^kde([0-9])*.*" );
    static std::regex moz( "^moz([0-9])*.*" );
    static std::regex ooo( "^ooo([0-9])*.*" );
    static std::regex suse( "^suse([0-9])*.*" );
    static std::regex novell( "^novell([0-9])*.*" ); // the same as suse
    static std::regex tdf( "^tdf([0-9])*.*" );
    std::smatch match;
    if( std::regex_match( source, match, abi ) && match.size() == 2 )
        return "https://bugzilla.abisource.com/show_bug.cgi?id=" + match[ 1 ].str();
    if( std::regex_match( source, match, fdo ) && match.size() == 2 )
        return "https://bugs.freedesktop.org/show_bug.cgi?id=" + match[ 1 ].str();
    if( std::regex_match( source, match, kde ) && match.size() == 2 )
        return "https://bugs.kde.org/show_bug.cgi?id=" + match[ 1 ].str();
    if( std::regex_match( source, match, moz ) && match.size() == 2 )
        return "https://bugzilla.mozilla.org/show_bug.cgi?id=" + match[ 1 ].str();
    if( std::regex_match( source, match, ooo ) && match.size() == 2 )
        return "https://bz.apache.org/ooo/show_bug.cgi?id=" + match[ 1 ].str();
    if(( std::regex_match( source, match, suse ) || std::regex_match( source, match, novell ) ) && match.size() == 2 )
        return "https://bugzilla.suse.com/show_bug.cgi?id=" + match[ 1 ].str();
    if( std::regex_match( source, match, tdf ) && match.size() == 2 )
        return "https://bugs.documentfoundation.org/show_bug.cgi?id=" + match[ 1 ].str();
    return "";
}

void printStats()
{
    std::vector<std::string> sorted;
    for( std::pair< std::string, int > it : counts )
        sorted.push_back( it.first );
    // sort alphabetically and then by count, so that equal counts are alphabetically sorted
    std::sort(sorted.begin(), sorted.end());
    if(sortByCount)
        std::stable_sort(sorted.begin(), sorted.end(),
            [](const std::string& l, const std::string& r) { return counts[ l ] > counts[ r ]; });
    if(html)
    {
        if( namesOnly )
        {
            std::cerr << "Cannot use -n with -h" << std::endl;
            abort();
        }
        std::cout << "<!DOCTYPE html>\n";
        std::cout << "<html>\n<body>\n";
        std::cout << "<table>\n";
        std::cout << "<tr><th>Count</th><th>Tag</th>";
        if( tagDescriptions )
            std::cout << "<th>Description</th>";
        if( addSource )
            std::cout << "<th>Sources</th>";
        std::cout << "</tr>\n";
        for( const std::string& tag : sorted )
        {
            std::cout << "<tr><td>" << counts[ tag ] << "</td><td>" << tag << "</td>";
            if( tagDescriptions )
            {
                std::string description = tagDescription( tag );
                std::cout << "<td>" << description << "</td>";
            }
            if( addSource )
            {
                std::cout << "<td>";
                for( auto source = sourceDocuments.find( tag );
                     source != sourceDocuments.end() && source->first == tag;
                     ++source )
                {
                    std::string sourceLink = makeSourceLink( source->second );
                    if( sourceLink.empty())
                        std::cout << " " << source->second;
                    else
                        std::cout << " <a href=\"" << sourceLink << "\">" << source->second << "</a>";
                }
                std::cout << "</td>";
            }
            std::cout << "</tr>\n";
        }
        std::cout << "</table>\n";
        std::cout << "</body>\n</html>\n";
    }
    else
    {
        if(namesOnly)
            for( const std::string& tag : sorted )
                std::cout << tag << std::endl;
        else
        {
            for( const std::string& tag : sorted )
            {
                std::cout << counts[ tag ] << " " << tag;
                if( tagDescriptions )
                {
                    std::string description = tagDescription( tag );
                    std::cout << " - " << description << std::endl;
                }
                if( addSource )
                {
                    for( auto source = sourceDocuments.find( tag );
                         source != sourceDocuments.end() && source->first == tag;
                         ++source )
                        std::cout << " " << source->second;
                }
                std::cout << std::endl;
            }
        }
    }
}

void readEnums()
{
    std::ifstream file( "enums.txt" );
    if( !file )
    {
        std::cerr << "Cannot open enums.txt" << std::endl;
        abort();
    }
    for( std::string line; std::getline( file, line ); )
        enums.insert( line );
}

void readTagDescriptions()
{
    std::ifstream file( "tagdescriptions.txt" );
    if( !file )
    {
        std::cerr << "Cannot open tagdescriptions.txt" << std::endl;
        abort();
    }
    for( std::string line; std::getline( file, line ); )
    {
        if( line.empty())
            continue;
        size_t pos = line.find( '#' );
        if( pos == std::string::npos )
        {
            std::cerr << "Incorrect tagdescriptions.txt format" << std::endl;
            abort();
        }
        std::string tagname = simplify( line.substr( 0, pos ));
        std::string description = simplify( line.substr( pos + 1 ));
        tagDescriptionsMap.emplace( tagname, description );
    }
}

int main(int argc, char* argv[])
{
    LIBXML_TEST_VERSION
    int arg = 1;
    // options first
    bool enums = true;
    for( ; arg < argc; ++arg )
    {
        if( strcmp( argv[ arg ], "-e" ) == 0 )
            enums = false;
        else if( strcmp( argv[ arg ], "-p" ) == 0 )
            onePerDoc = true;
        else if( strcmp( argv[ arg ], "-v" ) == 0 )
            verbose = true;
        else if( strcmp( argv[ arg ], "-c" ) == 0 )
            sortByCount = false;
        else if( strcmp( argv[ arg ], "-n" ) == 0 )
            namesOnly = true;
        else if( strcmp( argv[ arg ], "-d" ) == 0 )
            tagDescriptions = true;
        else if( strcmp( argv[ arg ], "-h" ) == 0 )
            html = true;
        else if( strcmp( argv[ arg ], "-s" ) == 0 )
            addSource = true;
        else
            break;
    }
    if( enums )
        readEnums();
    if( tagDescriptions )
        readTagDescriptions();
    for( ; arg < argc; ++arg )
    {
        isInDoc.clear();
        if( addSource )
        {
            sourceDocument = argv[ arg ];
            if( sourceDocument.size() > 4 && sourceDocument.substr( sourceDocument.size() - 4 ) == ".dir" )
                sourceDocument = sourceDocument.substr( 0, sourceDocument.size() - 4 );
        }
        checkDir( argv[ arg ] );
    }
    xmlCleanupParser();
    printStats();
}
