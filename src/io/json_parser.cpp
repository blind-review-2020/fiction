//
// Created by gregor on 11/2/18.
//

#include "json_parser.h"

void json_parser::tile_rep::print()
{
    std::cout << "X: " << x << " Y: " << y << " Z: " << z << " Clock: " << clock_zone
              << "Latch delay: " << latch_offset << " Function: " << function << "\n";
    std::cout << "In-Ports: ";
    for (auto& in_port : in_ports)
        std::cout << in_port.to_string() << ", ";
    std::cout << "\nOut-Ports: ";
    for (auto& out_port : out_ports)
        std::cout << out_port.to_string() << ", ";
    std::cout << std::endl;
}

bool json_parser::tile_rep::operator==(const json_parser::tile_rep& rhs)
{
    return this->x == rhs.x && this->y == rhs.y && this->z == rhs.z;
}

json_parser::coords json_parser::tile_rep::get_coords()
{
    return std::make_tuple(this->x, this->y, this->z);
}

std::string json_parser::tile_rep::print_coords()
{
    //coordinates need to be incremented by 1 because of the way the Interchange Format represents coordinates
    return std::to_string(this->x + 1) + ", " + std::to_string(this->y + 1) + ", " + std::to_string(this->z + 1);
}

std::string json_parser::port::to_string()
{
    std::stringstream ss;
    ss << "{" << this->x << ", " << this->y << ", " << this->z << "; net: " << this->net << "}";
    return ss.str();
}

json_parser::json_parser(const std::string& filename, logic_network_ptr lnp, fcn_gate_layout_ptr& fgp)
        :
        ln(std::move(lnp)),
        fgl(fgp)
{
    coord_to_tile = coord_tile_map{};
    std::ifstream infile(filename);
    if (infile.fail())
        throw std::invalid_argument("could not open file " + filename);
    else if (infile.peek() == std::ifstream::traits_type::eof())
        throw std::invalid_argument("file " + filename + " is empty");
    json j;
    try
    {
        infile >> j;
    } catch (...)
    {
        throw std::invalid_argument("could not interpret file as JSON");
    }
    to_parse = j;
}

void json_parser::parse()
{
    if (!to_parse.is_array() && to_parse.size() != 2)
    {
        throw std::invalid_argument("top level of JSON needs to be an array of length 2 with first element being the "
                                    "header and second element being the description of tiles");
    }

    json header = to_parse.at(0);

    std::string library;
    try
    {
        library = header.at("nameLibrary").get<std::string>();
    } catch (...)
    {
        throw std::invalid_argument("header does not contain name of library at key \"nameLibrary\"");
    }

    std::string clocking;
    try
    {
        clocking = header.at("clockingScheme").get<std::string>();
    } catch (...)
    {
        throw std::invalid_argument("header does not contain the layout's clocking scheme at key \"clockingScheme\"");
    }

    //TODO: Find out whether gate_layout object can be created here, aka whether dimensions can be reset after creation
    //dimensions of layout are the only thing not set here and only Z-Coordinates are missing.
    if (clocking == "2DDWAVE4" || clocking == "DIAG4" || clocking == "OPEN4" || clocking == "USE"
        || clocking == "RES")
    {
        num_clocks = 4;
    }
    else if (clocking == "2DDWAVE3" || clocking == "DIAG3" || clocking == "OPEN3" || clocking == "BANCS")
    {
        num_clocks = 3;
    }
    else
    {
        throw std::invalid_argument("unknown clocking scheme");
    }

    try
    {
        auto box_string = header.at("area").get<std::string>();
        std::vector<std::string> corner_points;
        boost::split(corner_points, box_string, [](char c) { return c == ','; });
        if (corner_points.size() != 4)
            throw std::invalid_argument("bounding box does not contain 4 elements");
        min_x = std::stoi(corner_points.at(0)) - 1;
        min_y = std::stoi(corner_points.at(1)) - 1;
        max_x = std::stoi(corner_points.at(2)) - 1;
        max_y = std::stoi(corner_points.at(3)) - 1;
    } catch (const std::invalid_argument& e)
    {
        throw e;
    } catch (...)
    {
        throw std::invalid_argument("header does not contain bounding-box at key \"area\"");
    }

    if (library == "QCA-ONE")
    {
        //TODO: find a prettier solution to the clockingscheme-problem and simplify this
        parse_qca_one(to_parse[1], clocking);
    }
    else
        throw std::invalid_argument("unknown library " + library);
}

