/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

#include <boost/lexical_cast.hpp>
#include <iomanip>

#include <libxml/parser.h>
#include <libxml/tree.h>
#ifdef LIBXML_XINCLUDE_ENABLED
#include <libxml/xinclude.h>
#else
#error "libxml2 was not built with required XInclude processing enabled."
#endif
#ifdef LIBXML_SCHEMAS_ENABLED
#include <libxml/xmlschemas.h>
#else
#error "libxml2 was not built with required Schema processing enabled."
#endif
#include "ProtocolConfigImpl.h"


namespace LLDLEP
{
namespace ProtocolStrings
{
// signal/message strings

const std::string Peer_Discovery = "Peer_Discovery";
const std::string Peer_Offer = "Peer_Offer";
const std::string Session_Initialization = "Session_Initialization";
const std::string Session_Initialization_Response =
    "Session_Initialization_Response";
const std::string Session_Termination = "Session_Termination";
const std::string Session_Termination_Response = "Session_Termination_Response";
const std::string Session_Update = "Session_Update";
const std::string Session_Update_Response = "Session_Update_Response";
const std::string Destination_Up = "Destination_Up";
const std::string Destination_Up_Response = "Destination_Up_Response";
const std::string Destination_Down = "Destination_Down";
const std::string Destination_Down_Response = "Destination_Down_Response";
const std::string Destination_Update = "Destination_Update";
const std::string Link_Characteristics_Request = "Link_Characteristics_Request";
const std::string Link_Characteristics_Response = "Link_Characteristics_Response";
const std::string Heartbeat = "Heartbeat";
const std::string Destination_Announce = "Destination_Announce";
const std::string Destination_Announce_Response = "Destination_Announce_Response";

// data item strings

const std::string Version = "Version";
const std::string Port = "Port";
const std::string Peer_Type = "Peer_Type";
const std::string MAC_Address = "MAC_Address";
const std::string IPv4_Address = "IPv4_Address";
const std::string IPv6_Address = "IPv6_Address";
const std::string Status = "Status";
const std::string Heartbeat_Interval = "Heartbeat_Interval";
const std::string Link_Characteristics_Response_Timer = "Link_Characteristics_Response_Timer";
const std::string IPv4_Attached_Subnet = "IPv4_Attached_Subnet";
const std::string IPv6_Attached_Subnet = "IPv6_Attached_Subnet";
const std::string Extensions_Supported = "Extensions_Supported";
const std::string Experimental_Definition = "Experimental_Definition";
const std::string IPv4_Connection_Point = "IPv4_Connection_Point";
const std::string IPv6_Connection_Point = "IPv6_Connection_Point";

// required metrics strings

const std::string Maximum_Data_Rate_Receive = "Maximum_Data_Rate_Receive";
const std::string Maximum_Data_Rate_Transmit = "Maximum_Data_Rate_Transmit";
const std::string Current_Data_Rate_Receive = "Current_Data_Rate_Receive";
const std::string Current_Data_Rate_Transmit = "Current_Data_Rate_Transmit";
const std::string Latency = "Latency";
const std::string Resources = "Resources";
const std::string Resources_Receive = "Resources_Receive";
const std::string Resources_Transmit = "Resources_Transmit";
const std::string Relative_Link_Quality_Receive = "Relative_Link_Quality_Receive";
const std::string Relative_Link_Quality_Transmit = "Relative_Link_Quality_Transmit";
const std::string Maximum_Transmission_Unit = "Maximum_Transmission_Unit";

// status code strings

const std::string Success = "Success";
const std::string Unknown_Message = "Unknown_Message";
const std::string Invalid_Message = "Invalid_Message";
const std::string Unexpected_Message = "Unexpected_Message";
const std::string Request_Denied = "Request_Denied";
const std::string Timed_Out = "Timed_Out";
const std::string Invalid_Data = "Invalid_Data";
const std::string Invalid_Destination = "Invalid_Destination";
const std::string Not_Interested = "Not_Interested";
const std::string Inconsistent_Data = "Inconsistent_Data";

} // namespace ProtocolStrings
} // namespace LLDLEP

using namespace LLDLEP;
using namespace LLDLEP::internal;

ProtocolConfigImpl::ProtocolConfigImpl(
    const std::string & proto_config_schema,
    const std::string & proto_config_file,
    DlepLoggerPtr logger) :
    version { {0, 0} },
    logger(logger)
{
    std::string err = load_protocol_config(proto_config_schema,
                                           proto_config_file);
    if (err != "")
    {
        throw BadProtocolConfig(err);
    }
}

DlepLoggerPtr
ProtocolConfigImpl::get_logger() const
{
    return logger;
}

//-----------------------------------------------------------------------------
//
// support for loading and validating the XML protocol config file

static void
xml_error_handler(void * ctx, const char * msg, ...)
{
    ProtocolConfigImpl * protocfg = static_cast<ProtocolConfigImpl *>(ctx);
    DlepLoggerPtr logger = protocfg->get_logger();
    va_list ap;
    va_start(ap, msg);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);
    LOG(DLEP_LOG_ERROR, std::ostringstream(buf));
}

