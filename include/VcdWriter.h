/**
 * @file VcdWriter.h
 * @date 2026/09/24
 * @brief Value Change Dump (IEEE 1364 VCD) output, viewable in GTKWave etc.
 */

#ifndef VCDWRITER_H
#define VCDWRITER_H

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "Net.h"
#include "Simulator.h"
#include "Types.h"

/**
 * @class VcdWriter
 * @brief Records net values as a VCD waveform.
 *
 * Dotted net names become nested scopes, so an instance "fa0.ha1.sum" shows
 * up as signal "sum" inside fa0 / ha1 in a waveform viewer.
 *
 * @code
 *   std::ofstream file("wave.vcd");
 *   VcdWriter vcd(file);
 *   vcd.attach(sim);          // records every net from now on
 *   sim.run(1000);
 * @endcode
 *
 * Only nets that exist when recording starts are recorded. VCD time only
 * moves forward: after Simulator::reset(), attach a new writer. A writer
 * must be destroyed (or detach()ed) before the Simulator it is attached to.
 */
class VcdWriter
{
public:
    /**
     * @param out       Destination stream; must outlive the writer.
     * @param timescale What one tick means, e.g. "1ns" or "10ps".
     * @param top_scope Name of the outermost scope.
     */
    explicit VcdWriter(std::ostream& out,
                       std::string timescale = "1ns",
                       std::string top_scope = "top");

    /// Detaches from the Simulator, if attached.
    ~VcdWriter();

    VcdWriter(const VcdWriter&) = delete;
    VcdWriter& operator=(const VcdWriter&) = delete;

    /// Records every net of @p sim: writes the header and current values at
    /// sim.now(), then follows changes through a change listener.
    void attach(Simulator& sim);

    /// Records only @p nets of @p sim.
    void attach(Simulator& sim, const std::vector<const Net*>& nets);

    /// Stops following the Simulator and flushes the stream.
    void detach();

    // --- Manual use, without a Simulator ---

    /// Adds a net to record. Only before writeHeader().
    /// @throws std::logic_error after the header has been written.
    void addNet(const Net& net);

    /// Writes definitions and the initial values of every added net.
    void writeHeader(uint64_t time);

    /// Records @p net taking @p value at @p time. Ignored for nets that were
    /// not added, or times earlier than the last one written.
    void change(const Net& net, LogicValue value, uint64_t time);

private:
    struct Var
    {
        const Net* net;
        uint32_t net_id;   ///< Guards against a removed net's address being reused.
        std::string code;  ///< VCD identifier code.
        char last = '?';   ///< Last value written, to skip repeats.
    };

    static std::string codeFor_(std::size_t index);
    static char vcdChar_(LogicValue v);
    void writeTime_(uint64_t time);

    std::ostream& out_;
    std::string timescale_;
    std::string top_;
    std::vector<Var> vars_;
    std::unordered_map<const Net*, std::size_t> index_;
    bool header_written_ = false;
    bool time_written_ = false;
    uint64_t last_time_ = 0;

    Simulator* sim_ = nullptr;
    Simulator::ListenerId listener_ = 0;
};

#endif