void json_parser::parse_qca_one(const json& coord_port_mapping, std::string& clocking)
{

    //This is a map from the coordinates of a tile to its description in JSON
    std::map<std::string, json> mapping{};
    try
    {
        mapping = coord_port_mapping.get<std::map<std::string, json>>();
    } catch (...)
    {
        throw std::invalid_argument("could not interpret JSON as mapping from string to JSON");
    }

    //Because coordinates can be 0 in the Interchange Format and because fiction needs all coordinates to be
    //0 or positive, an offset for x and/or y needs to be determined.
    int x_offset = 0, y_offset = 0;
    if (min_x < 0)
        x_offset = -min_x;
    if (min_y < 0)
        y_offset = -min_y;

    int max_z = 0;

    //This contains all tiles with at least one PI
    std::vector<tile_rep> starting_tiles;

    for (auto& m : mapping)
    {
        tile_rep current;

        std::vector<std::string> coordinates;
        boost::split(coordinates, m.first, [](char c) { return c == ','; });
        current.x = std::stoi(coordinates[0]) - 1 + x_offset;

        current.y = std::stoi(coordinates[1]) - 1 + y_offset;

        current.z = std::stoi(coordinates[2]) - 1;
        if (current.z > max_z)
            max_z = current.z;

        try
        {
            current.clock_zone = std::atoi(m.second.at("clocking").get<std::string>().c_str()) - 1;
        } catch (std::exception &e)
        {
            std::stringstream ss;
            ss << "tile description of tile at coordinates " << current.print_coords()
                << " does not contain clock zone at key \"clocking\"" << std::endl;
            throw std::invalid_argument(ss.str());
        }

        try
        {
            current.function = m.second.at("functionName");
        } catch (...)
        {
            std::stringstream ss;
            ss << "tile description of tile at coordinates " << current.print_coords()
               << " does not contain function of gate at key \"functionName\"" << std::endl;
            throw std::invalid_argument(ss.str());
        }

        try
        {
            current.tile_name = m.second.at("name");
        } catch (...)
        {
            std::stringstream ss;
            ss << "tile description of tile at coordinates " << current.print_coords()
               << " does not contain name of gate at key \"name\"" << std::endl;
            throw std::invalid_argument(ss.str());
        }

        try
        {
            current.latch_offset = m.second.at("clockLatch");
        } catch(...)
        {
            current.latch_offset = 0;
        }


        tiles.push_back(current);
        coord_to_tile.emplace(current.get_coords(), current);
    }

    unsigned int i = 0;
    //Iterate over mapping again to fill Ports and find PI/POs
    for (auto& m : mapping)
    {
        //access to tile-representation corresponding to mapping
        std::vector<std::string> coordinates;
        boost::split(coordinates, m.first, [](char c) { return c == ','; });
        tile_rep current;
        try
        {
            current = coord_to_tile.at(std::make_tuple(std::stoi(coordinates[0]) - 1 + x_offset,
                                                       std::stoi(coordinates[1]) - 1 + y_offset,
                                                       std::stoi(coordinates[2]) - 1));
        } catch (...)
        {
            std::stringstream ss;
            ss << "expecting tile at coordinates " << std::stoi(coordinates[0]) + x_offset << ", "
                << std::stoi(coordinates[1]) + y_offset << ", " << coordinates[2] << " but did not find it";
            throw std::invalid_argument(ss.str());
        }

        try
        {
            auto port_list = m.second.at("ports");
            for (auto& entry : port_list)
            {
                try
                {
                    port current_port{entry.at("X").get<std::size_t>() - 1, entry.at("Y").get<std::size_t>() - 1,
                                      entry.at("Z").get<std::size_t>() - 1, entry.at("net").get<size_t>()};

                    //Find out if port is in or out

                    auto current_dir = direction_for_port(current_port);
                    coords adjacent_coordinates;
                    //Z-Coordinate of current port is important because there has to be a cell in that layer
                    if (current_dir == layout::DIR_N)
                    {
                        adjacent_coordinates = std::make_tuple(current.x, current.y - 1, current_port.z);
                    }
                    else if (current_dir == layout::DIR_E)
                    {
                        adjacent_coordinates = std::make_tuple(current.x + 1, current.y, current_port.z);
                    }
                    else if (current_dir == layout::DIR_S)
                    {
                        adjacent_coordinates = std::make_tuple(current.x, current.y + 1, current_port.z);
                    }
                    else if (current_dir == layout::DIR_W)
                    {
                        adjacent_coordinates = std::make_tuple(current.x - 1, current.y, current_port.z);
                    }
                    tile_rep neighbor;
                    try
                    {
                        neighbor = coord_to_tile.at(adjacent_coordinates);
                    } catch (...)
                    {
                        /*std::cout << "Current coordinates: " << current.print_coords() << "\n";
                        std::cout << "Direction: " << current_dir << "\n";
                        std::cout << "Port: " << current_port.to_string() << "\n";
                        std::cout << std::get<0>(adjacent_coordinates) << ", " << std::get<1>(adjacent_coordinates)
                                  << ", "
                                  << std::get<2>(adjacent_coordinates) << "\n";
                        std::cout << "Not found!\n";*/
                        std::stringstream ss;
                        ss << "missing tile at coordinates " << std::get<0>(adjacent_coordinates) + 1
                           << ", " << std::get<1>(adjacent_coordinates) + 1 << ", " << std::get<2>(adjacent_coordinates) + 1;
                        throw std::invalid_argument(ss.str());
                    }


                    auto neighbor_clock_zone = neighbor.clock_zone;
                    auto neighbor_latch = neighbor.latch_offset;
                    if ((current.clock_zone + current.latch_offset + 1) % num_clocks == neighbor_clock_zone)
                    {
                        current.out_ports.emplace_back(current_port);
                        //std::cout << "OUT: " << (current.clock_zone + 1) % num_clocks  << ", " << neighbor.clock_zone <<"\n";
                    }
                    if ((neighbor_clock_zone + neighbor_latch + 1) % num_clocks == current.clock_zone)
                    {
                        current.in_ports.emplace_back(current_port);
                        //std::cout << "IN: " << neighbor_clock_zone << ", " << neighbor.clock_zone <<"\n";
                    }
                    //std::cout << current.print_coords() << "\n";
                    coord_to_tile.at(current.get_coords()) = current;
                    try
                    {
                        tiles.at(i) = current;
                    } catch (...)
                    {
                        std::stringstream ss;
                        ss << "Error accessing list of tiles at index " << i;
                        throw std::invalid_argument(ss.str());
                    }
                } catch (const std::invalid_argument& e)
                {
                    throw e;
                } catch (...)
                {
                    std::stringstream ss;
                    ss << "error parsing a port of tile at coordinates " << current.print_coords() << "; ports need "
                        << "information at keys \"X\", \"Y\", \"Z\" and \"net\"";
                    throw std::invalid_argument(ss.str());
                }
            }
            ++i;
        } catch (const std::invalid_argument& e)
        {
            throw e;
        } catch (...)
        {
            std::stringstream ss;
            ss << "ports of tile at coordinates " << current.print_coords() << " are not listed at key \"ports\"";
            throw std::invalid_argument(ss.str());
        }

        coords current_coords = current.get_coords();
        //This construct finds all tiles with primary inputs through the explicit function
        //or missing in-ports in gate-tiles.
        //Wire tiles cannot contain PIs because they are either PIs directly or have implicit fan-outs
        //which are not distinguishable from PIs in multi-wire tiles
        if (current.function == "PI"
            || ((current.function == "INV" || current.function == "F1O2" || current.function == "F1O3")
                && current.in_ports.empty()))
        {
            starting_tiles.emplace_back(current);
            coord_to_pis[current_coords] = std::make_pair(current, 1);
        }
        else if ((current.function == "AND" || current.function == "OR" || current.function == "XOR")
                 && current.in_ports.size() < 2)
        {
            starting_tiles.emplace_back(current);
            coord_to_pis[current_coords] = std::make_pair(current, 2 - current.in_ports.size());
        }
        else if (current.function == "MAJ" && current.in_ports.size() < 3)
        {
            starting_tiles.emplace_back(current);
            coord_to_pis[current_coords] = std::make_pair(current, 3 - current.in_ports.size());
        }
        else if (current.function == "Wire" && current.in_ports.size() < current.out_ports.size())
        {
            starting_tiles.emplace_back(current);
            coord_to_pis[current_coords] = std::make_pair(current, current.out_ports.size() - current.in_ports.size());
        }

        //This constructs finds all tiles with primary outputs through the function "PO"
        //or missing out-ports
        //Wire tiles can only contain implicit POs when there are no implicit fan-outs; otherwise
        //the difference between "multi-wire + implicit PO" is not distinguishable from
        //"implicit fan-out + wire"
        if (current.function == "PO")
            coord_to_pos[current.get_coords()] = std::make_pair(current, 1);
        else if ((current.function == "AND" || current.function == "OR" || current.function == "XOR"
                  || current.function == "MAJ" || current.function == "NOT") && current.out_ports.empty())
            coord_to_pos[current_coords] = std::make_pair(current, 1);
        else if (current.function == "F1O2" && current.out_ports.size() < 2)
            coord_to_pos[current_coords] = std::make_pair(current, 2 - current.out_ports.size());
        else if (current.function == "F1O3" && current.out_ports.size() < 3)
            coord_to_pos[current_coords] = std::make_pair(current, 3 - current.out_ports.size());
        else if (current.function == "Wire" && current.in_ports.size() > current.out_ports.size())
            coord_to_pos[current_coords] = std::make_pair(current, current.in_ports.size() - current.out_ports.size());

    }
    //std::cout << "# of PI-Tiles found: " << coord_to_pis.size() << "\n";
    //std::cout << "# of PO-Tiles found: " << coord_to_pos.size() << "\n";

    //std::cout << "Tile-processing is done!\n";

    //start processing at the PIs, since all Tiles have to be reachable from at least one PI
    for (auto& entry : coord_to_pis)
    {
        tile_rep current_tile = entry.second.first;
        for (auto& out_port : current_tile.out_ports)
        {
            std::vector<tile_rep> pi_path;
            pi_path.emplace_back(current_tile);
            process_out_port(current_tile, out_port, pi_path);
        }
    }

    //std::cout << "Networkpaths created!\n";

    //std::cout << network_paths.size() << "\n";

    fill_logic_network();
    //std::cout << "Logic network is now filled.\n";

    //std::ofstream network_stream("../network.dot");
    //ln->write_network(network_stream);
    //network_stream.close();
    //std::cout << "Logic network written to ../network.dot\n";

    //dimensions need to be increased by 1 because coordinates start at 0
    ++max_x;
    ++max_y;
    ++max_z;

    //NOTE: Due to a bug in the BGL, every dimension should have a minimum size of 2 to prevent SEGFAULTs.
    //      See https://svn.boost.org/trac10/ticket/11735 for details.
    fcn_dimension_xyz dimensions{static_cast<size_t >(max_x < 3 ? 2 : max_x + x_offset),
                                 static_cast<size_t >(max_y < 3 ? 2 : max_y + y_offset),
                                 static_cast<size_t >(max_z < 3 ? 2 : max_z)};
    //std::cout << "Max x: " << dimensions[0] << " Max y: " << dimensions[1] << " Max z: "
    //          << dimensions[2] << std::endl;

    //No need to check whether clocking refers to a supported clocking scheme because an error would have been found
    //while parsing the header.
    fgl = std::make_shared<fcn_gate_layout>(std::move(dimensions), *(get_clocking_scheme(clocking)), ln);

    fill_gate_layout();

    //std::cout << "Gate layout is now filled\n";
}


