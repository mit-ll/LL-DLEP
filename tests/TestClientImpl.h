/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// Test DLEP client implementation class declaration.

#ifndef TEST_CLIENT_IMPL_H
#define TEST_CLIENT_IMPL_H

#include "DlepClient.h" // base class
#include "Thread.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <libxml/parser.h> // for xmlDocPtr, xmlNodePtr

/// Test instantiation of the DlepClient interface.
class TestClientImpl : public LLDLEP::DlepClient
{
public:
    TestClientImpl();

    /// @see DlepClient for documentation of inherited methods

    void get_config_parameter(const std::string & parameter_name,
                              ConfigValue * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              bool * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              unsigned int * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              std::string * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              boost::asio::ip::address * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              std::vector<unsigned int> * value) override;

    void peer_up(const LLDLEP::PeerInfo & peer_info) override;

    void peer_update(const std::string & peer_id,
                     const LLDLEP::DataItems & data_items) override;

    void peer_down(const std::string & peer_id) override;

    std::string destination_up(const std::string & peer_id,
                               const LLDLEP::DlepMac & mac_address,
                               const LLDLEP::DataItems & data_items) override;

    void destination_update(const std::string & peer_id,
                            const LLDLEP::DlepMac & mac_address,
                            const LLDLEP::DataItems & data_items) override;

    void destination_down(const std::string & peer_id,
                          const LLDLEP::DlepMac & mac_address) override;

    void linkchar_request(const std::string & peer_id,
                          const LLDLEP::DlepMac & mac_address,
                          const LLDLEP::DataItems & data_items) override;

    void linkchar_reply(const std::string & peer_id,
                        const LLDLEP::DlepMac & mac_address,
                        const LLDLEP::DataItems & data_items) override;

    /// Parse one configuration parameter and put it into the configuration
    /// database.
    /// @param[in] param_name  string name of the parameter
    /// @param[in] param_value string value of the parameter
    /// @return true if success, else false.
    bool parse_parameter(const char * param_name, const char * param_value);

    /// Print the contents of the configuration database.
    void print_config() const;

    /// Load configuration parameters from a file.
    /// @param[in] config_filename name of the configuration file to read
    /// @return true if success, else false.
    bool parse_config_file(const char * config_filename);

    /// Set the session-port configuration parameter.
    void set_session_port(uint16_t session_port);

    /// Set the discovery-iface configuration parameter to a suitable
    /// value.
    /// @param[in] want_ipv4_addr
    ///            If true, find an interface with an IPv4 address, else
    ///            find one with an IPv6 address.  In the IPv6 case, only
    ///            interfaces with link-local IPv6 addresses will be used.
    ///            This usually (always?) excludes the loopback interface.
    void set_discovery_iface(bool want_ipv4_addr);

    /// Set a configuration parameter to the name of the loopback interface.
    /// @param[in] param_name
    ///            The parameter name to set.
    void set_loopback_iface(std::string & param_name);

    /// This class eases the job of waiting for a call by the Dlep library
    /// from a different thread to a DlepClient method.
    template<typename T>
    class ClientCallWaiter
    {
    public:
        ClientCallWaiter() : ready(false) {}

        // A test case should call this before there is any chance that the
        // DlepClient method has been called.
        void prepare_to_wait()
        {
            boost::lock_guard<boost::mutex> lock(mtx);
            ready = false;
        }

        /// Wait for a service call from a different thread.
        /// @return true if the expected call happened within a reasonable
        /// time, else false.
        bool wait_for_client_call()
        {
            boost::system_time timeout =
                boost::get_system_time() + boost::posix_time::minutes(1);
            boost::unique_lock<boost::mutex> lock(mtx);

            // Sometimes a condition variable wakes up even though its
            // condition hasn't actually happened.  Search the web for
            // "condition variable spurious wakeup".  To deal with
            // that possibility, we have to wrap the wait in a loop
            // that checks to see if the condition *really* occurred.
            while (! ready)
            {
                // We have to work with older versions of Boost (1.48) that don't
                // have wait_for(), so use timed_wait().
                bool ok = cv.timed_wait(lock, timeout);
                if (! ok)
                {
                    // timed out
                    return false;
                }
            }
            return true;
        }

        /// Tell the thread that should be waiting in wait_for_client_call()
        /// that the expected call has occured.  notify() is called from the
        /// library's thread.
        /// @param[in] x information that was provided to the DlepClient
        ///              method that will be recorded for possible later use
        ///              by check_result()
        void notify(const T & x)
        {
            boost::lock_guard<boost::mutex> lock(mtx);
            ready = true;
            result = x;
            cv.notify_one();
        }

        /// Compare the information recorded in notify() with the expected result.
        /// @return true if they are equal, else false
        bool check_result(const T & expected_result)
        {
            return (result == expected_result);
        }

    private:
        /// inter-thread synchronization objects
        boost::condition_variable cv;
        boost::mutex mtx;
        bool ready;

        /// Information that was passed to the DlepClient method and
        /// recorded by notify() for later inspection.
        T result;
    }; // class ClientCallWaiter

    // Waiters for various DlepClient methods

    ClientCallWaiter<LLDLEP::PeerInfo> peer_up_waiter;
    ClientCallWaiter<std::string> peer_down_waiter;
    ClientCallWaiter<std::string> peer_update_waiter;
    ClientCallWaiter<LLDLEP::DlepMac> destination_up_waiter;
    ClientCallWaiter<LLDLEP::DlepMac> destination_update_waiter;
    ClientCallWaiter<LLDLEP::DlepMac> destination_down_waiter;

private:
    /// Configuration database.
    /// Maps parameter names to their values.
    std::map<std::string, DlepClient::ConfigValue> config_map;

    /// Metadata information about one configuration parameter.
    struct ConfigParameterInfo
    {
        /// Types that config parameters can have
        enum class ParameterType
        {
            UnsignedInteger,
            String,
            IPAddress,
            Boolean,
            ConfigFile, // special case of string
            ListOfUnsignedInteger
        };

        ParameterType type;
    };

    /// Map of parameter names to their metadata.
    typedef std::map<std::string, ConfigParameterInfo> ConfigParameterMapType;

    /// Actual config parameter definitions.
    static ConfigParameterMapType param_info;
};

#endif // DLEP_CLIENT_IMPL_H