static void
xml_warning_handler(void * ctx, const char * msg, ...)
{
    ProtocolConfigImpl * protocfg = static_cast<ProtocolConfigImpl *>(ctx);
    DlepLoggerPtr logger = protocfg->get_logger();
    va_list ap;
    va_start(ap, msg);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);
    LOG(DLEP_LOG_NOTICE, std::ostringstream(buf));
}

std::string
ProtocolConfigImpl::load_protocol_config(const std::string &
                                         proto_config_schema,
                                         const std::string & proto_config_file)
{
    LIBXML_TEST_VERSION
    std::ostringstream msg;
    std::string err = "";
    xmlDocPtr doc = nullptr;
    xmlParserCtxtPtr parser_ctx = nullptr;
    xmlSchemaValidCtxtPtr schema_validate_ctx = nullptr;
    xmlSchemaParserCtxtPtr schema_parser_ctx = nullptr;
    xmlSchemaPtr schema = nullptr;
    int ret = 0;
    int xmlopts = 0;

    msg << "schema=" << proto_config_schema << " config=" << proto_config_file;
    LOG(DLEP_LOG_INFO, msg);

    // Parse the XML config file and store the resulting tree in doc.
    // This will check that it is well-formed XML, but does not
    // validate against the schema yet.

    parser_ctx = xmlNewParserCtxt();
    if (parser_ctx == nullptr)
    {
        err = "XML parser context creation failed";
        goto bailout;
    }

    msg << "parsing " << proto_config_file;
    LOG(DLEP_LOG_DEBUG, msg);

    xmlopts =
        XML_PARSE_NOBLANKS |   // remove useless blank nodes
        XML_PARSE_XINCLUDE |   // do XInclude processing
        XML_PARSE_NONET;       // forbid network access; everything is local

    doc = xmlCtxtReadFile(parser_ctx, proto_config_file.c_str(), nullptr, xmlopts);
    if (doc == nullptr)
    {
        err = "XML parsing failed for " + proto_config_file;
        goto bailout;
    }

    // Do XInclude processing

    ret = xmlXIncludeProcessFlags(doc, xmlopts);
    if (ret < 0)
    {
        err = "XML include processing failed for " + proto_config_file;
        goto bailout;
    }

    msg << "XInclude made " << ret << " substitutions";
    LOG(DLEP_LOG_DEBUG, msg);

    // Load the schema to use to validate the document (doc) parsed above.

    msg << "loading schema " << proto_config_schema;
    LOG(DLEP_LOG_DEBUG, msg);

    schema_parser_ctx = xmlSchemaNewParserCtxt(proto_config_schema.c_str());
    if (schema_parser_ctx == nullptr)
    {
        err = "XML schema parser context creation failed";
        goto bailout;
    }

    xmlSchemaSetParserErrors(schema_parser_ctx,
                             (xmlSchemaValidityErrorFunc)xml_error_handler,
                             (xmlSchemaValidityWarningFunc)xml_warning_handler,
                             this);
    schema = xmlSchemaParse(schema_parser_ctx);
    if (schema == nullptr)
    {
        err = "XML schema parsing failed for " + proto_config_schema;
        goto bailout;
    }

    // Validate the document (doc) with the schema.

    msg << "validating the config against the schema";
    LOG(DLEP_LOG_DEBUG, msg);

    schema_validate_ctx = xmlSchemaNewValidCtxt(schema);
    if (schema_validate_ctx == nullptr)
    {
        err = "XML schema validate context creation failed";
        goto bailout;
    }

    xmlSchemaSetValidErrors(schema_validate_ctx,
                            (xmlSchemaValidityErrorFunc)xml_error_handler,
                            (xmlSchemaValidityWarningFunc)xml_warning_handler,
                            this);
    ret = xmlSchemaValidateDoc(schema_validate_ctx, doc);
    if (ret)
    {
        err = "XML validation failed for " + proto_config_file
              + " err=" + std::to_string(ret);
        goto bailout;
    }

    err = extract_protocol_config(doc);

bailout:
    msg << "cleaning up";
    LOG(DLEP_LOG_DEBUG, msg);

    if (parser_ctx != nullptr)
    {
        xmlFreeParserCtxt(parser_ctx);
    }

    if (doc != nullptr)
    {
        xmlFreeDoc(doc);
    }

    if (schema_parser_ctx != nullptr)
    {
        xmlSchemaFreeParserCtxt(schema_parser_ctx);
    }

    if (schema != nullptr)
    {
        xmlSchemaFree(schema);
    }

    if (schema_validate_ctx != nullptr)
    {
        xmlSchemaFreeValidCtxt(schema_validate_ctx);
    }

    xmlCleanupParser();

    if (err != "")
    {
        LOG(DLEP_LOG_ERROR, std::ostringstream(err));
    }
    return err;
}

void
ProtocolConfigImpl::log_xml_tree(xmlNodePtr start_node, int level)
{
    if (start_node == nullptr)
    {
        return;
    }

    for (xmlNodePtr cur_node = start_node; cur_node; cur_node = cur_node->next)
    {
        std::ostringstream msg;
        xmlChar * content = xmlNodeGetContent(cur_node);
        msg << std::setw(4 * level) << " " << cur_node->name
            << " type: " << cur_node->type
            << " value: " << (char *)content;
        LOG(DLEP_LOG_DEBUG, msg);
        xmlFree(content);
        log_xml_tree(cur_node->children, level + 1);
    }
}