void json_parser::process_out_port(tile_rep& current, json_parser::port& current_out_port,
                                   std::vector<tile_rep>& current_network_path)
{

    //find tile with in-port fitting to current_out_port
    auto corresponding_in_port = get_adjacent_port(current_out_port);
    //now check whether there is a tile and whether it has an in-port where it is needed
    coords coords_of_adjacent_tile;

    auto in_dir = std::get<3>(corresponding_in_port);
    //No need to handle else-cases as "get_adjacent_port" would have thrown an exception. If that is not the case, then
    //"in_dir" is a cardinal direction
    if (in_dir == layout::DIR_N)
        coords_of_adjacent_tile = std::make_tuple(current.x, current.y + 1, 1);
    else if (in_dir == layout::DIR_W)
        coords_of_adjacent_tile = std::make_tuple(current.x + 1, current.y, 1);
    else if (in_dir == layout::DIR_S)
        coords_of_adjacent_tile = std::make_tuple(current.x, current.y - 1, 1);
    else if (in_dir == layout::DIR_E)
        coords_of_adjacent_tile = std::make_tuple(current.x - 1, current.y, 1);

    //NOTE: this does not support stacked circuits.
    //The code below finds the adjacent tile. Said tile can be in plane 0 or 1, depending on crossovers.
    //Both planes are checked for a tile with a port corresponding to the current out-port.
    tile_rep adjacent;
    port in_port{std::get<0>(corresponding_in_port), std::get<1>(corresponding_in_port),
                 std::get<2>(corresponding_in_port), 0};
    try
    {
        adjacent = coord_to_tile.at(coords_of_adjacent_tile);
        if(std::find(adjacent.in_ports.begin(), adjacent.in_ports.end(), in_port) == adjacent.in_ports.end())
        {
            //port not found in plane 1, must be in plane 0 instead.
            throw std::invalid_argument("");
        }

    }
    catch (...)
    {
        //Adjacent tile could not be found in plane 1 either because there is no tile in plane 1 or because ports did
        //not fit. Checking plane 0 now.
        std::get<2>(coords_of_adjacent_tile) = 0;
        try {
            adjacent = coord_to_tile.at(coords_of_adjacent_tile);
            if(std::find(adjacent.in_ports.begin(), adjacent.in_ports.end(), in_port) == adjacent.in_ports.end())
                throw std::invalid_argument("");
        }
        catch(const std::out_of_range& e)
        {
            //Tile does not exist
            std::stringstream ss;
            ss << "Tile at coordinates " << std::get<0>(coords_of_adjacent_tile) + 1 << ", "
                    << std::get<1>(coords_of_adjacent_tile) + 1 << ", " << 0 << " does not exist" << std::endl;
            throw std::invalid_argument(ss.str());
        }
        catch(...)
        {
            //Port does not exist
            std::stringstream ss;
            ss << "Port " << std::get<0>(corresponding_in_port) + 1 << ", " << std::get<1>(corresponding_in_port) + 1
                    << std::get<2>(corresponding_in_port) + 1 << " cannot be found either in plane 0 or 1 of tile at "
                    << "coordinates " << std::get<0>(coords_of_adjacent_tile) + 1 << ", "
                    << std::get<1>(coords_of_adjacent_tile) << ", 0/1" <<  std::endl;
            throw std::invalid_argument(ss.str());
        }

    }

    //Add the newly found adjacent tile to the current path
    if (adjacent.function == "WIRE")
    {
        //Adjacent tile is a wire and can have implicit fanouts. It needs to be added to the current path and
        //all of its out-ports need to be considered
        current_network_path.emplace_back(adjacent);
        //get relevant in- and out-ports
        auto port_map = create_port_map(adjacent);

        layout::directions out_dirs;

        std::vector<port> out_ports;
        try
        {
            out_ports = port_map.at(in_port);
        } catch (...)
        {
            std::stringstream ss;
            ss << "port " << in_port.x << ", " << in_port.y << ", " << in_port.z << " of tile at " << adjacent.print_coords()
                << " does not have any ports associated with itself";
            throw std::invalid_argument(ss.str());
        }


        //If there is more than one out-port, all tiles that are on the current path need
        //their directions for more than one network-path; they need to exist for all
        //out-ports and thus are duplicated here. It can be assumed by the way the processing
        //of out-ports works that they will have network_path-counters that are incremented by 1
        //per out-port / duplication.
        if (out_ports.size() > 1)
        {
            //Starting duplication at index 1 because index 0 is handled below
            for (unsigned long i = 1; i < port_map.at(in_port).size(); ++i)
            {
                //Starting duplication of directions at index 1 because index 0 is a gate and only indices 1 to size - 1
                //can be wire-tiles. Need to use size - 1 as last index because indexing starts at 0.
                for (unsigned long j = 1; j < current_network_path.size() - 1; ++j)
                {
                    auto tile = current_network_path.at(j);
                    try
                    {
                        //Using network_paths.size() as the starting point here to make sure that multi-wire-paths
                        //are successive in network_paths. This ensures that they are handled in order when the paths
                        //are read.
                        path_dir_map[std::make_pair(tile.get_coords(), network_paths.size() + i)] =
                                path_dir_map.at(std::make_pair(tile.get_coords(), network_paths.size()));
                    } catch (...)
                    {
                        //std::cout << "Error accessing at (" << tile.print_coords() << "), " << network_paths.size() << "\n";
                    }
                }
            }
        }

        for (auto& p : port_map.at(in_port))
        {
            if (p.x == 0)
                out_dirs |= layout::DIR_W;
            else if (p.x == 4)
                out_dirs |= layout::DIR_E;
            else if (p.y == 0)
                out_dirs |= layout::DIR_N;
            else if (p.y == 4)
                out_dirs |= layout::DIR_S;

            //std::cout << "valid entry at index: " << network_paths.size() << ", ";
            //std::cout << "coordinates: " << adjacent.print_coords() << "\n";

            path_dir_map[std::make_pair(adjacent.get_coords(), network_paths.size())] =
                    std::make_pair(std::get<3>(corresponding_in_port), out_dirs);

            std::vector<tile_rep> copied_path(current_network_path);
            try
            {
                process_out_port(adjacent, p, copied_path);
            } catch (const std::invalid_argument& e)
            {
                throw e;
            }


            //std::cout << "path-dir-map; in-dir: " << std::get<2>(corresponding_in_port).to_ulong() << " out-dirs: " << out_dirs.to_ulong() << "\n";

        }
        return;
    }
    else if (adjacent.function == "AND" || adjacent.function == "OR" || adjacent.function == "XOR"
             || adjacent.function == "F1O2" || adjacent.function == "F1O3" || adjacent.function == "MAJ"
             || adjacent.function == "NOT" || adjacent.function == "PO")
    {
        //No need to check for PIs as they would have generated an error above because they don't have in-ports
        current_network_path.emplace_back(adjacent);
        network_paths.emplace_back(current_network_path);
        //Only add Gate to processed_tiles if it is not contained yet
        //This also means that its outgoing connections have not been processed yet
        if (std::find(processed_tiles.begin(), processed_tiles.end(), adjacent) == processed_tiles.end())
        {
            //mark this tile as processed
            processed_tiles.emplace_back(adjacent);
            //process its out-ports
            for (auto& out_port : adjacent.out_ports)
            {
                //Since the tile contains a gate, all out-ports must start new paths
                std::vector<tile_rep> new_path;
                new_path.emplace_back(adjacent);
                try
                {
                    process_out_port(adjacent, out_port, new_path);
                } catch (const std::invalid_argument& e)
                {
                    throw e;
                }
            }
        }
        return;
    }
    else
    {
        std::stringstream ss;
        ss << "Tile at coordinates " << adjacent.print_coords() << " does not have an accepted function";
        throw std::invalid_argument(ss.str());
    }
}

