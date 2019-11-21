//
// Created on 01.05.2019.
//
#include "equivalence_checker.h"

equivalence_checker::equivalence_checker(fcn_gate_layout_ptr fgl, std::size_t faults)
        :
        two_layouts{false},
        fgl1{std::move(fgl)},
        faults{faults}
{}

equivalence_checker::equivalence_checker(fcn_gate_layout_ptr fgl1, fcn_gate_layout_ptr fgl2, std::size_t faults)
        :
        two_layouts{true},
        fgl1{std::move(fgl1)},
        fgl2{std::move(fgl2)},
        faults{faults}
{}

equivalence_checker::check_result equivalence_checker::check()
{
    logic_network_ptr ln1, ln2;
    if (two_layouts)
    {
        ln1 = extract_network(fgl1);
        ln2 = extract_network(fgl2);

        if (!ln1 || !ln2)
            throw std::invalid_argument("failure");
    }
    else
    {
        ln1 = extract_network(fgl1);

        if (faults != 0)
        {
            std::cout << "[i] inserting up to " << faults << " faults into the circuit" << std::endl;
            add_faults(*ln1, faults);
        }

        ln2 = fgl1->get_network();
    }

    auto miter = generate_miter<mockturtle::aig_network>(ln1, ln2);
    if (!miter)
    {
        std::cout << "Networks can't be compared by miter; number of PIs and/or POs is not equal! "
                  << "(" << ln1->num_pis() << ", " << ln1->num_pos() << ") and (" << ln2->num_pis() << ", "
                  << ln2->num_pos() << ")" << " pls set flag -i for ortho or exact and retry" << std::endl;
        nlohmann::json j;
        j["equivalent"] = false;
        j["runtime"] = 0.0f;
        j["miterGenerated"] = false;
        j["error"] = false;
        return check_result{false, j};
    }

    mockturtle::equivalence_checking_stats stats;
    bool eq, err;
    double runtime;
    nlohmann::json log;
    auto result = mockturtle::equivalence_checking(*miter, {}, &stats);
    if (result && *result)
    {
        nlohmann::json j;
        eq = true;
        runtime = mockturtle::to_seconds(stats.time_total);
        err = false;
    }
    else if (result && !*result)
    {
        eq = false;
        log["counter-example"] = stats.counter_example;
        runtime = mockturtle::to_seconds(stats.time_total);
        err = false;
    }
    else
    {
        eq = false;
        runtime = 0.0f;
        err = false;
    }
    log["equivalent"] = eq;
    log["miterGenerated"] = true;
    log["error"] = err;
    log["runtime"] = runtime;
    return check_result{eq, log};
}

logic_network_ptr equivalence_checker::extract(fcn_gate_layout_ptr fgl)
{
    using extraction_cache = std::unordered_map<fcn_gate_layout::tile, logic_network::vertex,
                                                boost::hash<fcn_gate_layout::tile>>;
    extraction_cache cache{};

    using operation_stack = std::stack<logic_network::vertex>;
    operation_stack o_stack{};

    auto ln = std::make_shared<logic_network>(fgl->get_name());

    auto push_and_cache = [&](const auto& _t, const auto _v) -> void
    {
        cache[_t] = _v;
        o_stack.push(_v);
    };

    const std::function<void(const fcn_gate_layout::tile_assignment&)>
            follow_data_flow = [&](const auto& ta) -> void
    {
        auto&[t, gw] = ta;

        if (auto g = std::get_if<logic_network::vertex>(&gw))
        {
            logic_network::vertex v;
            try
            {
                v = cache.at(t);
            }
            catch (const std::out_of_range&)
            {
                if (fgl->is_pi(t))
                {
                    if (fgl->get_op(t) == operation::PI)
                        v = ln->create_pi(fgl->get_inp_names(t)[0]);
                    else
                    {
                        v = ln->create_logic_vertex(fgl->get_op(t));
                        for (auto&& piv : fgl->get_network()
                                             ->inv_adjacent_vertices(*fgl->get_logic_vertex(t), true, true))
                        {
                            if (fgl->get_network()->is_pi(piv))
                            {
                                auto n_piv = ln->create_pi(fgl->get_network()->get_port_name(piv));
                                ln->create_edge(n_piv, v);
                            }
                        }
                    }
                }
                else
                    v = ln->create_logic_vertex(fgl->get_op(t));
            }
            // connect current vertex to lastly created one
            ln->create_edge(v, o_stack.top());
            // cache current operation
            push_and_cache(t, v);

            // follow predecessors
            for (auto&& idf : fgl->incoming_data_flow(t, gw))
                follow_data_flow(idf);

            o_stack.pop();
        }
        else if (auto w = std::get_if<logic_network::edge>(&gw))
        {
            for (auto&& idf : fgl->incoming_data_flow(t, {*w}))
                follow_data_flow(idf);
        }
    };

    for (auto&& po : fgl->get_pos())
    {
        //Handle implicit POs by checking operation on tile?
        if (fgl->get_op(po) == operation::PO)
            push_and_cache(po,
                           ln->create_po(fgl->get_out_names(po)[0]));  // this will only work for single output gates
        else
        {
            auto non_pov = ln->create_logic_vertex(fgl->get_op(po));
            for (auto&& tpo : fgl->get_network()->adjacent_vertices(*fgl->get_logic_vertex(po), true, true))
            {
                if (fgl->get_network()->is_po(tpo))
                {
                    auto tpov = ln->create_po(fgl->get_network()->get_port_name(tpo));
                    ln->create_edge(non_pov, tpov);
                }
            }
            push_and_cache(po, non_pov);
        }

        for (auto&& idf : fgl->incoming_data_flow(po, {*fgl->get_logic_vertex(po)}))
        {
            follow_data_flow(idf);
        }
    }

    return ln;
}