//-----------------------------------------------------------------------------
//
// support for extracing information from the XML tree once it has
// been loaded from the file

std::string
ProtocolConfigImpl::extract_protocol_config(xmlDocPtr doc)
{
    std::string err = "";

    xmlNodePtr root = xmlDocGetRootElement(doc);
    assert(root != nullptr);

    // log_xml_tree(root, 0);

    for (xmlNodePtr node = root->children; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "version")
        {
            extract_version(node->children);
        }
        else if (node_name == "signal_prefix")
        {
            extract_signal_prefix(node->children);
        }
        else if (node_name == "field_sizes")
        {
            extract_field_sizes(node->children);
        }
        else if (node_name == "module")
        {
            extract_module(node->children);
        }
        else
        {
            std::ostringstream msg;
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    }

    return err;
}

template<typename T>
void
ProtocolConfigImpl::log_node_path_and_value(xmlNodePtr node, T val)
{
    long lineno = xmlGetLineNo(node);

    // Build up the pathname to this value for logging.  This is not
    // efficient, but it is simple, and it only happens at startup.

    std::string path(std::string((const char *)node->name));
    while (node->parent != nullptr)
    {
        node = node->parent;
        if (node->name != nullptr)
        {
            path = std::string((const char *)node->name) + "/" + path;
        }
    }
    std::ostringstream msg;
    // XXX I'd like to log the name of the file here too, but
    // node->doc->name is null.
    msg << lineno << ": " << path << " = " << std::boolalpha << val;
    LOG(DLEP_LOG_DEBUG, msg);
}

template<typename T>
T
ProtocolConfigImpl::extract_node_value(xmlNodePtr node)
{
    xmlChar * str = xmlNodeGetContent(node);
    T val;

    // Use boolalpha in case T is bool because the XML string will be
    // "true" or "false", not 0/1.  For other data types, this has no
    // effect.

    std::stringstream((const char *)str) >> std::boolalpha >> val;
    xmlFree(str);

    log_node_path_and_value(node, val);

    return val;
}

// If I don't wrap the specialization of extract_node_value below
// inside its namespace LLDLEP::internal, g++ gives the following
// error:
//
// error: specialization of ‘template<class T> T
// LLDLEP::internal::ProtocolConfigImpl::extract_node_value(xmlNodePtr)’
// in different namespace [-fpermissive]
//
// The template is already in this namespace in the class declaration,
// so what's the problem?  clang accepts it without being wrapped.
// Not sure who is right.  Both compilers accept the code below, so
// we'll go with that.

namespace LLDLEP
{
namespace internal
{

// Specialization for strings.  We don't want to use the generic
// version of this method (above) for strings because when you read a
// string from a stringstream, it stops at the first space it
// encounters.  This would prevent us from having strings with spaces
// in them, and that's too limiting.
template<> std::string
ProtocolConfigImpl::extract_node_value(xmlNodePtr node)
{
    xmlChar * str = xmlNodeGetContent(node);
    std::string val {(const char *)str};
    xmlFree(str);

    log_node_path_and_value(node, val);

    return val;
}

} // namespace internal
} // namespace LLDLEP

void
ProtocolConfigImpl::extract_version(xmlNodePtr node)
{
    for (; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "major")
        {
            version[0] = extract_node_value<std::uint16_t>(node);
        }
        else if (node_name == "minor")
        {
            version[1] = extract_node_value<std::uint16_t>(node);
        }
        else
        {
            std::ostringstream msg;
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    }
}

void
ProtocolConfigImpl::extract_signal_prefix(xmlNodePtr node)
{
    signal_prefix = extract_node_value<std::string>(node);
}

void
ProtocolConfigImpl::extract_field_sizes(xmlNodePtr node)
{
    for (; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "signal_length")
        {
            signal_length_size = extract_node_value<std::size_t>(node);
        }
        else if (node_name == "signal_id")
        {
            signal_id_size = extract_node_value<std::size_t>(node);
        }
        else if (node_name == "data_item_length")
        {
            data_item_length_size = extract_node_value<std::size_t>(node);
        }
        else if (node_name == "data_item_id")
        {
            data_item_id_size = extract_node_value<std::size_t>(node);
        }
        else if (node_name == "extension_id")
        {
            extension_id_size = extract_node_value<std::size_t>(node);
        }
        else if (node_name == "status_code")
        {
            status_code_size = extract_node_value<std::size_t>(node);
        }
        else
        {
            std::ostringstream msg;
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    }
}