json_parser::port_mapping json_parser::create_port_map(const tile_rep& t)
{
    port_mapping port_map{};

    for (auto& in_port : t.in_ports)
    {
        std::size_t in_net = in_port.net;
        for (auto& out_port : t.out_ports)
        {
            if (in_net == out_port.net)
            {
                port_map[in_port].emplace_back(out_port);
            }
        }
    }

    return port_map;
}

std::tuple<std::size_t, std::size_t, std::size_t, layout::directions>
json_parser::get_adjacent_port(json_parser::port& out_port)
{
    //TODO: add support for things other than 5x5 (difference is length-1)
    if (out_port.x == 0)
        return std::make_tuple(4, out_port.y, out_port.z, layout::DIR_E);
    else if (out_port.x == 4)
        return std::make_tuple(0, out_port.y, out_port.z, layout::DIR_W);
    else if (out_port.y == 0)
        return std::make_tuple(out_port.x, 4, out_port.z, layout::DIR_S);
    else if (out_port.y == 4)
        return std::make_tuple(out_port.x, 0, out_port.z, layout::DIR_N);
    else
    {
        throw std::invalid_argument("port is not at the edge of tile");
    }
}

void json_parser::fill_logic_network()
{
    //Create all PI-vertices for later use
    for (auto& entry : coord_to_pis)
    {
        tile_rep current_tile = entry.second.first;
        logic_network::vertex vert;
        try
        {
            //Case 1: vertex already exists and can be retrieved.
            vert = coord_to_vertex.at(entry.first);
        } catch (...)
        {
            //Case 2: vertex does not yet exist and has to be created.
            vert = create_vertex(current_tile.function, current_tile.tile_name);
            coord_to_vertex[entry.first] = vert;
        }
        //All tiles with PIs on them have at least one PI-vertex now. It is possible for tiles to contain multiple PIs
        //though, so these have to be created too.
        for (int i = 1; i < entry.second.second; ++i)
        {
            auto pi_vert = create_vertex("PI", current_tile.tile_name);
            ln->create_edge(pi_vert, vert);
        }
    }

    //std::cout << "Starting processing of network paths!\n";

    //if network_paths is empty, then the circuit contains only one tile. This is acceptable behavior.
    unsigned int network_paths_counter = 0;
    //iterating over created paths. Paths with multi-wire tiles are next to each other in network_paths.
    for (auto& vec : network_paths)
    {
        tile_rep start = vec.front();
        tile_rep final = vec.back();
        logic_network::vertex start_vertex, end_vertex;
        //if vertices for coordinates of current source/target do not exist, create them
        try
        {
            start_vertex = coord_to_vertex.at(start.get_coords());
        } catch (const std::out_of_range&)
        {
            start_vertex = create_vertex(start.function, start.tile_name);
            coord_to_vertex.emplace(start.get_coords(), start_vertex);
        }
        try
        {
            end_vertex = coord_to_vertex.at(final.get_coords());
        } catch (const std::out_of_range&)
        {
            end_vertex = create_vertex(final.function, final.tile_name);
            coord_to_vertex.emplace(final.get_coords(), end_vertex);
        }

        //add edge between start and end, then assign it to all wire-tiles if it does not exist yet
        auto optional_edge = ln->get_edge(start_vertex, end_vertex);
        logic_network::edge edge = optional_edge ? *optional_edge : ln->create_edge(start_vertex, end_vertex);

        if (vec.size() > 2)
        {
            //iterate over all wire-tiles between start and end
            for (unsigned int i = 1; i < vec.size() - 1; ++i)
            {
                auto current_tile = vec.at(i);
                auto current_coords = current_tile.get_coords();
                //operator[] of map standard-constructs if key was not present before so vector is always initialized
                coord_to_edges[current_tile.get_coords()].emplace_back(edge);

                try
                {
                    edge_dir_map.insert(
                            {std::make_pair(current_coords, edge), path_dir_map.at(std::make_pair(current_coords,
                                                                                                  network_paths_counter))});

                } catch (...)
                {
                    //std::cout << "error accessing map at counter " << network_paths_counter << "\n";
                    //std::cout << "coordinates: " << current_tile.print_coords() << "\n";
                }
            }
        }
        ++network_paths_counter;
    }
    //std::cout << "Done processing network paths!\n";

    //Uses a map for POs because checking whether a map contains an item is way faster than searching a vector
    for (auto& entry : coord_to_pos)
    {
        tile_rep t = entry.second.first;
        //explicit POs will be created above so only implicit POs need to be constructed here
        if (t.function != "PO")
        {
            for (int i = 0; i < entry.second.second; ++i)
            {
                auto pov = create_vertex("PO", t.tile_name);
                auto v = coord_to_vertex.at(t.get_coords());
                ln->create_edge(v, pov);
            }
        }
    }
}

