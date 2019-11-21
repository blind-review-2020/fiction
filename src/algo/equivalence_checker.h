//
// Created on 01.05.2019.
//

#ifndef FICTION_EQUIVALENCE_CHECKER_H
#define FICTION_EQUIVALENCE_CHECKER_H

#include "fcn_gate_layout.h"
#include "logic_network.h"
#include <itertools.hpp>
#include <nlohmann/json.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <boost/functional/hash.hpp>
#include <random>
#include <stack>

/**
 * Performs equality checking of logic networks and can extract them from gate layouts for that purpose.
 */
class equivalence_checker
{

public:
    /**
     * Struct representing the result of an equivalence check; keeps track of the check's result and generates a log in
     * JSON-format.
     */
    struct check_result
    {
        bool result;
        nlohmann::json json;
    };

    /**
     * Constructor for checking gate layout against original logic network.
     *
     * @param fgl Gate layout to check against original logic network
     */
    explicit equivalence_checker(fcn_gate_layout_ptr fgl, std::size_t faults = 0);

    /**
     * Constructor for checking equality of two gate layouts.
     *
     * @param fgl1 First gate layout of equality check
     * @param fgl2 Second gate layout of equality check
     */
    explicit equivalence_checker(fcn_gate_layout_ptr fgl1, fcn_gate_layout_ptr fgl2, std::size_t faults = 0);

    /**
     * Performs the equivalence check according to parameters passed to this class during
     * construction.
     *
     * @return Whether the networks to check are equivalent
     */
    check_result check();

    void add_faults(logic_network& ln, std::size_t num_faults);

private:
    template<class Ntk>
    Ntk convert(logic_network_ptr ln) const noexcept
    {
        using vertex_cache = std::unordered_map<logic_network::vertex, typename Ntk::signal,
                                                boost::hash<logic_network::vertex>>;
        vertex_cache cache{};

        Ntk ntk;

        auto create_node = [&](const auto _n) -> std::optional<typename Ntk::signal>
        {
            switch (auto op = ln->get_op(_n); op)
            {
                case operation::AND:
                {
                    auto iter = ln->inv_adjacent_vertices(_n, true).begin();
                    auto pre_1 = cache.at(*iter);
                    std::advance(iter, 1);
                    auto pre_2 = cache.at(*iter);
                    return ntk.create_and(pre_1, pre_2);
                }
                case operation::OR:
                {
                    auto iter = ln->inv_adjacent_vertices(_n, true).begin();
                    auto pre_1 = cache.at(*iter);
                    std::advance(iter, 1);
                    auto pre_2 = cache.at(*iter);
                    return ntk.create_or(pre_1, pre_2);
                }
                case operation::NOT:
                {
                    auto pre_1 = cache.at(*(ln->inv_adjacent_vertices(_n, true).begin()));
                    return ntk.create_not(pre_1);
                }
                case operation::MAJ:
                {
                    auto iter = ln->inv_adjacent_vertices(_n, true).begin();
                    auto pre_1 = cache.at(*iter);
                    std::advance(iter, 1);
                    auto pre_2 = cache.at(*iter);
                    std::advance(iter, 1);
                    auto pre_3 = cache.at(*iter);
                    return ntk.create_maj(pre_1, pre_2, pre_3);
                }
                case operation::PI:
                {
                    return ntk.create_pi(ln->get_port_name(_n));
                }
                case operation::PO:
                {
                    auto iter = ln->inv_adjacent_vertices(_n, true).begin();
                    auto pre_1 = cache.at(*iter);
                    ntk.create_po(pre_1, ln->get_port_name(_n));

                    return std::nullopt;
                }
                case operation::ONE:
                case operation::ZERO:
                {
                    return std::nullopt;
                }
                default:
                {
                    auto iter = ln->inv_adjacent_vertices(_n, true).begin();
                    auto pre_1 = cache.at(*iter);
                    return ntk.create_buf(pre_1);
                }
            }
        };

        // handle PIs first to make sure they are in the right order
        ln->foreach_pi([&](auto pi)
                       {
                           if (auto _n = create_node(pi); _n)
                               cache[pi] = *_n;
                       });

        for (auto&& n : iter::reversed(ln->topological_sort()))
        {
            if (ln->is_pi(n))
                continue;

            if (auto _n = create_node(n); _n)
                cache[n] = *_n;
        }

        return ntk;
    }

    template<class Ntk>
    std::optional<Ntk> generate_miter(logic_network_ptr ln1, logic_network_ptr ln2) const noexcept
    {
        auto ntk1 = convert<Ntk>(ln1), ntk2 = convert<Ntk>(ln2);
        return mockturtle::miter<Ntk>(ntk1, ntk2);
    }

    logic_network_ptr extract(fcn_gate_layout_ptr fgl);
    /**
     * Maps vertices to an arbitrary number for marking them in the depth-first-search.
     */
    using vert_map = std::unordered_map<logic_network::vertex, std::size_t>;

    logic_network_ptr extract_network2(fcn_gate_layout_ptr& fgl, logic_network_ptr old_n);

    void extract_network2(fcn_gate_layout_ptr fgl, logic_network_ptr old_n, logic_network_ptr new_n,
                          fcn_gate_layout::tile current, fcn_gate_layout::gate_or_wire current_gow);

    /**
     * Extracts a logic network from the given gate layout by traversing the layout, starting
     * at the PIs
     *
     * @param fgl The gate layout from whose logic network is to be extracted
     * @return The logic network of the given gate layout
     */
    logic_network_ptr extract_network(fcn_gate_layout_ptr fgl);

    /**
     * Fills the given logic network by reading paths between gates and adding them to it.
     *
     * @param fgl The gate layout whose logic network is to be extracted
     * @param ln The logic network to fill with information
     * @param path_beginning The beginning of the current path as a logic vertex
     * @param current The current tile
     * @param gwc The gate_or_wire object for current
     */
    void extract_network(fcn_gate_layout_ptr& fgl, logic_network_ptr& ln, logic_network::vertex& path_beginning,
                         fcn_gate_layout::tile current, fcn_gate_layout::gate_or_wire gwc);

    /**
     * Creates a vertex in the given logic network based on the operation of the passed tile.
     * Fails for operations W, ZERO, ONE, BUF and NONE.
     *
     * @param ln The network in which the vertex is to be created
     * @param fgl The gate layout in which t can be found
     * @param t The tile for which a logic vertex is to created
     * @return The created vertex
     */
    logic_network::vertex create_vertex(logic_network_ptr& ln, fcn_gate_layout_ptr& fgl, fcn_gate_layout::tile t);

    /**
     * Stores whether equality check is between two layouts or a layout and its original network.
     */
    bool two_layouts;
    /**
     * First gate layout. Always contains a valid entry.
     */
    fcn_gate_layout_ptr fgl1;
    /**
     * Second gate layout. Only contains a valid entry when comparing two layouts.
     */
    fcn_gate_layout_ptr fgl2;
    /**
     * Maximum number of faults to randomly insert into the circuit prior to equivalence checking.
     */
    std::size_t faults;
    /**
     * Maps tile coordinates to their corresponding logic network vertices.
     */
    std::unordered_map<std::tuple<int, int, int>, logic_network::vertex, boost::hash<std::tuple<int, int, int>>> vmap{};

    std::vector<std::tuple<int, int, int>> pi_vector;

    std::size_t num_pis = 0;
};

#endif //FICTION_EQUIVALENCE_CHECKER_H