void
ProtocolConfigImpl::extract_module(xmlNodePtr node)
{
    ModuleInfo modinfo;

    for (; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "name")
        {
            modinfo.name = extract_node_value<std::string>(node);
        }
        else if (node_name == "draft")
        {
            modinfo.draft = extract_node_value<std::string>(node);
        }
        else if (node_name == "experiment_name")
        {
            modinfo.experiment_name = extract_node_value<std::string>(node);
        }
        else if (node_name == "extension_id")
        {
            modinfo.extension_id = extract_node_value<ExtensionIdType>(node);
        }
        else if (node_name == "signal")
        {
            extract_module_signal(node->children, modinfo);
        }
        else if (node_name == "data_item")
        {
            extract_module_data_item(node->children, modinfo);
        }
        else if (node_name == "status_code")
        {
            extract_module_status_code(node->children, modinfo);
        }
        else
        {
            std::ostringstream msg;
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    }

    auto it = module_info_map.find(modinfo.name);
    if (it != module_info_map.end())
    {
        throw BadProtocolConfig("redefinition of module " + modinfo.name);
    }

    module_info_map[modinfo.name] = modinfo;
}

template<typename MapType, typename IdType>
void
ProtocolConfigImpl::insert_id_name_mapping(MapType  & id_name_map, IdType id,
                                           const std::string & name,
                                           const std::string & mapname)
{
    auto itl = id_name_map.left.find(id);
    if ((itl != id_name_map.left.end()) && (itl->second != name))
    {
        throw BadProtocolConfig(
            mapname + " id " + std::to_string(id) + " has multiple names: " +
            itl->second + ", " + name);
    }

    auto itr = id_name_map.right.find(name);
    if ((itr != id_name_map.right.end()) && (itr->second != id))
    {
        throw BadProtocolConfig(
            mapname + " name " + name + " has multiple ids: " +
            std::to_string(id) + ", " + std::to_string(itr->second));
    }

    std::ostringstream msg;
    msg << mapname << " mapping: " << name << " <--> " << id;
    LOG(DLEP_LOG_DEBUG, msg);
    id_name_map.insert({id, name});
}

void
ProtocolConfigImpl::extract_module_signal(xmlNodePtr node,
                                          ModuleInfo & modinfo)
{
    std::ostringstream msg;
    SignalInfo siginfo;

    // Was id specified?  If so, this is the signal's definition, else
    // it is a reference to an existing signal.
    bool have_id = false;

    // Which signal flags were specified?  This records which flags we have a
    // value (0 or 1) for.  The actual flag values will be in siginfo.flags.
    std::uint32_t have_flags = 0;

    // shorthand for both send flags
    const std::uint32_t send_flags = SignalInfo::Flags::modem_sends |
                                     SignalInfo::Flags::router_sends;

    // Is this a message as opposed to a signal?
    bool is_message = false;

    for (; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "name")
        {
            siginfo.name = extract_node_value<std::string>(node);
        }
        else if (node_name == "id")
        {
            have_id = true;
            siginfo.id = extract_node_value<SignalIdType>(node);

        }
        else if (node_name == "message")
        {
            have_flags |= SignalInfo::Flags::message;
            is_message = extract_node_value<bool>(node);
            if (is_message)
            {
                siginfo.flags |= SignalInfo::Flags::message;
            }
            else
            {
                siginfo.flags &= ~SignalInfo::Flags::message;
            }
        }
        else if (node_name == "sender")
        {
            have_flags |= send_flags;
            std::string sender = extract_node_value<std::string>(node);
            if (sender == "modem")
            {
                siginfo.flags |= SignalInfo::Flags::modem_sends;
            }
            else if (sender == "router")
            {
                siginfo.flags |= SignalInfo::Flags::router_sends;
            }
            else // has to be both
            {
                siginfo.flags |= send_flags;
            }
        }
        else if (node_name == "data_item")
        {
            DataItemForSignal difs =
                extract_module_data_item_ref(siginfo.name, node->children);
            siginfo.data_items.push_back(difs);
        }
        else if (node_name == "response")
        {
            std::string response = extract_node_value<std::string>(node);

            try
            {
                siginfo.response_id = get_signal_id(response);
            }
            catch (BadSignalName)
            {
                // this signal hasn't been defined yet
                throw BadProtocolConfig(
                    "undefined response " + response +
                    " for signal " + siginfo.name);
            }
        }
        else
        {
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    } // end for each xml node

    // Now we have extracted all of the inforamtion about this
    // signal/message from the xml tree, and we have to store it.  To
    // begin, we'll assume we are dealing with a signal, and
    // initialize id_name_map/id_info_map accordingly.  If it turns
    // out to be a message, later we'll change these to point to the
    // message maps.

    auto * id_name_map = &signal_map;
    auto * id_info_map = &signal_info_map;

    // If have_id is set, this is the signal's definition.  Check that
    // the message and sender fields were also specified.  I didn't
    // find a way to express the constraint "if this field is present, then
    // these other fields must also be present" in the XML schema, so
    // it's enforced here instead.

    if (have_id)
    {
        if (!(have_flags & SignalInfo::Flags::message))
        {
            throw BadProtocolConfig("definition of signal/message " +
                                    siginfo.name +
                                    " must specify a message element");
        }

        if (!(have_flags & send_flags))
        {
            throw BadProtocolConfig("definition of signal/message " +
                                    siginfo.name +
                                    " must specify a sender element");
        }

        // Enter the id/name mapping into the appropriate table.

        if (is_message)
        {
            id_name_map = &message_map;
            id_info_map = &message_info_map;
        }

        insert_id_name_mapping(*id_name_map, siginfo.id, siginfo.name,
                               (is_message ? "message" : "signal"));
    }
    else
    {
        // This is a reference to what should be an already-defined signal,
        // probably by an extension.  Find the id for the signal being
        // referenced.

        try
        {
            bool is_signal = false;
            siginfo.id = get_signal_id(siginfo.name, &is_signal);
            if (! is_signal)
            {
                is_message = true;
                id_name_map = &message_map;
                id_info_map = &message_info_map;
            }
        }
        catch (BadSignalName)
        {
            // this signal hasn't been defined yet
            throw BadProtocolConfig("undefined signal/message " + siginfo.name);
        }
    }

    // Look up the signal info for this signal id.  If we're just defining the
    // signal now, it won't be found.
    // XXX_20 does this handle redefinition, which should be an error?

    auto it = id_info_map->find(siginfo.id);
    if (it == id_info_map->end())
    {
        // New signal definition
        assert(have_id);
        siginfo.module = modinfo.name;

        // Put the id in the right vector, messages vs. signals

        if (is_message)
        {
            modinfo.messages.push_back(siginfo.id);
        }
        else
        {
            modinfo.signals.push_back(siginfo.id);
        }
        (*id_info_map)[siginfo.id] = siginfo;
    }
    else
    {
        // An extension is modifying this signal.  Incorporate the changes.

        SignalInfo & existing_siginfo = it->second;

        // If flags are being changed, store the new flag values,
        // being careful to not modify any flags that were not
        // specified above.

        if (have_flags)
        {
            std::uint32_t previous_flags = existing_siginfo.flags;
            // clear old flag values
            existing_siginfo.flags &= ~have_flags;
            // store new flag values
            existing_siginfo.flags |= siginfo.flags;

            if (previous_flags != existing_siginfo.flags)
            {
                msg << "module " << modinfo.name
                    << " changed signal " << siginfo.name
                    << " flags from " << previous_flags
                    << " to " << existing_siginfo.flags;
                LOG(DLEP_LOG_INFO, msg);
            }
        }

        // Append any new data items to the existing signal
        // configuration info.

        if (!siginfo.data_items.empty())
        {
            existing_siginfo.data_items.insert(
                existing_siginfo.data_items.end(),
                siginfo.data_items.begin(),
                siginfo.data_items.end());

            msg << "module " << modinfo.name
                << " added " << siginfo.data_items.size()
                << " data items to signal " << siginfo.name;
            LOG(DLEP_LOG_INFO, msg);
        }
    } // end else modifying an existing signal
}