void json_parser::fill_gate_layout()
{
    //std::cout << "Filling gate layout\n";
    int i = 0;
    for (auto& t : tiles)
    {
        auto coords = t.get_coords();
        auto tile = (*fgl)(get<0>(coords), get<1>(coords), get<2>(coords));
        //check whether tile is a gate
        try
        {

            bool pi = false, po = false;
            auto vert = coord_to_vertex.at(coords);

            //check whether tile is PI
            try
            {
                coord_to_pis.at(coords);
                pi = true;
            } catch (...)
            {}
            //check whether tile is PO
            try
            {
                coord_to_pos.at(coords);
                po = true;
            } catch (...)
            {}
            fgl->assign_logic_vertex(tile, vert, pi, po);
            auto dirs = generate_tile_directions(t);

            fgl->assign_tile_inp_dir(tile, dirs.first);
            fgl->assign_tile_out_dir(tile, dirs.second);
        } catch (...)
        {
            /*std::cout << "Not a gate at coordinates: " << std::get<0>(t.get_coords()) << ", " << std::get<1>(t.get_coords())
                      << ", " << std::get<2>(t.get_coords()) << "\n";*/
        }

        //check whether tile is a wire
        try
        {
            auto edges = coord_to_edges.at(coords);
            layout::directions tile_in_dirs, tile_out_dirs;
            for (auto& edge : edges)
            {
                fgl->assign_logic_edge(tile, edge);
                auto in_out_dirs = edge_dir_map.at(std::make_pair(coords, edge));

                fgl->assign_wire_inp_dir(tile, edge, in_out_dirs.first);
                fgl->assign_wire_out_dir(tile, edge, in_out_dirs.second);
                //TODO: find out why directions are overwritten above and then remove this
                tile_in_dirs |= in_out_dirs.first;
                tile_out_dirs |= in_out_dirs.second;
            }
            fgl->assign_tile_inp_dir(tile, tile_in_dirs);
            fgl->assign_tile_out_dir(tile, tile_out_dirs);
        } catch (...)
        {
            /*std::cout << "Error accessing edge-dir-map with coords " << std::get<0>(t.get_coords()) << ", " << std::get<1>(t.get_coords())
                << ", " << std::get<2>(t.get_coords()) << "\n";*/
        }

        //assign clocking
        fgl->assign_clocking(tile, t.clock_zone);
        //This is fine to do for all tiles since setting it to 0 does nothing for the tile.
        fgl->assign_latch(tile, t.latch_offset);
        ++i;
    }
    //std::cout << "Gate-layout filled!\n";
}

