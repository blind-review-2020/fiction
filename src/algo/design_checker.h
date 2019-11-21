//
// Created on 26.04.19.
//

#ifndef FICTION_DESIGN_CHECKER_H
#define FICTION_DESIGN_CHECKER_H

#include "fcn_gate_layout.h"
#include "logic_network.h"
#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include "fmt/ostream.h"
#include <ostream>
#include <string>
#include <sstream>


class design_checker
{
public:
    /**
     * Standard constructor.
     *
     * @param fgl Gate layout to check for design rule flaws.
     * @param wl Maximum number of wires per tile.
     */
    design_checker(fcn_gate_layout_ptr fgl, std::size_t wl = 1);
    /**
     * Default Destructor.
     */
    ~design_checker() = default;
    /**
     * Copy constructor is not available.
     */
    design_checker(const design_checker& rhs) = delete;
    /**
     * Move constructor is not available.
     */
    design_checker(design_checker&& rhs) = delete;
    /**
     * Assignment operator is not available.
     */
    design_checker& operator=(const design_checker& rhs) = delete;
    /**
     * Move assignment operator is not available.
     */
    design_checker& operator=(design_checker&& rhs) = delete;
    /**
     * Performs design rule checks on the stored gate layout. The following properties are checked.
     *
     *  Design breaking:
     *   - Too many wires per tile
     *   - No missing connections in the layout
     *   - No wires crossing gates
     *   - Directions assigned against clocking not respecting data flow
     *   - No wires or empty tiles are PI/POs
     *
     *  Warning:
     *   - Non-empty tiles without clocking
     *   - Not all PO/PIs located at layout's borders
     *   - Gate I/Os instead of designated PI/PO ports
     *
     * @return A detailed report containing all check results.
     */
    nlohmann::json check(std::ostream& out = std::cout) noexcept;

private:
    /**
     * Layout to perform design rule checks on.
     */
    fcn_gate_layout_ptr layout;
    /**
     * Maximum number of logic edges allowed per tile.
     */
    const std::size_t wire_limit;
    /**
     * Escape color sequence for passed checks followed by a check mark.
     */
    const char* CHECK_PASSED = "[\033[38;5;28m✓\033[0m]";
    /**
     * Escape color sequence for failed checks followed by an x mark.
     */
    const char* CHECK_FAILED = "[\033[38;5;166m✗\033[0m]";
    /**
     * Escape color sequence for warnings followed by an exclamation point.
     */
    const char* WARNING = "[\033[38;5;226m!\033[0m]";
    /**
     * Logs information about the given tile in the given report. Assigned vertex' or edges' ids are logged in this
     * process under the tile position.
     *
     * @param t Tile whose assigned elements are the be logged.
     * @param report JSON report to be extended by the given tile.
     */
    void log_tile(const fcn_gate_layout::tile& t, nlohmann::json& report) const noexcept;
    /**
     * Generates a summarizing one liner for a design rule check.
     *
     * @param msg Message to output in success case. For failure, a "not" will be added as a prefix.
     * @param chk Result of the check.
     * @param brk Flag to indicate that a failure is design breaking. If it's not, msg is printed as a warning.
     * @return Formatted summary message.
     */
    std::string summary(std::string&& msg, const bool chk, const bool brk) const noexcept;
    /**
     * Checks for too many assigned logic edges on a single tile.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string wire_count_check(nlohmann::json& report) const noexcept;
    /**
     * Checks for non-PO tiles with successors and non-PI tiles without predecessors.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string missing_connections_check(nlohmann::json& report) const noexcept;
    /**
     * Check for wires crossing gates.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string crossing_gates_check(nlohmann::json& report) const noexcept;
    /**
     * Checks for unclocked non-empty tiles.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string tile_clocking_check(nlohmann::json& report) const noexcept;
    /**
     * Checks for directions assigned against clocking.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string direction_check(nlohmann::json& report) const noexcept;
    /**
     * Checks if no PI/PO is assigned to a wire or an empty tile.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string operation_io_check(nlohmann::json& report) const noexcept;
    /**
     * Checks if all PI/POs are designated pins.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string io_port_check(nlohmann::json& report) const noexcept;
    /**
     * Checks if all PI/POs are located at the layout's borders.
     *
     * @param report Report to add results to.
     * @return Check summary as a one liner.
     */
    std::string border_io_check(nlohmann::json& report) const noexcept;
};


#endif //FICTION_DESIGN_CHECKER_H
