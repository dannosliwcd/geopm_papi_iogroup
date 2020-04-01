/*
 * Copyright (c) 2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <papi.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <sys/types.h>
#include <stdexcept>
#include <cmath>

#include "geopm/IOGroup.hpp"
#include "geopm/PlatformTopo.hpp"
#include "geopm/Exception.hpp"
#include "geopm/Agg.hpp"
#include "geopm/Helper.hpp"

using geopm::Exception;
using geopm::IOGroup;

static void die(std::string file, int line, std::string call, int retval)
{
    std::ostringstream oss;
    oss << "PapiIOGroup:" << line << ": ";
    if (retval == PAPI_ESYS) {
        oss << "System error in " << call << ": " << strerror(errno);
    }
    else if (retval > 0) {
        oss << "Error calculating: " << call;
    }
    else {
        oss << "Error in " << call << "(" << retval << "): " << PAPI_strerror(retval);
    }
    throw std::runtime_error(oss.str());
}

class PapiIOGroup : public IOGroup
{
    public:
    PapiIOGroup();
    PapiIOGroup(const std::string &pm_counters_path);
    virtual ~PapiIOGroup() = default;
    std::set<std::string> signal_names(void) const override;
    std::set<std::string> control_names(void) const override;
    bool is_valid_signal(const std::string &signal_name) const override;
    bool is_valid_control(const std::string &control_name) const override;
    int signal_domain_type(const std::string &signal_name) const override;
    int control_domain_type(const std::string &control_name) const override;
    int push_signal(const std::string &signal_name, int domain_type,
                    int domain_idx) override;
    int push_control(const std::string &control_name, int domain_type,
                     int domain_idx) override;
    void read_batch(void) override;
    void write_batch(void) override;
    double sample(int batch_idx) override;
    void adjust(int batch_idx, double setting) override;
    double read_signal(const std::string &signal_name, int domain_type,
                       int domain_idx) override;
    void write_control(const std::string &control_name, int domain_type,
                       int domain_idx, double setting) override;
    void save_control(void) override;
    void restore_control(void) override;
    std::function<double(const std::vector<double> &)>
        agg_function(const std::string &signal_name) const override;
    std::function<std::string(double)>
        format_function(const std::string &signal_name) const override;
    std::string signal_description(const std::string &signal_name) const override;
    std::string control_description(const std::string &control_name) const override;
    static std::string plugin_name(void);
    static std::unique_ptr<IOGroup> make_plugin(void);

    private:
    using papi_signal_offset = size_t;
    struct signal_s {
        const std::string m_description;
        bool m_do_read;
    };

    std::map<std::string, papi_signal_offset> m_signal_offsets;
    std::vector<std::vector<signal_s>> m_signals_per_core;
    std::vector<std::vector<long long>> m_papi_values_per_core;
    std::vector<double> m_batch_values;
    std::vector<int> m_papi_event_sets; // One per core
};

PapiIOGroup::PapiIOGroup()
    : m_signal_offsets(), m_signals_per_core(), m_papi_values_per_core(), m_batch_values(), m_papi_event_sets()
{

    std::vector<std::string> event_names;
    const char *env_str = std::getenv("GEOPM_PAPI_EVENTS");
    if (env_str) {
        std::stringstream ss(env_str);
        std::istream_iterator<std::string> begin(ss);
        std::istream_iterator<std::string> end;
        event_names = std::vector<std::string>(begin, end);
    }

    int retval;

    // Initialize PAPI and our multiplexed events set
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) die(__FILE__, __LINE__,  "PAPI_library_init", retval);

    retval = PAPI_multiplex_init();
    if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_multiplex_init", retval);

    retval = PAPI_set_granularity(PAPI_GRN_SYS);
    if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_set_granularity(PAPI_GRN_SYS)", retval);

    const auto* papi_hardware = PAPI_get_hardware_info();
    int num_cores = papi_hardware->sockets * papi_hardware->cores;
    m_papi_event_sets = std::vector<int>(num_cores, PAPI_NULL);

    // TODO: Can we get pids of our other processes and use PAPI_attach? Would
    // that avoid the permissions we need for system-wide monitoring?
    for (int i = 0; i < num_cores; ++i) {
        retval = PAPI_create_eventset(&m_papi_event_sets[i]);
        if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_create_eventset", retval);

        retval = PAPI_assign_eventset_component(m_papi_event_sets[i], 0);
        if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_assign_eventset_component", retval);

        PAPI_option_t opt;
        opt.cpu.cpu_num = i;
        opt.cpu.eventset = m_papi_event_sets[i];
        retval = PAPI_set_opt(PAPI_CPU_ATTACH, &opt);
        if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_set_opt(PAPI_CPU_ATTACH)", retval);

        retval = PAPI_set_multiplex(m_papi_event_sets[i]);
        if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_set_multiplex", retval);
        m_signals_per_core.emplace_back(std::vector<signal_s>{});

        // Add events to the PAPI event set
        for (const auto& event_name : event_names)
        {
            int event_code = PAPI_NULL;
            retval = PAPI_event_name_to_code(event_name.c_str(), &event_code);

            std::ostringstream oss;
            oss << "PAPI_event_name_to_code(\"" << event_name << "\")";
            if (retval != PAPI_OK) die(__FILE__, __LINE__, oss.str(), retval);

            retval = PAPI_add_event(m_papi_event_sets[i], event_code);

            oss.clear();
            oss << "PAPI_add_event(\"" << event_name << "\")";
            if (retval != PAPI_OK) die(__FILE__, __LINE__,  oss.str(), retval);

            m_signals_per_core.back().emplace_back(signal_s{std::string("PAPI Counter: ") + event_name, false});
        }

        m_papi_values_per_core.emplace_back(std::vector<long long>(event_names.size(), 0));

        retval = PAPI_start(m_papi_event_sets[i]);
        if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_start CPU " + std::to_string(i), retval);
    }

    m_batch_values = std::vector<double>(num_cores * event_names.size(), 0);

    // Add events to the PAPI event set
    for (size_t i = 0; i < event_names.size(); ++i)
    {
        const auto& event_name = event_names[i];
        m_signal_offsets.emplace(event_name, i);
    }

}

std::set<std::string> PapiIOGroup::signal_names(void) const
{
    std::set<std::string> names;
    for (const auto &signal_offset_kv : m_signal_offsets) {
        names.insert(signal_offset_kv.first);
    }
    return names;
}

std::set<std::string> PapiIOGroup::control_names(void) const
{
    return {};
}

bool PapiIOGroup::is_valid_signal(const std::string &signal_name) const
{
    return m_signal_offsets.find(signal_name) != m_signal_offsets.end();
}

bool PapiIOGroup::is_valid_control(const std::string &control_name) const
{
    return false;
}

int PapiIOGroup::signal_domain_type(const std::string &signal_name) const
{
    return is_valid_signal(signal_name) ? GEOPM_DOMAIN_CORE : GEOPM_DOMAIN_INVALID;
}

int PapiIOGroup::control_domain_type(const std::string &control_name) const
{
    return GEOPM_DOMAIN_INVALID;
}

int PapiIOGroup::push_signal(const std::string &signal_name, int domain_type, int domain_idx)
{
    auto offset_it = m_signal_offsets.find(signal_name);
    if (offset_it == m_signal_offsets.end()) {
        throw Exception("PapiIOGroup::push_signal(): " + signal_name +
                            "not valid for PapiIOGroup",
                        GEOPM_ERROR_INVALID, __FILE__, __LINE__);
    }
    else if (domain_type != GEOPM_DOMAIN_CORE) {
        throw Exception("PapiIOGroup::push_signal(): domain_type " +
                            std::to_string(domain_type) +
                            "not valid for PapiIOGroup",
                        GEOPM_ERROR_INVALID, __FILE__, __LINE__);
    }
    for (size_t core = 0; core < m_papi_event_sets.size(); ++core) {
        for (size_t i = 0; i < m_signal_offsets.size(); ++i) {
            m_signals_per_core[domain_idx][offset_it->second].m_do_read = true;
        }
    }
    return domain_idx*m_signal_offsets.size() + offset_it->second;
}

int PapiIOGroup::push_control(const std::string &control_name, int domain_type,
                              int domain_idx)
{
    throw Exception("PapiIOGroup has no controls", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
}

void PapiIOGroup::read_batch(void)
{
    for (size_t core = 0; core < m_papi_event_sets.size(); ++core) {
        // TODO: This currently just gets everything. It should filter based on the counters
        // enabled for this batch.
        int retval = PAPI_read(m_papi_event_sets[core], m_papi_values_per_core[core].data());
        if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_read", retval);

        for (size_t i = 0; i < m_signal_offsets.size(); ++i) {
            m_batch_values[core * m_signal_offsets.size() + i] = static_cast<double>(m_papi_values_per_core[core][i]);
        }
    }
}

void PapiIOGroup::write_batch(void) {}

double PapiIOGroup::sample(int batch_idx)
{
    double result = NAN;
    if (batch_idx < 0 || batch_idx >= static_cast<int>(m_batch_values.size())) {
        throw Exception("PapiIOGroup::sample(): batch_idx " + std::to_string(batch_idx) +
                            " not valid for PapiIOGroup",
                        GEOPM_ERROR_INVALID, __FILE__, __LINE__);
    }
    else {
        result = m_batch_values[batch_idx];
    }
    return result;
}



// Save a setting to be written by a future write_batch()
void PapiIOGroup::adjust(int batch_idx, double setting)
{
    throw Exception("PapiIOGroup has no controls", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
}

double PapiIOGroup::read_signal(const std::string &signal_name, int domain_type,
                                int domain_idx)
{
    auto offset_it = m_signal_offsets.find(signal_name);
    if (offset_it == m_signal_offsets.end()) {
        throw Exception("PapiIOGroup::read_signal(): " + signal_name +
                            "not valid for PapiIOGroup",
                        GEOPM_ERROR_INVALID, __FILE__, __LINE__);
    }
    else if (domain_type != GEOPM_DOMAIN_CORE) {
        throw Exception("PapiIOGroup:read_signal(): domain_type " +
                            std::to_string(domain_type) +
                            "not valid for PapiIOGroup",
                        GEOPM_ERROR_INVALID, __FILE__, __LINE__);
    }

    int retval = PAPI_read(m_papi_event_sets[domain_idx], m_papi_values_per_core[domain_idx].data());
    if (retval != PAPI_OK) die(__FILE__, __LINE__,  "PAPI_read", retval);

    return static_cast<double>(m_papi_values_per_core[domain_idx][offset_it->second]);
}

void PapiIOGroup::write_control(const std::string &control_name, int domain_type, int domain_idx, double setting)
{
        throw Exception("PapiIOGroup has no controls", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
}

void PapiIOGroup::save_control(void)
{
}

void PapiIOGroup::restore_control(void)
{
}

std::function<double(const std::vector<double> &)> PapiIOGroup::agg_function(const std::string &signal_name) const
{
    // All signals will be aggregated as a sum
    return geopm::Agg::sum;
}

std::function<std::string(double)> PapiIOGroup::format_function(const std::string &signal_name) const
{
    return geopm::string_format_integer;
}

std::string PapiIOGroup::signal_description(const std::string &signal_name) const
{
    return "Dummy description. See papi_avail and papi_native_avail";
}

std::string PapiIOGroup::control_description(const std::string &control_name) const
{
    throw Exception("PapiIOGroup has no controls", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
}

std::string PapiIOGroup::plugin_name(void)
{
    return "PAPI";
}

std::unique_ptr<geopm::IOGroup> PapiIOGroup::make_plugin(void)
{
    return std::unique_ptr<geopm::IOGroup>(new PapiIOGroup);
}

static void __attribute__((constructor)) papi_iogroup_load(void)
{
    try {
        geopm::iogroup_factory().register_plugin(PapiIOGroup::plugin_name(),
                                                 PapiIOGroup::make_plugin);
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Error: unknown cause" << std::endl;
    }
}
