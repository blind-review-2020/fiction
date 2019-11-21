//
// Created by Gregor Kuhn on 9/26/18.
//
#include "json_writer.h"

json_writer::json_writer(fcn_gate_library_ptr gl)
        :
        gate_layout{gl->get_layout()},
        router{gl->get_port_router()},
        library{gl}
{
    router->compute_ports();
    cell_layout = nullptr;
}

json_writer::json_writer(fcn_cell_layout_ptr cl)
        :
        cell_layout{cl}
{
    library = cl->get_library();
    gate_layout = library->get_layout();
    router = library->get_port_router();
}

json_writer::json json_writer::export_gate_layout()
{
    json layout_json{}, tile_json{};

    for (auto&& item : gate_layout->tiles()
                       | iter::filterfalse([this](const fcn_gate_layout::tile& _t)
                                           { return gate_layout->is_free_tile(_t); }))
    {
        json j{};

        j["name"] = gate_layout->is_pi(item) ? gate_layout->get_inp_names(item)[0] :
                (gate_layout->is_po(item) ? gate_layout->get_out_names(item)[0] : "");

        operation tile_op = gate_layout->get_op(item);
        j["functionName"] = name_str(tile_op);

        j["ports"] = tile_op == operation::W ? export_wire(item) : export_gate(item);

        auto clk = gate_layout->tile_clocking(item);
        j["clocking"] = clk ? std::to_string(*clk + 1) : "none";

        if(unsigned offset = gate_layout->get_latch(item) > 0u)
            j["clockLatch"] = offset;

        // Coordinates of tiles are unique and can therefore be used to identify them
        tile_json[std::to_string(item[X] + 1) + "," + std::to_string(item[Y] + 1) + "," +
                  std::to_string(item[Z] + 1)] = j;
    }

    layout_json.emplace_back(generate_header(true));
    layout_json.emplace_back(tile_json);

    return layout_json;
}

json_writer::json json_writer::export_cell_layout()
{

    json header{}, body{}, layout_json{};

    header = generate_header(false);
    body = generate_body_cell_layout();

    layout_json.emplace_back(header);
    layout_json.emplace_back(body);

    return layout_json;
}

json_writer::json json_writer::generate_header(bool exporting_gate_layout)
{
    json header{};

    std::string clocking{};
    auto cs = library->get_layout()->get_clocking_scheme();

    if (cs == use_4_clocking)
        clocking = "USE";
    else if (cs == twoddwave_3_clocking)
        clocking = "2DDWAVE3";
    else if (cs == twoddwave_4_clocking)
        clocking = "2DDWAVE4";
    else if (cs == open_3_clocking)
        clocking = "OPEN3";
    else if (cs == open_4_clocking)
        clocking = "OPEN4";
    else if(cs == res_4_clocking)
        clocking = "RES";
    else if(cs == bancs_3_clocking)
        clocking = "BANCS";

    header["clockingScheme"] = clocking;

    std::stringstream ss{};
    auto bounding_box = library->get_layout()->determine_bounding_box();

    if (exporting_gate_layout)
    {
        ss << bounding_box.min_x + 1 << "," << bounding_box.min_y + 1 << ","
           << bounding_box.max_x + 1 << "," << bounding_box.max_y + 1;

        header["nameLibrary"] = library->get_name();
    }
    else
    {
        auto tile_size = library->get_tile_size();

        ss << bounding_box.min_x * tile_size << "," << bounding_box.min_y * tile_size << ","
           << (bounding_box.max_x + 1) * tile_size << "," << (bounding_box.max_y + 1) * tile_size;
    }

    header["area"] = ss.str();
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    ss.str(std::string());
    ss << now.date().year() << "-" << static_cast<int>(now.date().month()) << "-" << now.date().day();
    header["date"] = ss.str();
    header["tool"] = VERSION;

    return header;
}

