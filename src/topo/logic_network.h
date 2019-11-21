//
// Created by marcel on 18.05.18.
//

#ifndef FICTION_LOGIC_NETWORK_H
#define FICTION_LOGIC_NETWORK_H

#include "bidirectional_graph.h"
#include "operations.h"
#include "fmt/format.h"
#include "fmt/ostream.h"
#include <mockturtle/networks/detail/foreach.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <itertools.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/constructors.hpp>

/**
 * Special graph structure with annotated vertices representing a logic_network. Each vertex holds a function from
 * operations.h. More functions can be implemented there easily. Note that this class provides topological information
 * only even though it could be extended pretty easily to also enables logic simulation.
 *
 * To iterate over all vertices of a logic_network n, use
 *  for (auto&& v : n.vertices())
 *  {
 *      ...
 *  }
 *
 * There are more functions returning ranges of vertices. They work analogously.
 *
 * logic_networks have "hidden" PI/PO and CONST0/CONST1 vertices which are excluded from normal iteration. To iterate
 * over them too, use the respective flags in the call of vertices() and other functions.
 *
 * This class is heavily used by fcn_gate_layout to model gate and wire tiles.
 */
class logic_network : protected bidirectional_graph<operation, boost::no_property, boost::no_property>
{
    friend class json_parser;
public:
    /**
     * vertex_t of logic_networks are called vertices for convenience.
     */
    using vertex = vertex_t;
    using vertex_index = std::size_t;
    /**
     * edge_t of logic_networks are called edges for convenience.
     */
    using edge = edge_t;
    using edge_index = std::size_t;
    /**
     * A sequence of edges can be used as a path.
     */
    using edge_path = edges_t;
    /**
     * mockturtle requirements for the network API
     */
    using base_type = logic_network;
    using node = vertex;
    using signal = vertex;
    /**
     * All the data types used to define a logic_network.
     */
    struct logic_network_storage
    {
        /**
         * Default constructor.
         */
        logic_network_storage() = default;
        /**
         * Standard constructor used by logic_network with minimal initialization.
         *
         * @param name Name of the logic_network.
         * @param op_count Number of different logic operations possible.
         */
        logic_network_storage(std::string&& name, std::size_t op_count)
                :
                name{std::move(name)},
                operation_counter(op_count, 0ul)
        {}
        /**
         * Path name from which the logic_network was created.
         */
        const std::string name;
        /**
         * Alias for a hash set that holds vertices to represent PI/PO ports.
         */
        using primary_set = std::unordered_set<vertex, boost::hash<vertex>>;
        /**
         * Stores vertices that are marked as PIs/POs. Helper functions for access save memory.
         */
        primary_set pi_set{}, po_set{};
        /**
         * Alias for a bidirectional hash map of vertices to names specifying port nodes in the network.
         */
        using port_map = boost::bimap<boost::bimaps::unordered_set_of<vertex, boost::hash<vertex>>,
                boost::bimaps::unordered_set_of<std::string, boost::hash<std::string>>>;
        /**
         * Stores I/O port names.
         */
        port_map io_port_map{};
        /**
         * Alias for a hashmap that maps vertices to their respective values.
         */
        using value_map = std::unordered_map<vertex, std::size_t>;
        /**
         * Stores values assigned to vertices.
         */
        value_map v_map{};
        /**
         * Counts the operations the network is composed of.
         */
        std::vector<std::size_t> operation_counter;
        /**
         * Pointers to the constant nodes 0 and 1.
         */
        std::unique_ptr<vertex> zero, one;
    };
    /**
     * mockturtle requirement for the network API
     */
    using storage = std::shared_ptr<logic_network_storage>;
    /**
     * Minimum fan-in for every node is 0.
     */
    static constexpr uint32_t min_fanin_size = 0;
    /**
     * Maximum fan-in for every node is 3.
     */
    static constexpr uint32_t max_fanin_size = 3;

