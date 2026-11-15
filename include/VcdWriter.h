/**
 * @file VcdWriter.h
 * @author Sebastian Paulus
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
 * Usage notes:
 *  - Only nets that exist when recording starts are recorded.
 *  - VCD time only moves forward: after Simulator::reset(), attach a new
 *    writer.
 *  - A writer must be destroyed (or detach()ed) before the Simulator it is
 *    attached to.
 *
 * @code
 *   std::ofstream file("wave.vcd");
 *   VcdWriter vcd(file);
 *   vcd.attach(sim);          // records every net from now on
 *   sim.run(1000);
 * @endcode
 */
class VcdWriter
{
public:
    /**
     * @brief Creates a writer; nothing is written until a header is needed.
     *
     * @param out       Destination stream; must outlive the writer.
     * @param timescale What one tick means, e.g. "1ns" or "10ps".
     * @param top_scope Name of the outermost scope.
     */
    explicit VcdWriter(std::ostream& out,
                       std::string timescale = "1ns",
                       std::string top_scope = "top");

    /// Detaches from the Simulator, if attached.
    ~VcdWriter();

    /// Not copyable: it writes to one stream and listens to one Simulator.
    VcdWriter(const VcdWriter&) = delete;

    /// Not copyable: it writes to one stream and listens to one Simulator.
    VcdWriter& operator=(const VcdWriter&) = delete;

    /**
     * @brief Records every net of @p sim: writes the header and current values
     *        at sim.now(), then follows changes through a change listener.
     *
     * @param sim The simulator to record.
     * @throws std::logic_error if already attached.
     */
    void attach(Simulator& sim);

    /**
     * @brief Records only some nets of @p sim.
     *
     * @param sim  The simulator to record.
     * @param nets The nets to record; nullptr entries are skipped.
     * @throws std::logic_error if already attached.
     */
    void attach(Simulator& sim, const std::vector<const Net*>& nets);

    /// Stops following the Simulator and flushes the stream.
    void detach();

    // --- Manual use, without a Simulator ---

    /**
     * @brief Adds a net to record. Only before writeHeader().
     * @param net The net.
     * @throws std::logic_error after the header has been written.
     */
    void addNet(const Net& net);

    /**
     * @brief Writes definitions and the initial values of every added net.
     * @param time Timestamp of the initial values.
     * @throws std::logic_error if the header was already written.
     */
    void writeHeader(uint64_t time);

    /**
     * @brief Records @p net taking @p value at @p time.
     *
     * Ignored for nets that were not added, for repeats of the last value
     * written, and for times earlier than the last one written.
     *
     * @param net   The net that changed.
     * @param value Its new value.
     * @param time  When it changed.
     */
    void change(const Net& net, LogicValue value, uint64_t time);

private:
    /// One recorded signal.
    struct Var
    {
        const Net* net;  ///< The net recorded.
        /// Guards against a removed net's address being reused.
        uint32_t net_id;
        std::string code; ///< VCD identifier code.
        char last = '?';  ///< Last value written, to skip repeats.
    };

    /**
     * @brief VCD identifier code for the @p index-th signal.
     * @param index Position in vars_.
     * @return A short code of printable characters ('!' to '~').
     */
    static std::string codeFor_(std::size_t index);

    /**
     * @brief VCD character for a value.
     * @param v The value.
     * @return '0', '1', 'x' or 'z'.
     */
    static char vcdChar_(LogicValue v);

    /// @brief Writes a "#time" line unless the last one written was the same.
    /// @param time The timestamp.
    void writeTime_(uint64_t time);

    std::ostream& out_;     ///< Destination stream.
    std::string timescale_; ///< What one tick means.
    std::string top_;       ///< Outermost scope name.
    std::vector<Var> vars_; ///< Recorded signals, in header order.
    std::unordered_map<const Net*, std::size_t> index_; ///< Net to vars_ index.
    bool header_written_ = false; ///< writeHeader() has run.
    bool time_written_ = false;   ///< At least one "#time" line written.
    uint64_t last_time_ = 0;      ///< Last timestamp written.

    Simulator* sim_ = nullptr;           ///< Simulator followed, if attached.
    Simulator::ListenerId listener_ = 0; ///< Its change-listener id.
};

#endif
