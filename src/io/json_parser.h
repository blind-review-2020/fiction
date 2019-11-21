//
// Created by gregor on 10/17/18.
//

#ifndef FICTION_JSON_PARSER_H
#define FICTION_JSON_PARSER_H

#include "fcn_gate_layout.h"
#include "logic_network.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <climits>
#include <fstream>
#include <boost/array.hpp>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>

using json = nlohmann::json;

class json_parser
{
public:
    /**
     * Struct for handling port information. Holds coordinates and affiliation with other ports inside the same tile
     * for multi-wire support.
     */
    struct port
    {
        /**
         * Coodinates inside the tile and affiliation with other ports.
         */
        std::size_t x, y, z, net;
        /**
         * Generates a string representation of the port.
         *
         * @return String representing the port.
         */
        std::string to_string();
        /**
         * Checks equality of ports. Ports are equal when their coordinates and net-affiliation are equal.
         * If one or both net-affiliations is 0, then net-affiliation is ignored for equality-check.
         *
         * @param rhs Port to check equality with
         * @return Whether this and rhs are equal
         */
        friend bool operator==(const json_parser::port& lhs, const json_parser::port& rhs)
        {
            bool equal = lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
            if (lhs.net == 0 || rhs.net == 0)
                return equal;
            return equal && (lhs.net == rhs.net);
        }

        /**
         * Calculates hash-value for unordered set/map insertion. Net-affiliation is ignored for hashing.
         * This makes it possible to look up ports in a map that have an unknown affiliation.
         *
         * @param p The port to be hashed
         * @return The hash for the given port
         */
        friend std::size_t hash_value(const port& p)
        {
            std::size_t seed = p.x ^p.y ^p.z;
            boost::hash_combine(seed, p.x);
            boost::hash_combine(seed, p.y);
            boost::hash_combine(seed, p.z);

            return seed;
        }
    };
    /**
     * Represents a single tile and contains the information a tile description contains in the Interchange Format.
     */
    struct tile_rep
    {
        /**
         * The tile's coordinates.
         */
        int x = 0, y = 0, z = 0;
        /**
         * The tile's clocking zone.
         */
        unsigned int clock_zone = 0;
        /**
         * The tile's latch offset, is 0 for tiles with no latch.
         */
        unsigned int latch_offset = 0;
        /**
         * Strings representing the tile's function and its name, if it is a primary input or output.
         */
        std::string function = "", tile_name = "";
        /**
         * The tile's ports.
         */
        std::vector<json_parser::port> in_ports, out_ports;
        /**
         * Prints the tile description to cout.
         */
        void print();
        /**
         * Gives the tile's coordinates as a 3-tupel, in order x, y, z.
         *
         * @return The tile's coordinates
         */
        std::tuple<int, int, int> get_coords();
        /**
         * Returns a string representation of the tile's coordinates.
         *
         * @return a string representation of the tile's coordinates.
         */
        std::string print_coords();
        /**
         * Overloads equality-operator based on uniqueness of coordinates.
         */
        bool operator==(const json_parser::tile_rep& rhs);
        /**
         * The hash of tile representations is based on their coordinates since these are uniquely identifying.
         *
         * @param t The tile representation to hash
         * @return The hash for the given tile representation
         */
        friend std::size_t hash_value(const tile_rep& t)
        {
            std::size_t seed = t.x ^t.y ^t.z;
            boost::hash_combine(seed, t.x);
            boost::hash_combine(seed, t.y);
            boost::hash_combine(seed, t.z);

            return seed;
        }
    };

    using port_mapping = std::unordered_map<port, std::vector<port>, boost::hash<port>>;

    using port_port_map = std::unordered_map<port, port, boost::hash<port>>;

    using coords = std::tuple<int, int, int>;

    using coord_tile_map = std::unordered_map<coords, tile_rep, boost::hash<coords>>;

    using vertex_map = std::unordered_map<coords, logic_network::vertex,
            boost::hash<coords>>;

    using edge_map = std::unordered_map<coords, std::vector<logic_network::edge>,
            boost::hash<coords>>;
    /**
     * Constructor of parser objects. Uses smart pointers to fill logic_network and fcn_gate_layout with the information
     * from the given file.
     *
     * @param filename The file to read
     * @param lnp The logic_network to fill
     * @param fgl The gate layout to fill
     */
    explicit json_parser(const std::string& filename, logic_network_ptr lnp, fcn_gate_layout_ptr& fgl);
    /**
     * Starts the parsing process by calling subroutines. Parsing does not return anything, results are inserted into
     * the given logic_network and fcn_gate_layout objects.
     */
    void parse();

private:

    void parse_qca_one(const json& coord_port_mapping, std::string& clocking);