// Extract a reference to a previously defined data item.
// Signals/messages contain these references to specify which data
// items are allowed in the signal.  Similarly, data items that
// contain sub data items contain these references to specify which
// data items are allowed as sub data items.
SubDataItem
ProtocolConfigImpl::extract_module_data_item_ref(const std::string & parent_name,
                                                 _xmlNode * node)
{
    SubDataItem sdi;

    for (; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "name")
        {
            sdi.name = extract_node_value<std::string>(node);

            // Looking up the data item's info here to ensure
            // that the data item has already been defined.

            try
            {
                DataItemInfo di_info = get_data_item_info(sdi.name);
            }
            catch (std::invalid_argument)
            {
                throw BadProtocolConfig("In " + parent_name +
                                        ", undefined reference to data item " +
                                        sdi.name);
            }

            // If this data item has an id defined at the top level,
            // copy it here.  The copied id may be overridden if an id
            // is configured for this data item reference.
            try
            {
                sdi.id = get_data_item_id(sdi.name);
            }
            catch (BadDataItemName)
            {
                // id doesn't have to be defined at the top level
            }
        }
        else if (node_name == "id")
        {
            // The schema doesn't allow an id field for data item
            // references in signals, so we should only come here for
            // sub data items.

            sdi.id = extract_node_value<DataItemIdType>(node);
        }
        else if (node_name == "occurs")
        {
            sdi.occurs = extract_node_value<std::string>(node);
        }
        else
        {
            std::ostringstream msg;
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    }

    // Error if the id wasn't defined anywhere.

    if (sdi.id == IdUndefined)
    {
        throw BadProtocolConfig("In " + parent_name +
            ", id is undefined for data item reference " + sdi.name);
    }

    return sdi;
}

