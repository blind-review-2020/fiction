//
// Created by Gregor Kuhn on 9/26/18.
//

#ifndef FICTION_EXPORT_JSON_H
#define FICTION_EXPORT_JSON_H

#include "fcn_gate_layout.h"
#include "fcn_cell_layout.h"
#include "fcn_technology.h"
#include "fcn_gate_library.h"
#include "port_router.h"
#include "directions.h"
#include "version.h.in"
#include "nlohmann/json.hpp"
#include <memory>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>

class json_writer
{
    using json = nlohmann::json;

public:
    /**
     * Constructor for gate layouts. Cell layout is not initialized.
     *
     * @param gl Gate library to be exported.
     */
    explicit json_writer(fcn_gate_library_ptr gl);
    /**
     * Constructor for cell layouts.
     *
     * @param cl The cell layout to be exported
     */
    explicit json_writer(fcn_cell_layout_ptr cl);
    /**
     * Default destructor.
     */
    ~json_writer() = default;
    /**
     * Copy constructor is not available.
     */
    json_writer(const json_writer& exp) = delete;
    /**
     * Move constructor is not available.
     */
    json_writer(json_writer&& rhs) = delete;
    /**
     * Assignment operator is not available.
     */
    json_writer& operator=(const json_writer& rhs) = delete;
    /**
     * Move assignment operator is not available.
     */
    json_writer& operator=(json_writer&& rhs) = delete;
    /**
     * Generates a JSON representation of the stored gate layout.
     *
     * @return A JSON representation of the stored gate layout.
     */
    json export_gate_layout();
    /**
     * Generates a JSON representation of the stored cell layout.
     *
     * @return A JSON representation of the stored cell layout.
     */
    json export_cell_layout();

private:
    /**
     * Gate layout that should be exported.
     */
    fcn_gate_layout_ptr gate_layout;
    /**
     * Cell layout that should be exported.
     */
    fcn_cell_layout_ptr cell_layout;
    /**
    * Port router to be used for getting the port positions within tiles.
    */
    port_router_ptr router;
    /**
     * Library that is used. Gives information on technology and tile size.
     */
    fcn_gate_library_ptr library;
    /**
     * Maps cell types to their string representation in the Interchange Format.
     */
    std::unordered_map<fcn::cell_type, std::string> type_name_map{{fcn::NORMAL_CELL, "normal"}, {fcn::CONST_0_CELL, "const_0"},
                                                                  {fcn::CONST_1_CELL, "const_1"}, {fcn::INPUT_CELL, "in"}, {fcn::OUTPUT_CELL, "out"}};
    /**
     * Generates the header for the JSON-file. Different information based on distinction between gate and cell layouts.
     *
     * @param exporting_gate_layout Indicates whether a gate or a cell layout is to be exported.
     * @return The header of the JSON export.
     */
    json generate_header(bool exporting_gate_layout);
    /**
     * Exports a tile-based cell layout by generating the body of the JSON export.
     *
     * @return The body of the JSON export.
     */
    json generate_body_cell_layout();
    /**
     * Generates a JSON representation of the given tile with a gate on it.
     *
     * @param gate The gate to export.
     * @return A JSON representation of the given gate-tile.
     */
    json export_gate(const fcn_gate_layout::tile& gate);
    /**
     * Generates a JSON representation of the given tile with a wire on it.
     *
     * @param wire The wire to export.
     * @return A JSON representation of the given wire-tile.
     */
    json export_wire(const fcn_gate_layout::tile& wire);
    /**
     * Generates a JSON representation of the netlist of a given tile based on its ports.
     *
     * @param tile The tile whose netlist is to be exported.
     * @param ports The ports in the given tile.
     * @return A vector containing a representation of all ports in the given tile and portlist.
     */
    std::vector<json> generate_port_json(const fcn_gate_layout::tile& tile, const port_router::port_list& ports);
};

#endif //FICTION_EXPORT_JSON_H