    /**
     * Standard constructor. Creates an empty logic network.
     */
    explicit logic_network() noexcept;
    /**
     * Standard constructor. Creates a named empty logic network.
     *
     * @param name Path name from which the logic network was created.
     */
    explicit logic_network(std::string&& name) noexcept;
    /**
     * Standard constructor. Creates a logic network from given storage data.
     *
     * @param strg Internal content of a logic network.
     */
    explicit logic_network(storage&& strg) noexcept;
    /**
     * Copy constructor.
     */
    logic_network(const logic_network& ln) noexcept;
    /**
     * Move constructor.
     */
    logic_network(logic_network&& ln) noexcept;
    /**
     * Default destructor.
     */
    ~logic_network() override = default;
    /**
     * Assignment operator is not available.
     */
    logic_network& operator=(const logic_network& rhs) noexcept = delete;
    /**
     * Move assignment operator is not available.
     */
    logic_network& operator=(logic_network&& rhs) noexcept = delete;
    /**
     * Make topological sort visible.
     */
    using bidirectional_graph::topological_sort;
    /**
     * Function alias for add_edge using perfect forwarding and the name create_edge to fit naming in logic_network.
     *
     * @tparam ARGS Stack of argument types.
     * @param args Stack of arguments.
     * @return get_target(args).
     */
    template <typename... ARGS>
    auto create_edge(ARGS&&... args) noexcept
    {
        return add_edge(std::forward<ARGS>(args)...);
    }
    /**
     * Function alias for get_target using perfect forwarding and the name target to fit naming in logic_network.
     *
     * @tparam ARGS Stack of argument types.
     * @param args Stack of arguments.
     * @return get_target(args).
     */
    template <typename... ARGS>
    auto target(ARGS&&... args) const noexcept
    {
        return get_target(std::forward<ARGS>(args)...);
    }
    /**
     * Function alias for get_source using perfect forwarding and the name source to fit naming in
     * logic_network.
     *
     * @tparam ARGS Stack of argument types.
     * @param args Stack of arguments.
     * @return get_source(args).
     */
    template <typename... ARGS>
    auto source(ARGS&&... args) const noexcept
    {
        return get_source(std::forward<ARGS>(args)...);
    }
    /**
     * Function alias for get_index using perfect forwarding and the name index to fit naming in logic_network.
     *
     * @tparam ARGS Stack of argument types.
     * @param args Stack of arguments.
     * @return get_index(args).
     */
    template <typename... ARGS>
    auto index(ARGS&&... args) const noexcept
    {
        return get_index(std::forward<ARGS>(args)...);
    }
    /**
     * Returns the index of the given node in the network.
     *
     * @param n The node whose index is requested.
     * @return The node's index.
     */
    std::size_t node_to_index(node const& n) const noexcept
    {
        return index(n);
    }
    /**
     * Function alias for get_edge using perfect forwarding and the name get_edge to fit naming in
     * logic_network.
     *
     * @tparam ARGS Stack of argument types.
     * @param args Stack of arguments.
     * @return get_edge(args).
     */
    template <typename... ARGS>
    auto get_edge(ARGS&&... args) const noexcept
    {
        return bidirectional_graph::get_edge(std::forward<ARGS>(args)...);
    }
    /**
     * Adds a vertex with the given operation type to the network.
     *
     * @param o Operation type.
     * @return Created vertex.
     */
    vertex create_logic_vertex(const operation o) noexcept;
    /**
     * Removes the given vertex from the network.
     *
     * @param v Vertex to remove.
     */
    void remove_logic_vertex(const vertex v) noexcept;
    /**
     * Creates a primary input vertex in the network.
     *
     * @param name Name associated with the input port.
     * @return Created vertex.
     */
    vertex create_pi(const std::string& name = "") noexcept;
    /**
     * Calls fn on every PI in the network.
     *
     * @tparam Fn Callable type of fn.
     * @param fn Callable to invoke on every PI.
     */
    template <typename Fn>
    void foreach_pi(Fn&& fn) const
    {
        std::vector<vertex> pis;
        pis.insert(pis.begin(), strg->pi_set.begin(), strg->pi_set.end());
        std::sort(pis.begin(), pis.end(), [this](vertex a, vertex b){return get_port_name(a) < get_port_name(b);});
        mockturtle::detail::foreach_element(pis.begin(), pis.end(), fn);
    }
    /**
     * Creates a primary output vertex in the network.
     *
     * @param a Input signal to the newly created PO.
     * @param name Name associated with the output port.
     */
    void create_po(const vertex a, const std::string& name = "") noexcept;
    /**

     * Creates a primary output vertex in the network without an input signal.
     *
     * @return Created vertex.
     */
    vertex create_po(const std::string& name = "") noexcept;