void
ProtocolConfigImpl::extract_module_data_item(xmlNodePtr node,
                                             ModuleInfo & modinfo)
{
    DataItemInfo di_info;
    di_info.module = modinfo.name;

    for (; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "name")
        {
            di_info.name = extract_node_value<std::string>(node);
        }
        else if (node_name == "id")
        {
            di_info.id = extract_node_value<DataItemIdType>(node);

            insert_id_name_mapping(data_item_map, di_info.id,
                                   di_info.name, "data_item");
        }
        else if (node_name == "type")
        {
            std::string type_str = extract_node_value<std::string>(node);
            try
            {
                di_info.value_type = from_string(type_str);
            }
            catch (const std::exception & e)
            {
                throw BadProtocolConfig(
                    " unrecognized data item type " + type_str);
            }
        }
        else if (node_name == "metric")
        {
            bool is_metric = extract_node_value<bool>(node);
            if (is_metric)
            {
                di_info.flags |= DataItemInfo::Flags::metric;
            }
            else
            {
                di_info.flags &= ~DataItemInfo::Flags::metric;
            }
        }
        else if (node_name == "units")
        {
            di_info.units = extract_node_value<std::string>(node);
        }
        else if (node_name == "sub_data_item")
        {
            SubDataItem sdi = 
                extract_module_data_item_ref(di_info.name, node->children);
            di_info.sub_data_items.push_back(sdi);
        }
        else
        {
            std::ostringstream msg;
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    }

    auto it = data_item_info_map.find(di_info.name);
    if (it != data_item_info_map.end())
    {
        throw BadProtocolConfig("redefinition of data item " + di_info.name);
    }

    modinfo.data_items.push_back(di_info.name);
    data_item_info_map[di_info.name] = di_info;
}

void
ProtocolConfigImpl::extract_module_status_code(xmlNodePtr node,
                                               ModuleInfo & modinfo)
{
    StatusCodeInfo sc_info;
    sc_info.module = modinfo.name;

    for (; node; node = node->next)
    {
        std::string node_name((const char *)node->name);
        if (node_name == "name")
        {
            sc_info.name = extract_node_value<std::string>(node);
        }
        else if (node_name == "id")
        {
            sc_info.id = extract_node_value<StatusCodeIdType>(node);

            insert_id_name_mapping(status_code_map, sc_info.id,
                                   sc_info.name, "status_code");
        }
        else if (node_name == "failure_mode")
        {
            sc_info.failure_mode = extract_node_value<std::string>(node);
        }
        else
        {
            std::ostringstream msg;
            msg << "ignoring unrecognized xml node " << node_name;
            LOG(DLEP_LOG_INFO, msg);
        }
    }

    auto it = status_code_info_map.find(sc_info.id);
    if (it != status_code_info_map.end())
    {
        throw BadProtocolConfig("redefinition of status code " + sc_info.name);
    }

    modinfo.status_codes.push_back(sc_info.id);
    status_code_info_map[sc_info.id] = sc_info;
}

//-----------------------------------------------------------------------------
//
// support for retrieving protocol configuration information

std::array<std::uint16_t, 2>
ProtocolConfigImpl::get_version() const
{
    return version;
}


std::size_t
ProtocolConfigImpl::get_signal_length_size() const
{
    return signal_length_size;
}

std::size_t
ProtocolConfigImpl::get_signal_id_size() const
{
    return signal_id_size;
}

std::size_t
ProtocolConfigImpl::get_data_item_length_size() const
{
    return data_item_length_size;
}

std::size_t
ProtocolConfigImpl::get_data_item_id_size() const
{
    return data_item_id_size;
}

std::size_t
ProtocolConfigImpl::get_extension_id_size() const
{
    return extension_id_size;
}

std::size_t
ProtocolConfigImpl::get_status_code_size() const
{
    return status_code_size;
}

SignalIdType
ProtocolConfigImpl::get_signal_id(const std::string & name,
                                  bool * is_signal_return) const
{
    // First look up the name as a signal name.  We assume that there
    // won't be a signal and a message with exactly the same name.

    auto sig_iter = signal_map.right.find(name);
    if (sig_iter != signal_map.right.end())
    {
        if (is_signal_return)
        {
            *is_signal_return = true;
        }
        return sig_iter->second;
    }

    // It wasn't found as a signal name, so look it up as a message
    // name.

    auto msg_iter = message_map.right.find(name);
    if (msg_iter != message_map.right.end())
    {
        if (is_signal_return)
        {
            *is_signal_return = false;
        }
        return msg_iter->second;
    }

    // name was neither a signal name nor a message name.

    throw BadSignalName(name);
}

std::string
ProtocolConfigImpl::get_signal_name(SignalIdType id) const
{
    auto iter = signal_map.left.find(id);
    if (iter == signal_map.left.end())
    {
        throw BadSignalId(std::to_string(id));
    }

    return iter->second;
}

std::string
ProtocolConfigImpl::get_message_name(SignalIdType id) const
{
    auto iter = message_map.left.find(id);
    if (iter == message_map.left.end())
    {
        throw BadSignalId(std::to_string(id));
    }

    return iter->second;
}

std::string
ProtocolConfigImpl::get_message_response_name(const std::string & msg_name) const
{
    SignalInfo siginfo = get_signal_info(msg_name);
    if (siginfo.response_id == 0)
    {
        return "";
    }
    else
    {
        if (siginfo.flags & SignalInfo::Flags::message)
        {
            return get_message_name(siginfo.response_id);
        }
        else
        {
            return get_signal_name(siginfo.response_id);
        }
    }
}