json_writer::json json_writer::generate_body_cell_layout()
{
    json body{};

    for (auto&& c : cell_layout->cells()
                       | iter::filterfalse([this](const fcn_cell_layout::cell& _c)
                                           { return cell_layout->is_free_cell(_c); }))
    {
        json current_cell{};

        current_cell["type"] = type_name_map.at(cell_layout->get_cell_type(c));

        auto clk = cell_layout->cell_clocking(c);
        current_cell["clocking"] = clk ? std::to_string(*clk + 1) : "none";

        std::stringstream ss;
        ss << c[0] << "," << c[1] << "," << c[2];

        body[ss.str()] = current_cell;
    }

    return body;
}

json_writer::json json_writer::export_gate(const fcn_gate_layout::tile& gate)
{
    json ports_json{};

    port_router::port_list gate_ports;
    if (auto gate_vertex = gate_layout->get_logic_vertex(gate))
    {
        gate_ports = router->get_ports(gate, *gate_vertex);

        for (json& p : generate_port_json(gate, gate_ports))
            ports_json.emplace_back(p);
    }

    return ports_json;
}

json_writer::json json_writer::export_wire(const fcn_gate_layout::tile& wire)
{
    json wire_json;

    port_router::port_list wire_ports;
    for (auto& edge : gate_layout->get_logic_edges(wire))
    {
        wire_ports = router->get_ports(wire, edge);

        for (json& current : generate_port_json(wire, wire_ports))
            wire_json.emplace_back(current);
    }

    return wire_json;

}

std::vector<json_writer::json>
json_writer::generate_port_json(const fcn_gate_layout::tile& tile, const port_router::port_list& ports)
{
    std::vector<json> port_vector{};

    //NOTE: ports can, in theory, also be in planes higher than 1. Stacked circuits are not handled here; this code
    //assumes a ground layer and one crossover layer above, not more layers.
    if(tile[Z] == 1)
    {
        for (auto&& p : iter::chain(ports.inp, ports.out))
        {
            //TODO: find out Tile-size
            //for finding the plane of the given port (its z-coordinate), its neighboring tile is needed. It can be found by
            //the placement of the port.
            fcn_gate_layout::tile neighbor;
            if(p.x == 0)
                neighbor = (*gate_layout)(tile[X] - 1, tile[Y], tile[Z]);
            else if(p.x == 4)
                neighbor = (*gate_layout)(tile[X] + 1, tile[Y], tile[Z]);
            else if(p.y == 0)
                neighbor = (*gate_layout)(tile[X], tile[Y] - 1, tile[Z]);
            else if(p.y == 4)
                neighbor = (*gate_layout)(tile[X], tile[Y] + 1, tile[Z]);


            //It is fine to assume that there is at least one edge on the current tile if it does not have a vertex
            //because only non-empty tiles are considered and it can be assumed that there are no faulty layouts stored.
            fcn_gate_layout::gate_or_wire gw;
            if(auto vert = gate_layout->get_logic_vertex(tile))
                gw = fcn_gate_layout::gate_or_wire{*vert};
            else
                gw = fcn_gate_layout::gate_or_wire{*(gate_layout->get_logic_edges(tile).begin())};

            //if neighboring tile has information flow with the given one, then it is in the same layer, the crossover
            //layer per construction above. Otherwise the connecting tile needs to be in the ground layer, mandating a
            //port in that layer. The information about the connection of tiles to the logic network can be stripped
            //away since it is not important here.
            auto in_out = gate_layout->incoming_data_flow(tile, gw);
            auto out = gate_layout->outgoing_data_flow(tile, gw);
            in_out.insert(in_out.end(), out.begin(), out.end());
            std::vector<fcn_gate_layout::tile> flow_tiles;
            std::for_each(in_out.begin(), in_out.end(), [&flow_tiles](auto a){flow_tiles.push_back(a.first);});
            int layer = std::find(flow_tiles.begin(), flow_tiles.end(), neighbor) != flow_tiles.end() ? 2 : 1;

            port_vector.push_back(json::object({{"net", p.net}, {"X",   p.x + 1}, {"Y",   p.y + 1}, {"Z", layer}}));
        }
    }
    else
    {
        for (auto&& p : iter::chain(ports.inp, ports.out))
            port_vector.push_back(json::object({{"net", p.net}, {"X",   p.x + 1}, {"Y",   p.y + 1}, {"Z", 1}}));
    }

    return port_vector;
}