    /**
     * Returns a constant vertex of the network with respect to value.
     * True = 1, False = 0.
     *
     * @param value Flag to indicate polarity of the constant vertex.
     * @return ONE iff value == true, ZERO iff value == false.
     */
    vertex get_constant(bool value) const noexcept;
    /**
     * Creates a buffer element in the network with the given vertex as input signal.
     *
     * @param a Input signal to the newly created buffer.
     * @return The created buffer vertex.
     */
    vertex create_buf(const vertex a) noexcept;
    /**
     * Creates a buffer element in the network without an input signal.
     *
     * @return The created buffer vertex.
     */
    vertex create_buf() noexcept;
    /**
     * Creates a NOT gate in the network with the given vertex as input signal.
     *
     * @param a Input signal to the newly created inverter.
     * @return The created inverter vertex.
     */
    vertex create_not(const vertex a) noexcept;
    /**
     * Creates a NOT gate in the network without input signals.
     *
     * @return The created inverter vertex.
     */
    vertex create_not() noexcept;
    /**
     * Creates a AND gate in the network with the given vertices as input signals.
     *
     * @param a Input signal 1 to the newly created conjunction.
     * @param b Input signal 2 to the newly created conjunction.
     * @return The created conjunction vertex.
     */
    vertex create_and(const vertex a, const vertex b) noexcept;
    /**
     * Creates an AND gate in the network without input signals.
     *
     * @return The created conjunction vertex.
     */
    vertex create_and() noexcept;
    /**
     * Creates an OR gate in the network with the given vertices as input signals.
     *
     * @param a Input signal 1 to the newly created disjunction.
     * @param b Input signal 2 to the newly created disjunction.
     * @return The created disjunction vertex.
     */
    vertex create_or(const vertex a, const vertex b) noexcept;
    /**
     * Creates an OR gate in the network without input signals.
     *
     * @return The created disjunction vertex.
     */
    vertex create_or() noexcept;
    /**
     * Creates an n-ary OR gate in the network with the given vertices as input signals.
     * In case fs is empty, the zero vertex is returned.
     *
     * @param fs Input signals to the newly created OR gate.
     * @return The created n-ary disjunction vertex.
     */
    vertex create_nary_or(const std::vector<vertex>& fs) noexcept;
    /**
     * Creates an XOR gate in the network with the given vertices as input signals.
     *
     * @param a Input signal 1 to the newly created exclusive disjunction.
     * @param b Input signal 2 to the newly created exclusive disjunction.
     * @return The created exclusive disjunction vertex.
     */
    vertex create_xor(const vertex a, const vertex b) noexcept;
    /**
     * Creates a XOR gate in the network without input signals.
     *
     * @return The created exclusive disjunction vertex.
     */
    vertex create_xor() noexcept;
    /**
     * Creates a MAJ gate in the network with the given vertices as input signals.
     *
     * @param a Input signal 1 to the newly created majority.
     * @param b Input signal 2 to the newly created majority.
     * @param c Input signal 3 to the newly created majority.
     * @return The created majority vertex.
     */
    vertex create_maj(const vertex a, const vertex b, const vertex c) noexcept;
    /**
     * Creates a MAJ gate in the network without input signals.
     *
     * @return The created majority vertex.
     */
    vertex create_maj() noexcept;
    /**
     * Creates a F1O2 vertex in the network without signals.
     *
     * @return The created 1-2-fanout vertex.
     */
    vertex create_f1o2() noexcept;
    /**
     * Creates a F1O3 vertex in the network without signals.
     *
     * @return The created 1-3-fanout vertex.
     */
    vertex create_f1o3() noexcept;
    /**
     * Returns whether the given node represents an AND-gate.
     *
     * @param n The node to check.
     * @return Whether the node represents an AND-gate.
     */
    bool is_and(node const& n) const
    {
        return get_op(n) == operation::AND;
    }
    /**
     * Returns whether the given node represents an OR-gate.
     *
     * @param n The node to check.
     * @return Whether the node represents an OR-gate.
     */
    bool is_or(node const& n) const
    {
        return get_op(n) == operation::OR;
    }
    /**
     * Returns whether the given node represents a NOT-gate.
     *
     * @param n The node to check.
     * @return Whether the node represents a NOT-gate.
     */
    bool is_not(node const& n) const
    {
        return get_op(n) == operation::NOT;
    }
    /**
     * Returns whether the given node represents a MAJ-gate.
     *
     * @param n The node to check.
     * @return Whether the node represents a MAJ-gate.
     */
    bool is_maj(node const& n) const
    {
        return get_op(n) == operation::MAJ;
    }
    /**
     * Returns whether the given node represents a XOR-gate.
     *
     * @param n The node to check.
     * @return Whether the node represents a XOR-gate.
     */
    bool is_xor(node const& n) const
    {
        return get_op(n) == operation::XOR;
    }
    /**
     * Returns the node a signal is pointing to. This is only needed for mockturtle-intern purposes. In this
     * logic_network implementation, nodes and signals are identical.
     *
     * @param f Signal whose node is desired.
     * @return Node of signal f.
     */
    node get_node(signal const& f) const noexcept;
    /**
     * Clones a node from another logic_network other. The node source is a node in the network other, but the signals
     * in fanin refer to signals in the current logic_network.
     *
     * @param other Other logic_network.
     * @param source Node in other.
     * @param children Fan-in signals from the current network.
     * @return New signal representing node in current network.
     */
    signal clone_node(const base_type& other, const node& source, const std::vector<signal>& fanin) noexcept;
    /**
     * Resets all values to 0.
     */
    void clear_values() const;
    /**
     * Returns value associated with a node.
     *
     * @param n The node whose value is requested.
     * @return The node's value.
     */
    std::size_t value(node const& n) const;
    /**
     * Sets the value of a node to the given value.
     *
     * @param n The node whose value is to be set.
     * @param value The value to associate with the node.
     */
    void set_value(node const& n, std::size_t value) const;
    /**
     * Replaces the given edge e (s->e->t) with a balance vertex b of operation W and two new edges (s->e'->b->e''->t).
     *
     * @param e Edge to substitute.
     * @return Inserted balance vertex.
     */
    vertex create_balance_vertex(const edge& e) noexcept;
    /**
     * Returns a range of vertices in the network. The range can be parameterized to specify whether I/Os or constants
     * should be included.
     *
     * @param ios Flag to indicate that I/Os should be included in the range.
     * @param consts Flag to indicate that constants should be included in the range.
     * @return Range of vertices that might include I/Os or constants depending on parameters.
     */
    auto vertices(const bool ios = false, const bool consts = false) const noexcept
    {
        auto logic_selector = [this, ios = ios, consts = consts](const vertex _v) -> bool
        {
            // do neither return consts nor I/Os
            if (!ios && !consts)
                return is_io(_v) || is_constant(_v);
            // do not return consts
            if (ios && !consts)
                return is_constant(_v);
            // do not return I/Os
            if (!ios && consts)
                return is_io(_v);

            // else return all vertices
            return false;
        };

        return get_vertices() | iter::filterfalse(logic_selector);
    }
    /**
     * Calls fn on every PO in the network.
     *
     * @tparam Fn Callable type of fn.
     * @param fn Callable to invoke on every PO.
     */
    template<typename Fn>
    void foreach_po(Fn&& fn) const {
        //mockturtle::detail::foreach_element(strg->po_set.begin(), strg->po_set.end(), fn);
        std::vector<vertex> pos;
        for (auto &&v : vertices(true, true))
        {
            if (get_op(v) == operation::PO)
                pos.emplace_back(v);
        }
        std::sort(pos.begin(), pos.end(), [this](vertex a, vertex b){return get_port_name(a) < get_port_name(b);});
        mockturtle::detail::foreach_element(pos.begin(), pos.end(), fn);
    }
    /**
     * Returns a range of vertices adjacent to the given vertex v. The range can be parameterized to specify whether
     * I/Os or constants should be included.
     *
     * @param v Vertex whose adjacent vertices are desired.
     * @param ios Flag to indicate that I/Os should be included in the range.
     * @param consts Flag to indicate that constants should be included in the range.
     * @return Range of vertices adjacent to v that might include I/Os or constants depending on parameters.
     */
    auto adjacent_vertices(const vertex v, const bool ios = false, const bool consts = false) const noexcept
    {
        auto logic_selector = [this, ios = ios, consts = consts](const vertex _v) -> bool
        {
            // do neither return consts nor I/Os
            if (!ios && !consts)
                return is_io(_v) || is_constant(_v);
            // do not return consts
            if (ios && !consts)
                return is_constant(_v);
            // do not return I/Os
            if (!ios && consts)
                return is_io(_v);

            // else return all vertices
            return false;
        };

        return get_adjacent_vertices(v) | iter::filterfalse(logic_selector);
    }
    /**
     * Returns a range of vertices inversely adjacent to the given vertex v. The range can be parameterized to specify
     * whether I/Os or constants should be included.
     *
     * @param v Vertex whose inversely adjacent vertices are desired.
     * @param ios Flag to indicate that I/Os should be included in the range.
     * @param consts Flag to indicate that constants should be included in the range.
     * @return Range of vertices inversely adjacent to v that might include I/Os or constants depending on parameters.
     */
    auto inv_adjacent_vertices(const vertex v, const bool ios = false, const bool consts = false) const noexcept
    {
        auto logic_selector = [this, ios = ios, consts = consts](const vertex _v) -> bool
        {
            // do neither return consts nor I/Os
            if (!ios && !consts)
                return is_io(_v) || is_constant(_v);
            // do not return consts
            if (ios && !consts)
                return is_constant(_v);
            // do not return I/Os
            if (!ios && consts)
                return is_io(_v);

            // else return all vertices
            return false;
        };

        return get_inv_adjacent_vertices(v) | iter::filterfalse(logic_selector);
    }
    /**
     * Calls fn on every fanin of a node.
     *
     * @tparam Fn Callable type of fn
     * @param n Node whose fanins are considered
     * @param fn Callable to invoke on every node (including I/Os and constants).
     */
    template <typename Fn>
    void foreach_fanin(node const& n, Fn&& fn) const
    {
        auto vs = inv_adjacent_vertices(n, true, true);
        mockturtle::detail::foreach_element(vs.begin(), vs.end(), std::forward<Fn>(fn));
        // vermutlich geht std::forward für alle foreach-Methoden ebenfalls
    }
    /**
     * Calls fn on every gate in the network.
     *
     * @tparam Fn Callable type of fn
     * @param n Node whose fanins are considered
     * @param fn Callable to invoke on every node (including I/Os and constants).
     */
    template <typename Fn>
    void foreach_gate(Fn&& fn) const
    {
        mockturtle::detail::foreach_element(vertices().begin(), vertices().end(), std::forward<Fn>(fn));
    }
    /**
     * Returns the fanin of the given node.
     *
     * @param n Node whose number of inputs is requested.
     * @return Number of inputs of given node.
     */
    std::size_t fanin_size(node const& n) const
    {
       return in_degree(n);
    }
    /**
     * Returns a truthtable for the given vertex. Note that this is only defined for AND, OR, NOT, XOR and MAJ operations.
     *
     * @param n The vertex whose truthtable is requested.
     * @return Truthtable for operation that n represents.
     */
    kitty::dynamic_truth_table node_function(node const& n) const
    {
        kitty::dynamic_truth_table tt;
        switch(get_op(n))
        {
            case AND:
                tt = kitty::dynamic_truth_table(2);
                kitty::create_from_binary_string<kitty::dynamic_truth_table>(tt, "1000");
                return tt;
            case OR:
                tt = kitty::dynamic_truth_table(2);
                kitty::create_from_binary_string<kitty::dynamic_truth_table>(tt, "1110");
                return tt;
            case NOT:
                tt = kitty::dynamic_truth_table(1);
                kitty::create_from_binary_string<kitty::dynamic_truth_table>(tt, "01");
                return tt;
            case MAJ:
                tt = kitty::dynamic_truth_table(3);
                kitty::create_from_binary_string<kitty::dynamic_truth_table>(tt, "11101000");
                return tt;
            case XOR:
                tt = kitty::dynamic_truth_table(2);
                kitty::create_from_binary_string<kitty::dynamic_truth_table>(tt, "0110");
                return tt;
            case F1O2:
            case F1O3:
            case W:
            case PI:
            case PO:
                tt = kitty::dynamic_truth_table(1);
                kitty::create_from_binary_string<kitty::dynamic_truth_table>(tt, "10");
                return tt;
            default:
                throw std::invalid_argument("Can't create truthtable for operation " + name_str(get_op(n)));
        }
    }
    /**
     * Returns the number of vertices in the network. The function can be parameterized to specify whether I/Os or
     * constants should be included.
     *
     * Calculation is performed as #V [- #I - #O] [- 2] (brackets denote options not parentheses), where the 2 represents
     * constants ONE and ZERO. Therefore, no other constant vertices should be added to the network.
     *
     * @param ios Flag to indicate that I/Os should be counted as vertices.
     * @param consts Flag to indicate that consts should be counted as vertices.
     * @return Number of vertices with respect to the flags.
     */
    num_vertices_t vertex_count(const bool ios = false, const bool consts = false) const noexcept;
    /**
     * Returns the number of vertices in the network, including constants, PIs and dead nodes.
     *
     * @return Total number of vertices in the network.
     */
    std::size_t size() const noexcept;
    /**
     * Calls fn on every node in the network.
     *
     * @tparam Fn Callable type of fn.
     * @param fn Callable to invoke on every node (including I/Os and constants).
     */
    template<typename Fn>
    void foreach_node(Fn&& fn) const
    {
        mockturtle::detail::foreach_element(vertices(true, true).begin(), vertices(true, true).end(), fn);
    }
    /**
     * Returns a range of edges in the network. The range can be parameterized to specify whether edges leading towards
     * or from I/Os or constants should be included.
     *
     * @param ios Flag to indicate that edges leading towards or from I/Os should be included in the range.
     * @param consts Flag to indicate that edges leading toward or from constants should be included in the range.
     * @return Range of edges that might include such leading towards or from I/Os or constants depending on parameters.
     */
    auto edges(const bool ios = false, const bool consts = false) const noexcept
    {
        auto logic_selector = [this, ios = ios, consts = consts](const edge& _e) -> bool
        {
            auto _s = source(_e), _t = target(_e);
            // do neither return consts nor I/Os
            if (!ios && !consts)
                return is_io(_s) || is_io(_t) || is_constant(_s) || is_constant(_t);
            // do not return consts
            if (ios && !consts)
                return is_constant(_s) || is_constant(_t);
            // do not return I/Os
            if (!ios && consts)
                return is_io(_s) || is_io(_t);

            // else return all vertices
            return false;
        };

        return get_edges() | iter::filterfalse(logic_selector);
    }
    /**
     * Returns a range of edges outgoing from the given vertex v. The range can be parameterized to specify whether
     * edges leading towards I/Os or constants should be included.
     *
     * @param v Vertex whose outgoing edges are desired.
     * @param ios Flag to indicate that edges leading towards I/Os should be included in the range.
     * @param consts Flag to indicate that edges leading towards constants should be included in the range.
     * @return Range of edges outgoing from v that might include edges towards I/Os or constants depending on parameters.
     */
    auto out_edges(const vertex v, const bool ios = false, const bool consts = false) const noexcept
    {
        auto logic_selector = [this, ios = ios, consts = consts](const edge& _e) -> bool
        {
            auto _t = target(_e);
            // do neither return consts nor I/Os
            if (!ios && !consts)
                return is_io(_t) || is_constant(_t);
            // do not return consts
            if (ios && !consts)
                return is_constant(_t);
            // do not return I/Os
            if (!ios && consts)
                return is_io(_t);

            // else return all vertices
            return false;
        };

        return get_out_edges(v) | iter::filterfalse(logic_selector);
    }
    /**
     * Returns a range of edges incoming to the given vertex v. The range can be parameterized to specify whether
     * edges coming from I/Os or constants should be included.
     *
     * @param v Vertex whose incoming edges are desired.
     * @param ios Flag to indicate that edges coming from I/Os should be included in the range.
     * @param consts Flag to indicate that edges coming from constants should be included in the range.
     * @return Range of edges incoming to v that might include edges from I/Os or constants depending on parameters.
     */
    auto in_edges(const vertex v, const bool ios = false, const bool consts = false) const noexcept
    {
        auto logic_selector = [this, ios = ios, consts = consts](const edge& _e) -> bool
        {
            auto _s = source(_e);
            // do neither return consts nor I/Os
            if (!ios && !consts)
                return is_io(_s) || is_constant(_s);
            // do not return consts
            if (ios && !consts)
                return is_constant(_s);
            // do not return I/Os
            if (!ios && consts)
                return is_io(_s);

            // else return all vertices
            return false;
        };

        return get_in_edges(v) | iter::filterfalse(logic_selector);
    }
    /**
     * Returns the number of edges in the network. The function can be parameterized to specify whether edges connecting
     * I/Os or constants should be included.
     *
     * Calculation is performed as #E [- #I - #O] [- #1 - #0] (brackets denote options no parentheses).
     *
     * @param ios Flag to indicate that I/Os should be counted as edges.
     * @param consts Flag to indicate that consts should be counted as edges.
     * @return Number of edges with respect to the flags.
     */
    num_edges_t edge_count(const bool ios = false, const bool consts = false) const noexcept;
    /**
     * Returns the number of outgoing edges of the given vertex v. The function can be parameterized to specify whether
     * edges to I/Os or constants should be included.
     *
     * @param v Vertex whose out degree is desired.
     * @param ios Flag to indicate that edges to I/Os should be counted.
     * @param consts Flag to indicate that edges to constants should be counted.
     * @return Number of v's outgoing edges with respect to the flags.
     */
    degree_t out_degree(const vertex v, const bool ios = false, const bool consts = false) const noexcept;
    /**
     * Returns the number of incoming edges of the given vertex v. The function can be parameterized to specify whether
     * edges from I/Os or constants should be included.
     *
     * @param v Vertex whose in degree is desired.
     * @param ios Flag to indicate that edges from I/Os should be counted.
     * @param consts Flag to indicate that edges from constants should be counted.
     * @return Number of v's incoming edges with respect to the flags.
     */
    degree_t in_degree(const vertex v, const bool ios = false, const bool consts = false) const noexcept;
    /**
     * Assigns vertex v with operation o.
     *
     * @param v vertex which should be associated with operation o.
     * @param o Operation which should be assigned to vertex v.
     */
    void assign_op(const vertex v, const operation o) noexcept;
    /**
     * Returns the operation assigned to vertex v.
     *
     * @param v vertex whose assigned operation is desired.
     * @return Operation associated with v.
     */
    operation get_op(const vertex v) const noexcept;
    /**
     * Returns whether or not given vertex v is a PI port as by set entry.
     *
     * @param v vertex whose assigned PI port should be checked.
     * @return true iff v is a PI port.
     */
    bool is_pi(const vertex v) const noexcept;
    /**
     * Returns whether or not given vertex v has predecessors which are PIs as by set entry.
     *
     * @param v vertex whose predecessors should be checked for PI status.
     * @return True if v has PI predecessors.
     */
    bool pre_pi(const vertex v) const noexcept;
    /**
     * Returns the number of primary input flagged vertices in the network.
     *
     * @return Number of PIs as an unsigned integral type.
     */
    auto num_pis() const noexcept
    {
        return strg->pi_set.size();
    }
    /**
     * Returns a range of all vertices flagged as primary input in the network.
     *
     * @return range_t of all primary input vertices.
     */
    auto get_pis() const noexcept
    {
        return range_t<logic_network_storage::primary_set::const_iterator>
        {std::make_pair(strg->pi_set.begin(), strg->pi_set.end())};
    }
    /**
     * Returns whether or not given vertex v is a PO port as by set entry.
     *
     * @param v vertex whose assigned PO port should be checked.
     * @return true iff v is a PO port.
     */
    bool is_po(const vertex v) const noexcept;
    /**
     * Returns whether or not given vertex v has successors which are POs as by set entry.
     *
     * @param v vertex whose successors should be checked for PO status.
     * @return True if v has PO successors.
     */
    bool post_po(const vertex v) const noexcept;
    /**
     * Returns the number of primary output flagged vertices in the network.
     *
     * @return Number of POs as an unsigned integral type.
     */
    auto num_pos() const noexcept
    {
        return strg->po_set.size();
    }
    /**
     * Returns a range of all vertices flagged as primary output in the network.
     *
     * @return range_t of all primary output vertices.
     */
    auto get_pos() const noexcept
    {
        return range_t<logic_network_storage::primary_set::const_iterator>
        {std::make_pair(strg->po_set.begin(), strg->po_set.end())};
    }
    /**
     * Returns true iff given vertex v is an explicit PI or PO node.
     *
     * @param v Vertex to test for I/O-ness.
     * @return True iff v is I/O.
     */
    bool is_io(const vertex v) const noexcept;
    /**
     * Returns true iff given node n is an explicit ONE or ZERO node.
     *
     * @param n Node to test for const-ness.
     * @return True iff v is const.
     */
    bool is_constant(const node& n) const noexcept;
    /**
     * Returns true iff given signal f is complemented. This is only needed for mockturtle-intern purposes. In this
     * logic_network implementation, signals cannot be complemented.
     *
     * @param f Signal to test for complemented attribute.
     * @return False because signals cannot be complemented.
     */
    bool is_complemented(const signal& f) const noexcept;
    /**
     * Returns the number of constant signals in the network. Note that the number of const vertices always should be 2.
     * They can have multiple edges leading to other vertices though.
     *
     * @return Number of edges coming from constant vertices.
     */
    auto const_count() const noexcept
    {
        return out_degree(*strg->zero, true, true) + out_degree(*strg->one, true, true);
    }
    /**
     * Returns the number of operations of given type in the network.
     *
     * @param o Type of operation whose count is desired.
     * @return Number of operations of type o in the network.
     */
    std::size_t operation_count(const operation o) const noexcept;
    /**
     * Checks whether the network only contains MAJ and NOT logic vertices. As non-logic vertices,
     * fan-outs, buffers, and auxiliary wire vertices are accepted as well.
     *
     * @return True iff network is an Majority-Inverter-Graph.
     */
    bool is_MIG() const noexcept;
    /**
     * Checks whether the network only contains AND and NOT logic vertices. As non-logic vertices,
     * fan-outs, buffers, and auxiliary wire vertices are accepted as well.
     *
     * @return True iff network is an AND-Inverter-Graph.
     */
    bool is_AIG() const noexcept;
    /**
     * Checks whether the network only contains OR and NOT logic vertices. As non-logic vertices,
     * fan-outs, buffers, and auxiliary wire vertices are accepted as well.
     *
     * @return True iff network is an OR-Inverter-Graph.
     */
    bool is_OIG() const noexcept;
    /**
     * Checks whether the network only contains AND, OR, and NOT logic vertices. As non-logic vertices,
     * fan-outs, buffers, and auxiliary wire vertices are accepted as well.
     *
     * @return True iff network is an AND-OR-Inverter-Graph.
     */
    bool is_AOIG() const noexcept;
    /**
     * Checks whether the network only contains MAJ, AND, OR, and NOT logic vertices. As non-logic vertices,
     * fan-outs, buffers, and auxiliary wire vertices are accepted as well.
     *
     * @return True iff network is an Majority-AND-OR-Inverter-Graph.
     */
    bool is_MAOIG() const noexcept;
    /**
     * Returns the stored I/O port name of given vertex v. If no port name was stored before, an empty string is
     * returned.
     *
     * @param v Vertex whose port name is desired.
     * @return Port name of v if there is one, "" otherwise.
     */
    std::string get_port_name(const vertex v) const noexcept;
    /**
     * Returns the stored file path name.
     *
     * @return The stored file path name.
     */
    std::string get_name() const noexcept;
    /**
     * Returns a vector of all possible paths to reach the given vertex within the logic_network. Function can be
     * parameterized to define whether I/Os or constants should be considered as well.
     *
     * Each vertex without predecessors is considered a terminal and a path is defined as a sequence of edges.
     *
     * @param v Vertex to which all paths should lead.
     * @param ios Flag to indicate that I/Os should be considered as path elements.
     * @param consts Flag to indicate that constants should be considered as path elements.
     * @return A vector of all possible edge paths leading from terminals to v.
     */
    std::vector<edge_path> get_all_paths(const vertex v, const bool ios = false, const bool consts = false) noexcept;
    /**
     * Substitutes all connections that exceed allowed incoming degree for certain operations, i.e. 1-NOT, 1-BUF, 2-AND,
     * 2-OR, 2-XOR, 3-MAJ, and decomposes XOR into AND/OR/NOT vertices.
     */
    void substitute() noexcept;
    /**
     * Re-assigns indices to the vertices and edges. Only needed if index() functions are to be called and only once
     * until the graph gets structurally manipulated the next time. Note that by calling this function, indices might
     * change!
     */
    void update_index_maps() noexcept;
    /**
     * Writes a Graphviz (https://www.graphviz.org/) dot representation of the logic_network to the given ostream.
     * Incorporates the logic function names.
     *
     * @param os Channel into which the graphviz representation should be written.
     */
    void write_network(std::ostream& os = std::cout) noexcept;
private:
    /**
     * Container to encapsulate all private data. See logic_network_storage above for detailed information.
     */
    storage strg;
    /**
     * Increases the operation counter of the given operation by 1.
     *
     * @param o Operation type to increment.
     */
    void increment_op_counter(const operation o) noexcept;
    /**
     * Decrements the operation counter of the given operation by 1.
     *
     * @param o Operation type to decrement.
     */
    void decrement_op_counter(const operation o) noexcept;
};

using logic_network_ptr = std::shared_ptr<logic_network>;

#endif //FICTION_LOGIC_NETWORK_H