DataItemIdType
ProtocolConfigImpl::get_data_item_id(const std::string & name,
                                     const DataItemInfo * parent_di_info) const
{
    if (parent_di_info)
    {
        for (const SubDataItem & sdi : parent_di_info->sub_data_items)
        {
            // if sdi.id is IdUndefined, it means it wasn't set in the protocol config
            if ( (sdi.name == name) && (sdi.id != IdUndefined) )
            {
                return sdi.id;
            }
        }
    }

    // Execution comes here if parent_di_info was null (the data item
    // name is not a sub data item), OR if there was a parent_di_info
    // but the name was not found in its list of sub data items, OR if
    // there was a parent_di_info, name was found in its list, but the
    // associated id was IdUndefined (effectively, not set).  In any
    // case, we now look up the name in the top-level scope.  This
    // allows a core data item to be used as a sub data item without
    // having to repeat its id in the protocol configuration of each
    // parent data items it can appear in.

    auto iter = data_item_map.right.find(name);
    if (iter == data_item_map.right.end())
    {
        throw BadDataItemName(name);
    }

    return iter->second;
}

std::string
ProtocolConfigImpl::get_data_item_name(DataItemIdType id,
                                       const DataItemInfo * parent_di_info) const
{
    if (parent_di_info)
    {
        for (const SubDataItem & sdi : parent_di_info->sub_data_items)
        {
            if (sdi.id == id)
            {
                return sdi.name;
            }
        }
    }

    // Execution comes here if parent_di_info was null (id is not a
    // sub data item) OR if there was a parent_di_info but the id was
    // not found in its list of sub data items.  Either way, we now
    // look up the id in the top-level scope.  This allows a core data
    // item to be used as a sub data item without having to repeat its
    // id in the protocol configuration of each parent data items it
    // can appear in.

    auto iter = data_item_map.left.find(id);
    if (iter == data_item_map.left.end())
    {
        throw BadDataItemId(std::to_string(id));
    }

    return iter->second;
}

StatusCodeIdType
ProtocolConfigImpl::get_status_code_id(const std::string & name) const
{
    auto iter = status_code_map.right.find(name);
    if (iter == status_code_map.right.end())
    {
        throw BadStatusCodeName(name);
    }

    return iter->second;
}

std::string
ProtocolConfigImpl::get_status_code_name(StatusCodeIdType id) const
{
    auto iter = status_code_map.left.find(id);
    if (iter == status_code_map.left.end())
    {
        throw BadStatusCodeId(std::to_string(id));
    }

    return iter->second;
}

std::string
ProtocolConfigImpl::get_signal_prefix() const
{
    return signal_prefix;
}

DataItemValueType
ProtocolConfigImpl::get_data_item_value_type(const std::string & name) const
{
    auto it = data_item_info_map.find(name);
    if (it == data_item_info_map.end())
    {
        throw BadDataItemName(name);
    }

    return it->second.value_type;
}

std::vector<DataItemInfo>
ProtocolConfigImpl::get_data_item_info() const
{
    std::vector<std::string> vs; //empty vector
    return get_data_item_info(vs);
}

DataItemInfo
ProtocolConfigImpl::get_data_item_info(const std::string & di_name) const
{
    std::vector<std::string> vdi {di_name};
    std::vector<DataItemInfo> vdiinfo = get_data_item_info(vdi);
    if (vdiinfo.empty())
    {
        throw BadDataItemName(di_name);
    }
    return vdiinfo[0];
}

std::vector<DataItemInfo>
ProtocolConfigImpl::get_data_item_info(const std::vector<std::string> &
                                       di_names) const
{
    std::vector<DataItemInfo> vmi;

    for (const auto & kvpair : data_item_info_map)
    {
        const DataItemInfo & di_info = kvpair.second;
        if (di_names.empty())
        {
            vmi.push_back(di_info);
        }
        else
        {
            for (const std::string & di_name : di_names)
            {
                if (di_name == di_info.name)
                {
                    vmi.push_back(di_info);
                    break;
                }
            }
        }
    }

    return vmi;
}

DataItemInfo
ProtocolConfigImpl::get_data_item_info(DataItemIdType id,
                                       const DataItemInfo * parent_di_info) const
{
    return get_data_item_info(get_data_item_name(id, parent_di_info));
}

std::vector<ProtocolConfig::SignalInfo>
ProtocolConfigImpl::get_signal_info() const
{
    std::vector<std::string> vs; //empty vector
    return get_signal_info(vs);
}

ProtocolConfig::SignalInfo
ProtocolConfigImpl::get_signal_info(const std::string & sig_name) const
{
    std::vector<std::string> vs {sig_name};
    std::vector<SignalInfo> vsiginfo = get_signal_info(vs);
    if (vsiginfo.empty())
    {
        throw BadSignalName(sig_name);
    }
    return vsiginfo[0];
}

std::vector<ProtocolConfig::SignalInfo>
ProtocolConfigImpl::get_signal_info(const std::vector<std::string> & sig_names)
const
{
    std::vector<ProtocolConfig::SignalInfo> vsi;

    // Find any signals that match sig_names

    for (const auto & kvpair : signal_info_map)
    {
        const SignalInfo & sig_info = kvpair.second;
        if (sig_names.empty())
        {
            vsi.push_back(sig_info);
        }
        else
        {
            for (const std::string & sig_name : sig_names)
            {
                if (sig_name == sig_info.name)
                {
                    vsi.push_back(sig_info);
                    break;
                }
            }
        }
    }

    // Find any messages that match sig_names

    for (const auto & kvpair : message_info_map)
    {
        const SignalInfo & sig_info = kvpair.second;
        if (sig_names.empty())
        {
            vsi.push_back(sig_info);
        }
        else
        {
            for (const std::string & sig_name : sig_names)
            {
                if (sig_name == sig_info.name)
                {
                    vsi.push_back(sig_info);
                    break;
                }
            }
        }
    }

    return vsi;
}

