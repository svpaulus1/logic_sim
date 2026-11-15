/**
 * @file Module.h
 * @author Sebastian Paulus
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
 * to nets by local name:
 *  - a port name refers to whatever net the port is bound to;
 *  - any other name is a net private to each instance;
 *  - an empty name leaves the pin unconnected.
 *
 * Modules may contain instances of other modules. Simulator::instantiate()
 * flattens a Module into real gates and nets, named hierarchically
 * ("fa0.ha1.sum"), so the kernel only ever sees plain gates.
 *
 * Every add* function returns *this so calls can be chained. Item names must
 * be unique within the module; an empty name is replaced by
 * "<type><index>". Pin counts are validated when an item is added, so
 * instantiation cannot fail on them.
 *
 * @code
 *   auto ha = std::make_shared<Module>("half_adder",
 *       std::vector<std::string>{"a", "b"},
 *       std::vector<std::string>{"s", "c"});
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
     * @brief Creates an empty module with the given ports.
     *
     * @param name         Module name, also the default instance-name prefix.
     * @param input_ports  Names of the input ports, in pin order.
     * @param output_ports Names of the output ports, in pin order.
     *
     * @throws std::invalid_argument if a port name is empty or repeated.
     */
    Module(std::string name,
           std::vector<std::string> input_ports,
           std::vector<std::string> output_ports);

    // --- Body ------------------------------------------------------------

    /**
     * @brief Adds a primitive gate to the body.
     *
     * @param type    Which primitive.
     * @param inputs  Local net names for the input pins.
     * @param outputs Local net names for the output pins.
     * @param name    Item name; "<keyword><index>" if empty.
     * @return This module.
     * @throws std::invalid_argument on bad pin counts or a duplicate name.
     */
    Module& addGate(BuiltinGate::Type type,
                    std::vector<std::string> inputs,
                    std::vector<std::string> outputs,
                    std::string name = "");

    /**
     * @brief Adds a flip-flop to the body (see FlipFlop for the pin layout).
     *
     * @param type    Which flip-flop.
     * @param inputs  Local net names for the input pins.
     * @param outputs Local net names for {Q, QN}.
     * @param name    Item name; "ff<index>" if empty.
     * @param edge    Clock edge that loads it.
     * @return This module.
     * @throws std::invalid_argument on too many pins or a duplicate name.
     */
    Module& addFlipFlop(FlipFlop::Type type,
                        std::vector<std::string> inputs,
                        std::vector<std::string> outputs,
                        std::string name = "",
                        FlipFlop::Edge edge = FlipFlop::Edge::Rising);

    /**
     * @brief Adds a gated D latch to the body: inputs {D, EN}, outputs
     *        {Q, QN}.
     *
     * @param inputs      Local net names for {D, EN}.
     * @param outputs     Local net names for {Q, QN}.
     * @param name        Item name; "latch<index>" if empty.
     * @param active_high true: transparent while EN is HIGH; false: while LOW.
     * @return This module.
     * @throws std::invalid_argument on too many pins or a duplicate name.
     */
    Module& addLatch(std::vector<std::string> inputs,
                     std::vector<std::string> outputs,
                     std::string name = "",
                     bool active_high = true);

    /**
     * @brief Adds any Gate subclass, built by @p make at instantiation time.
     *
     * @param make    Builds the gate from its resolved pins and full name.
     * @param inputs  Local net names for the input pins.
     * @param outputs Local net names for the output pins.
     * @param name    Item name; "u<index>" if empty.
     * @return This module.
     * @throws std::invalid_argument if @p make is empty or the name repeats.
     */
    Module& addPrimitive(Factory make,
                         std::vector<std::string> inputs,
                         std::vector<std::string> outputs,
                         std::string name = "");

    /**
     * @brief Nests another module. @p inputs / @p outputs bind its ports, in
     *        order, to nets of this module.
     *
     * @param sub     The module to nest.
     * @param inputs  Local net names, one per input port of @p sub.
     * @param outputs Local net names, one per output port of @p sub.
     * @param name    Item name; "<module name><index>" if empty.
     * @return This module.
     * @throws std::invalid_argument if @p sub is null, the pin counts do not
     *         match its ports, or the name repeats.
     */
    Module& addInstance(std::shared_ptr<const Module> sub,
                        std::vector<std::string> inputs,
                        std::vector<std::string> outputs,
                        std::string name = "");

    // --- Accessors -------------------------------------------------------

    /// @brief The module's name.
    /// @return The name passed to the constructor.
    const std::string& getName() const { return name_; }

    /// @brief Names of the input ports.
    /// @return The names, in pin order.
    const std::vector<std::string>& inputPorts() const { return input_ports_; }

    /// @brief Names of the output ports.
    /// @return The names, in pin order.
    const std::vector<std::string>& outputPorts() const { return output_ports_; }

    /// @brief Number of gates and sub-instances directly in this module's body.
    /// @return The item count (nested modules count as one each).
    std::size_t itemCount() const { return items_.size(); }

private:
    /// Reads the body when flattening it in Simulator::instantiate().
    friend class Simulator;

    /// One entry of the body: a primitive or a nested module.
    struct Item
    {
        Factory make;                       ///< Set for a primitive...
        std::shared_ptr<const Module> sub;  ///< ...or this for an instance.
        std::vector<std::string> inputs;    ///< Local net names, input pins.
        std::vector<std::string> outputs;   ///< Local net names, output pins.
        std::string name;                   ///< Unique item name.
    };

    /**
     * @brief Names the item if needed, checks the name is unique and stores it.
     *
     * @param item           The item to add.
     * @param default_prefix Prefix for a generated name.
     * @throws std::invalid_argument if the name repeats.
     */
    void addItem_(Item item, const char* default_prefix);

    std::string name_;                      ///< Module name.
    std::vector<std::string> input_ports_;  ///< Input port names, pin order.
    std::vector<std::string> output_ports_; ///< Output port names, pin order.
    std::vector<Item> items_;               ///< The body, in insertion order.
};

#endif