logic_network_ptr equivalence_checker::extract_network(fcn_gate_layout_ptr fgl)
{
    vmap.clear();
    auto ln = std::make_shared<logic_network>("");

    for (auto& g : fgl->get_pis())
    {
        try
        {
            auto pi = create_vertex(ln, fgl, g);
            //PI-vertices for implicit PIs need to be added manually.
            //TODO: find out how to handle implicit PIs
            if (fgl->get_op(g) != operation::PI)
            {
                auto v = ln->create_pi("");
                ln->create_edge(v, pi);
            }
            for (auto& p : fgl->outgoing_data_flow(g, fcn_gate_layout::gate_or_wire{*(fgl->get_logic_vertex(g))}))
                extract_network(fgl, ln, pi, p.first, p.second);
        }
        catch (std::exception& e)
        {
            throw e;
        }
    }

    return ln;
}

void
equivalence_checker::extract_network(fcn_gate_layout_ptr& fgl, logic_network_ptr& ln, logic_network::vertex& path_beginning,
                                     fcn_gate_layout::tile current, fcn_gate_layout::gate_or_wire gwc)
{

    bool is_gate = fgl->is_gate_tile(current);
    logic_network::vertex v;
    if (is_gate)
    {
        //look up current coordinates in vertex map; if they do not have an entry, create it and emplace it in the map.
        auto coords = std::make_tuple(current[0], current[1], current[2]);
        try
        {
            v = vmap.at(coords);
        }
        catch (...)
        {
            v = create_vertex(ln, fgl, current);
            vmap[coords] = v;
        }
        //since current tile has a gate, the current path is completed
        if (auto e = ln->get_edge(path_beginning, v); !e)
            ln->create_edge(path_beginning, v);
    }
    for (auto& g : fgl->outgoing_data_flow(current, gwc))
    {
        //if current tile was a gate, then new paths start from its vertex, otherwise continue with old path
        //because current tile is a wire
        extract_network(fgl, ln, is_gate ? v : path_beginning, g.first, g.second);
    }
}

void equivalence_checker::add_faults(logic_network& ln, std::size_t num_faults)
{
    auto num_v = ln.vertex_count(true, true);
    std::vector<std::size_t> indices;
    //Using this over rand() % num_v for reasons stated in
    //https://channel9.msdn.com/Events/GoingNative/2013/rand-Considered-Harmful
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<std::size_t> dist(0, num_v);

    for (std::size_t i = 0; i < num_faults; ++i)
    {
        auto rand = dist(mt);
        std::size_t tries = 0;
        //re-generate random numbers until number is found that has not been found yet
        //also check for operations that cannot be replaced
        auto op = ln.get_op(rand);
        while (((std::find(indices.begin(), indices.end(), rand)) != indices.end()
                || op == operation::PI || op == operation::PO || op == operation::ZERO || op == operation::ONE
                || op == operation::F1O2 || op == operation::F1O3 || op == operation::MAJ || op == operation::BUF)
               && tries < 5)
        {
            rand = dist(mt);
            op = ln.get_op(rand);
            ++tries;
        }
        indices.emplace_back(rand);

        //perform change in network
        switch (op)
        {
            case NOT:
                ln.assign_op(rand, operation::W);
                break;
            case W:
                ln.assign_op(rand, operation::NOT);
                break;
            case AND:
                ln.assign_op(rand, operation::OR);
                break;
            case OR:
                ln.assign_op(rand, operation::XOR);
                break;
            case XOR:
                ln.assign_op(rand, operation::AND);
                break;
            default:
                break;
        }
    }
}

logic_network::vertex
equivalence_checker::create_vertex(logic_network_ptr& ln, fcn_gate_layout_ptr& fgl, fcn_gate_layout::tile t)
{
    //Check that the given tile has an associated logic vertex.
    auto opt_vert = fgl->get_logic_vertex(t);
    std::stringstream ss;
    if (!opt_vert)
    {
        ss << "No logic vertex associated with tile at coordinates " << t[0] << ", "
           << t[1] << ", " << t[2];
        throw std::invalid_argument(ss.str());
    }

    //This does not work for implicit PI/PO since those are not on seperate tiles; tile function will not be PI/PO in
    //those cases.
    switch (fgl->get_op(t))
    {
        case operation::AND:
            return ln->create_and();
        case operation::OR:
            return ln->create_or();
        case operation::NOT:
            return ln->create_not();
        case operation::MAJ:
            return ln->create_maj();
        case operation::F1O2:
            return ln->create_f1o2();
        case operation::F1O3:
            return ln->create_f1o3();
        case operation::XOR:
            return ln->create_xor();
        case operation::PI:
            ++num_pis;
            return ln->create_pi(fgl->get_network()->get_port_name(*opt_vert));
        case operation::PO:
            return ln->create_po(fgl->get_network()->get_port_name(*opt_vert));
        default:
            ss << "Unsupported operation " << name_str(fgl->get_op(t)) << " of tile at coordinates " << t[0] << ", "
               << t[1] << ", " << t[2];
            throw std::invalid_argument(ss.str());
    }
}