std::vector<ProtocolConfig::StatusCodeInfo>
ProtocolConfigImpl::get_status_code_info() const
{
    std::vector<std::string> vs; //empty vector
    return get_status_code_info(vs);
}

ProtocolConfig::StatusCodeInfo
ProtocolConfigImpl::get_status_code_info(const std::string & sc_name) const
{
    std::vector<std::string> vs {sc_name};
    std::vector<StatusCodeInfo> vscinfo = get_status_code_info(vs);
    if (vscinfo.empty())
    {
        throw BadSignalName(sc_name);
    }
    return vscinfo[0];
}

std::vector<ProtocolConfig::StatusCodeInfo>
ProtocolConfigImpl::get_status_code_info(const std::vector<std::string> &
                                         sc_names) const
{
    std::vector<ProtocolConfig::StatusCodeInfo> vsi;

    for (const auto & kvpair : status_code_info_map)
    {
        const StatusCodeInfo & sc_info = kvpair.second;
        if (sc_names.empty())
        {
            vsi.push_back(sc_info);
        }
        else
        {
            for (const std::string & sc_name : sc_names)
            {
                if (sc_name == sc_info.name)
                {
                    vsi.push_back(sc_info);
                    break;
                }
            }
        }
    }

    return vsi;
}

std::vector<ProtocolConfig::ModuleInfo>
ProtocolConfigImpl::get_module_info() const
{
    std::vector<std::string> vm; //empty vector
    return get_module_info(vm);
}

ProtocolConfig::ModuleInfo
ProtocolConfigImpl::get_module_info(const std::string & module_name) const
{
    std::vector<std::string> vm {module_name};
    std::vector<ModuleInfo> vmodinfo = get_module_info(vm);
    if (vmodinfo.empty())
    {
        throw BadModuleName(module_name);
    }
    return vmodinfo[0];
}

std::vector<ProtocolConfig::ModuleInfo>
ProtocolConfigImpl::get_module_info(
    const std::vector<std::string> & module_names) const
{
    std::vector<ProtocolConfig::ModuleInfo> vmi;

    for (const auto & kvpair : module_info_map)
    {
        const ModuleInfo & modinfo = kvpair.second;
        if (module_names.empty())
        {
            vmi.push_back(modinfo);
        }
        else
        {
            for (const std::string & module_name : module_names)
            {
                if (module_name == modinfo.name)
                {
                    vmi.push_back(modinfo);
                    break;
                }
            }
        }
    }

    return vmi;
}

std::vector<ExtensionIdType>
ProtocolConfigImpl::get_extension_ids() const
{
    std::vector<ExtensionIdType> v_extid;

    for (const auto & kvpair : module_info_map)
    {
        const ModuleInfo & modinfo = kvpair.second;
        if (modinfo.extension_id != 0)
        {
            v_extid.push_back(modinfo.extension_id);
        }
    }
    return v_extid;
}

std::vector<std::string>
ProtocolConfigImpl::get_experiment_names() const
{
    std::vector<std::string> v_expname;

    for (const auto & kvpair : module_info_map)
    {
        const ModuleInfo & modinfo = kvpair.second;
        if (modinfo.experiment_name.length() != 0)
        {
            v_expname.push_back(modinfo.experiment_name);
        }
    }
    return v_expname;
}

bool
ProtocolConfigImpl::is_metric(DataItemIdType id,
                              const DataItemInfo * parent_di_info) const
{
    // An undefined id isn't a metric.  This can happen with sub data items.
    if (id == IdUndefined)
    {
        return false;
    }

    DataItemInfo di_info = get_data_item_info(id, parent_di_info);
    return static_cast<bool>(di_info.flags & DataItemInfo::Flags::metric);
}

bool
ProtocolConfigImpl::is_ipaddr(DataItemIdType id,
                              const DataItemInfo * parent_di_info) const
{
    // An undefined id isn't an ip address.  This can happen with sub data items.
    if (id == IdUndefined)
    {
        return false;
    }

    DataItemInfo di_info = get_data_item_info(id, parent_di_info);
    DataItemValueType divt = di_info.value_type;

    return ((divt == DataItemValueType::div_u8_ipv4)     ||
            (divt == DataItemValueType::div_u8_ipv6)     ||
            (divt == DataItemValueType::div_ipv4_u8)     ||
            (divt == DataItemValueType::div_ipv6_u8)     ||
            (divt == DataItemValueType::div_u8_ipv4_u16) ||
            (divt == DataItemValueType::div_u8_ipv6_u16) ||
            (divt == DataItemValueType::div_u8_ipv4_u8)  ||
            (divt == DataItemValueType::div_u8_ipv6_u8)
        );
}