logic_network::vertex json_parser::create_vertex(const std::string& function_name, std::string name)
{
    if (function_name == "NOT")
        return ln->create_not();
    else if (function_name == "AND")
        return ln->create_and();
    else if (function_name == "OR")
        return ln->create_or();
    else if (function_name == "XOR")
        return ln->create_xor();
    else if (function_name == "MAJ")
        return ln->create_maj();
    else if (function_name == "F1O2")
        return ln->create_f1o2();
    else if (function_name == "F1O3")
        return ln->create_f1o3();
    else if (function_name == "PO")
    {
        return ln->create_po(name);
    }
    else if (function_name == "PI")
    {
        return ln->create_pi(name);
    }
    else
    {
        throw std::invalid_argument("cannot create vertex for function " + function_name);
    }
}

std::pair<layout::directions, layout::directions> json_parser::generate_tile_directions(json_parser::tile_rep& gate)
{
    layout::directions in_dirs = layout::DIR_NONE, out_dirs = layout::DIR_NONE;

    for (auto& in_port : gate.in_ports)
    {
        in_dirs |= direction_for_port(in_port);
    }

    for (auto& out_port : gate.out_ports)
    {
        out_dirs |= direction_for_port(out_port);
    }

    return std::make_pair(in_dirs, out_dirs);
}

layout::directions json_parser::direction_for_port(json_parser::port& p)
{
    if (p.x == 0)
        return layout::DIR_W;
    else if (p.x == 4)
        return layout::DIR_E;
    else if (p.y == 0)
        return layout::DIR_N;
    else if (p.y == 4)
        return layout::DIR_S;
    else
    {
        throw std::invalid_argument("port is not at edge of tile");
    }
}