    /**
     * This function processes one explicit out-port in a specific tile by finding the path from it to the next
     * gate (or multiple gates in case of implicit fanouts). The thusly created paths are added to network_paths.
     *
     * @param current The current tile
     * @param current_out_port The current out-port within tile
     * @param current_network_path The path from the original gate including current
     * @return The number of Tiles added to tiles_to_process in the original function
     */
    void process_out_port(tile_rep& current, port& current_out_port,
                          std::vector<tile_rep>& current_network_path);

    /**
     * Generates a mapping of in-ports to lists of out-ports. Ports are represented as objects of type std::pair<int, int>
     *  where the contents of the pair are the x- and y-coordinates respectively.
     *
     * @param t The Tile for which the port-map is to be created
     * @return The mapping of in-ports to out-ports
     */
    port_mapping create_port_map(const tile_rep& t);

    /**
     * Calculates the coordinates of a not necessarily existing in-port of an adjacent tile
     * and returns the tuple (x, y, z, d) where x, y and z coordinates of the
     * in-port and d is the direction in which the adjacent tile lays.
     *
     * @param out_port The outport whose adjacent port is to be found
     * @return tuple with coordinates of the adjacent port and the direction of the adjacent tile
     */
    std::tuple<std::size_t, std::size_t, std::size_t, layout::directions> get_adjacent_port(port& out_port);
    /**
     * Uses the information stored in network_paths to fill the logic network that was created in the constructor.
     */
    void fill_logic_network();
    /**
     * Uses the created logic network and information about tiles to create a gate layout.
     */
    void fill_gate_layout();
    /**
     * Creates a new vertex in the logic network representing the function given as the parameter. The newly created
     * vertex is not connected to any other vertices.
     *
     * @param function_name Name of the new vertex's function
     * @param name Name of the PI/PO, standard is empty for non-IO-functions
     * @return A new vertex in the logic network for the given function
     */
    logic_network::vertex create_vertex(const std::string& function_name, std::string name = "");
    /**
     *
     * @param gate
     * @return
     */
    std::pair<layout::directions, layout::directions> generate_tile_directions(tile_rep& gate);
    /**
     * Returns the direction of the tile-edge containing the given port.
     *
     * @param p The port whose direction is requested
     * @return The direction the port is facing
     */
    layout::directions direction_for_port(port& p);
    /**
     * Holds the logic network that is filled by the parser.
     */
    logic_network_ptr ln;
    /**
     * Holds the gate layout that is filled by the parser.
     */
    fcn_gate_layout_ptr& fgl;
    /**
     * Holds the contents of the read file in JSON-format.
     */
    json to_parse;
    /**
     * Holds whether the number of clocking zones in the read file.
     */
    std::size_t num_clocks;
    /**
     *  Hold the corner points of the read circuit as coordinates.
     */
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    /**
     * Vector of all tiles that exist in the gate_layout.
     */
    std::vector<tile_rep> tiles;
    /**
     * Vector of tiles that have been processed for generating the logic_network.
     */
    std::vector<tile_rep> processed_tiles;
    /**
     * This maps tuples of coordinates to their respective Tiles.
     */
    std::unordered_map<coords, tile_rep, boost::hash<coords>> coord_to_tile;
    /**
     * This contains a mapping of coordinates to PI-tiles and the number of PIs on each tile.
     */
    std::unordered_map<coords, std::pair<tile_rep, int>, boost::hash<coords>> coord_to_pis{};
    /**
     * This contains a mapping of coordinates to PO-tiles and the number of POs on each tile.
     */
    std::unordered_map<coords, std::pair<tile_rep, int>, boost::hash<coords>> coord_to_pos{};
    //std::unordered_map<logic_network::edge, layout::directions, boost::hash<logic_network::edge>> edge_dir_map{};
    /**
     * Maps tiles via coordinates and a network path to in- and out-directions for an edge on a tile. The number allows
     * identifying the logic network-edge later.
     */
    std::unordered_map<std::pair<coords, unsigned int>, std::pair<layout::directions, layout::directions>,
            boost::hash<std::pair<coords, unsigned int>>> path_dir_map{};
    /**
     * Maps logic edges to information flow direction within a specific tile. First element of value pair contains
     * in-direction(s) and second element contains out-direction(s).
     */
    std::unordered_map<std::pair<coords, logic_network::edge>, std::pair<layout::directions, layout::directions>,
            boost::hash<std::pair<coords, logic_network::edge>>> edge_dir_map{};
    /**
     * Maps coordinates of tiles to their logic vertex for later creating gate layouts.
     */
    vertex_map coord_to_vertex{};
    /**
     * Maps coordinates of tiles to the logic edges running through them for creating gate layouts.
     */
    edge_map coord_to_edges{};
    /**
     * This represents connections between different gates and has the form
     * "Gate" - ("Wire" -)* "Gate"
     * All entries contain the path from one gate to another with optional
     * wiring in between.
     */
    std::vector<std::vector<tile_rep>> network_paths;
};

#endif //FICTION_JSON_PARSER_H
