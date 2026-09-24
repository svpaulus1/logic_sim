/**
 * @file Module.h
 * @date 2026/09/24
 * @brief Reusable sub-circuit definitions (blueprints) with named ports.
 */

#ifndef MODULE_H
#define MODULE_H

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "BuiltinGate.h"
#include "FlipFlop.h"
#include "Gate.h"

/**
 * @class Module
 * @brief A named, reusable circuit with input and output ports.
 *
 * A Module is only a description: it owns no nets or gates. Its body refers
 * to nets by local name; a port name refers to whatever net the port is bound
 * to, any other name is a net private to each instance, and an empty name
 * leaves the pin unconnected. Modules may contain instances of other modules.
 *
 * Simulator::instantiate() flattens a Module into real gates and nets, named
 * hierarchically ("fa0.ha1.sum"), so the kernel only ever sees plain gates.
 *
 * @code
 *   auto ha = std::make_shared<Module>("half_adder",
 *       std::vector<std::string>{"a", "b"}, std::vector<std::string>{"s", "c"});
 *   ha->addGate(BuiltinGate::Type::XOR, {"a", "b"}, {"s"});
 *   ha->addGate(BuiltinGate::Type::AND, {"a", "b"}, {"c"});
 *   sim.instantiate(*ha, {&a, &b}, {&sum, &carry}, "ha0");
 * @endcode
 */
class Module
{
public:
    /// Builds one primitive gate for an instance. Receives the resolved pins
    /// and the full hierarchical name.
    using Factory = std::function<std::unique_ptr<Gate>(std::vector<Net*> inputs,
                                                        std::vector<Net*> outputs,
                                                        std::string name)>;

    /**
     * @throws std::invalid_argument if a port name is empty or repeated.
     */
    Module(std::string name,
           std::vector<std::string> input_ports,
           std::vector<std::string> output_ports);

    // --- Body ------------------------------------------------------------
    // Each add* returns *this so calls can be chained. Item names must be
    // unique within the module; an empty name is replaced by "<type><index>".
    // Pin counts are validated here, so instantiation cannot fail on them.

    /// @throws std::invalid_argument on bad pin counts or a duplicate name.
    Module& addGate(BuiltinGate::Type type,
                    std::vector<std::string> inputs,
                    std::vector<std::string> outputs,
                    std::string name = "");

    /// @throws std::invalid_argument on too many pins or a duplicate name.
    Module& addFlipFlop(FlipFlop::Type type,
                        std::vector<std::string> inputs,
                        std::vector<std::string> outputs,
                        std::string name = "",
                        FlipFlop::Edge edge = FlipFlop::Edge::Rising);

    /// Gated D latch: inputs {D, EN}, outputs {Q, QN}.
    /// @throws std::invalid_argument on too many pins or a duplicate name.
    Module& addLatch(std::vector<std::string> inputs,
                     std::vector<std::string> outputs,
                     std::string name = "",
                     bool active_high = true);

    /// Any Gate subclass, built by @p make at instantiation time.
    /// @throws std::invalid_argument if @p make is empty or the name repeats.
    Module& addPrimitive(Factory make,
                         std::vector<std::string> inputs,
                         std::vector<std::string> outputs,
                         std::string name = "");

    /**
     * @brief Nests another module. @p inputs / @p outputs bind its ports, in
     *        order, to nets of this module.
     * @throws std::invalid_argument if @p sub is null, the pin counts do not
     *         match its ports, or the name repeats.
     */
    Module& addInstance(std::shared_ptr<const Module> sub,
                        std::vector<std::string> inputs,
                        std::vector<std::string> outputs,
                        std::string name = "");

    // --- Accessors -------------------------------------------------------

    const std::string& getName() const { return name_; }
    const std::vector<std::string>& inputPorts() const { return input_ports_; }
    const std::vector<std::string>& outputPorts() const { return output_ports_; }

    /// Number of gates and sub-instances directly in this module's body.
    std::size_t itemCount() const { return items_.size(); }

private:
    friend class Simulator;

    struct Item
    {
        Factory make;                       ///< Set for a primitive...
        std::shared_ptr<const Module> sub;  ///< ...or this for an instance.
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
        std::string name;
    };

    void addItem_(Item item, const char* default_prefix);

    std::string name_;
    std::vector<std::string> input_ports_;
    std::vector<std::string> output_ports_;
    std::vector<Item> items_;
};

#endif
