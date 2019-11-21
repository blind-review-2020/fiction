//
// Created by marcel on 18.05.18.
//

#include "logic_network.h"


logic_network::logic_network() noexcept
        :
        bidirectional_graph(),
        strg{std::make_shared<logic_network_storage>("", OP_COUNT)}
{
    strg->zero = std::make_unique<vertex>(create_logic_vertex(operation::ZERO));
    strg->one  = std::make_unique<vertex>(create_logic_vertex(operation::ONE));
}

logic_network::logic_network(std::string&& name) noexcept
        :
        bidirectional_graph(),
        strg{std::make_shared<logic_network_storage>(std::move(name), OP_COUNT)}
{
    strg->zero = std::make_unique<vertex>(create_logic_vertex(operation::ZERO));
    strg->one  = std::make_unique<vertex>(create_logic_vertex(operation::ONE));
}

logic_network::logic_network(storage&& strg) noexcept
        :
        bidirectional_graph(),
        strg{std::move(strg)}
{}

logic_network::logic_network(const logic_network& ln) noexcept
        :
        bidirectional_graph(ln),
        strg{ln.strg}
{}

logic_network::logic_network(logic_network&& ln) noexcept
        :
        bidirectional_graph(ln),
        strg{std::move(ln.strg)}
{}

logic_network::num_vertices_t logic_network::vertex_count(const bool ios, const bool consts) const noexcept
{
    auto count = get_vertex_count();
    if (!ios)
        count -= (num_pis() + num_pos());
    if (!consts)
        count -= 2;

    return count;
}

std::size_t logic_network::size() const noexcept
{
    return vertex_count(true, true);
}

logic_network::num_edges_t logic_network::edge_count(const bool ios, const bool consts) const noexcept
{
    auto count = get_edge_count();
    if (!ios)
        count -= (num_pis() + num_pos());
    if (!consts)
        count -= const_count();

    return count;
}

logic_network::degree_t logic_network::out_degree(const vertex v, const bool ios, const bool consts) const noexcept
{
    auto out = out_edges(v, ios, consts);
    return static_cast<degree_t>(std::distance(out.begin(), out.end()));
}

logic_network::degree_t logic_network::in_degree(const vertex v, const bool ios, const bool consts) const noexcept
{
    auto in = in_edges(v, ios, consts);
    return static_cast<degree_t>(std::distance(in.begin(), in.end()));
}

logic_network::vertex logic_network::create_logic_vertex(const operation o) noexcept
{
    increment_op_counter(o);
    auto v = add_vertex(o);
    strg->v_map[v] = 0;
    return v;
}

void logic_network::remove_logic_vertex(const vertex v) noexcept
{
    decrement_op_counter(get_op(v));
    remove_vertex(v);
}

logic_network::vertex logic_network::create_pi(const std::string& name) noexcept
{
    auto v = create_logic_vertex(operation::PI);
    strg->pi_set.emplace(v);
    strg->io_port_map.insert(logic_network_storage::port_map::value_type(v, name));

    return v;
}

void logic_network::create_po(const vertex a, const std::string& name) noexcept
{
    auto v = create_logic_vertex(operation::PO);
    add_edge(a, v);
    strg->po_set.emplace(v);
    strg->io_port_map.insert(logic_network_storage::port_map::value_type(v, name));
}

logic_network::vertex logic_network::create_po(const std::string& name) noexcept
{
    auto v = add_vertex(operation::PO);
    strg->po_set.emplace(v);
    strg->io_port_map.insert(logic_network_storage::port_map::value_type(v, name));
    return v;
}

logic_network::vertex logic_network::get_constant(bool value) const noexcept
{
    return value ? *strg->one : *strg->zero;
}

logic_network::vertex logic_network::create_buf(const vertex a) noexcept
{
    auto v = create_buf();
    add_edge(a, v);

    return v;
}

logic_network::vertex logic_network::create_buf() noexcept
{
    return create_logic_vertex(operation::BUF);
}

logic_network::vertex logic_network::create_not(const vertex a) noexcept
{
    auto v = create_logic_vertex(operation::NOT);
    add_edge(a, v);

    return v;
}

logic_network::vertex logic_network::create_not() noexcept
{
    auto v = add_vertex(operation::NOT);

    return v;
}

logic_network::vertex logic_network::create_and(const vertex a, const vertex b) noexcept
{
    auto v = create_logic_vertex(operation::AND);
    add_edge(a, v);
    add_edge(b, v);

    return v;
}

logic_network::vertex logic_network::create_and() noexcept
{
    auto v = add_vertex(operation::AND);

    return v;
}

logic_network::vertex logic_network::create_or(const vertex a, const vertex b) noexcept
{
    auto v = create_logic_vertex(operation::OR);
    add_edge(a, v);
    add_edge(b, v);

    return v;
}


logic_network::vertex logic_network::create_or() noexcept
{
    auto v = add_vertex(operation::OR);
	return v;
}
    
logic_network::vertex logic_network::create_nary_or(const std::vector<vertex>& fs) noexcept
{
    if (fs.empty())
        return *strg->zero;

    auto v = create_logic_vertex(operation::OR);
    for (const auto& s : fs)
        add_edge(s, v);

    return v;
}

logic_network::vertex logic_network::create_xor(const vertex a, const vertex b) noexcept
{
    auto v = create_logic_vertex(operation::XOR);
    add_edge(a, v);
    add_edge(b, v);

    return v;
}

logic_network::vertex logic_network::create_xor() noexcept
{
    auto v = add_vertex(operation::XOR);

    return v;
}

logic_network::vertex logic_network::create_maj(const vertex a, const vertex b, const vertex c) noexcept
{
    auto v = create_logic_vertex(operation::MAJ);
    add_edge(a, v);
    add_edge(b, v);
    add_edge(c, v);

    return v;
}

logic_network::vertex logic_network::create_maj() noexcept
{
    auto v = add_vertex(operation::MAJ);

    return v;
}

logic_network::vertex logic_network::create_f1o2() noexcept
{
    auto v = add_vertex(operation::F1O2);

    return v;
}

logic_network::vertex logic_network::create_f1o3() noexcept
{
    auto v = add_vertex(operation::F1O3);

    return v;
}

logic_network::node logic_network::get_node(signal const& f) const noexcept
{
    return static_cast<node>(f);
}

logic_network::signal logic_network::clone_node(const base_type& other, const node& source, const std::vector<signal>& fanin) noexcept
{
    auto v = create_logic_vertex(other.get_op(source));
    for (const auto& s : fanin)
        add_edge(s, v);

    return static_cast<signal>(v);
}

void logic_network::clear_values() const
{
    for(auto&& i : strg->v_map)
        i.second = 0;
}

std::size_t logic_network::value(node const& n) const
{
    try {
        return strg->v_map.at(n);
    }
    catch (...)
    {
        strg->v_map[n] = 0;
        return 0;
    }

}

void logic_network::set_value(node const& n, std::size_t value) const
{
    strg->v_map[n] = value;
}

logic_network::vertex logic_network::create_balance_vertex(const edge& e) noexcept
{
    auto v = create_logic_vertex(operation::W);
    auto s = source(e), t = target(e);

    remove_edge(e);
    add_edge(s, v);
    add_edge(v, t);

    return v;
}

void logic_network::assign_op(const vertex v, const operation o) noexcept
{
    properties(v) = o;
}

operation logic_network::get_op(const vertex v) const noexcept
{
    return properties(v);
}

bool logic_network::is_pi(const vertex v) const noexcept
{
    return strg->pi_set.count(v) > 0u;
}

bool logic_network::pre_pi(const vertex v) const noexcept
{
    auto pre = inv_adjacent_vertices(v, true);
    return std::any_of(pre.begin(), pre.end(), [this](const vertex _v){return is_pi(_v);});
}

bool logic_network::is_po(const vertex v) const noexcept
{
    return strg->po_set.count(v) > 0u;
}

bool logic_network::post_po(const vertex v) const noexcept
{
    auto post = adjacent_vertices(v, true);
    return std::any_of(post.begin(), post.end(), [this](const vertex _v){return is_po(_v);});
}

bool logic_network::is_io(const vertex v) const noexcept
{
    return get_op(v) == operation::PI || get_op(v) == operation::PO;
}

bool logic_network::is_constant(const node& n) const noexcept
{
    return get_op(n) == operation::ONE || get_op(n) == operation::ZERO;
}

bool logic_network::is_complemented(const signal& f) const noexcept
{
    (void)f;
    return false;
}

std::size_t logic_network::operation_count(const operation o) const noexcept
{
    return strg->operation_counter[o];
}

bool logic_network::is_MIG() const noexcept
{
    for (auto&& oc : iter::range(strg->operation_counter.size()))
    {
        switch (oc)
        {
            case operation::MAJ:
            case operation::NOT:
            case operation::F1O2:
            case operation::F1O3:
            case operation::W:
            case operation::PI:
            case operation::PO:
            case operation::ONE:
            case operation::ZERO:
            case operation::BUF:
                continue;
            default:
            {
                if (strg->operation_counter[oc] != 0lu)
                    return false;
            }
        }
    }

    return true;
}

bool logic_network::is_AIG() const noexcept
{
    for (auto&& oc : iter::range(strg->operation_counter.size()))
    {
        switch (oc)
        {
            case operation::AND:
            case operation::NOT:
            case operation::F1O2:
            case operation::F1O3:
            case operation::W:
            case operation::PI:
            case operation::PO:
            case operation::ONE:
            case operation::ZERO:
            case operation::BUF:
                continue;
            default:
            {
                if (strg->operation_counter[oc] != 0lu)
                    return false;
            }
        }
    }

    return true;
}

bool logic_network::is_OIG() const noexcept
{
    for (auto&& oc : iter::range(strg->operation_counter.size()))
    {
        switch (oc)
        {
            case operation::OR:
            case operation::NOT:
            case operation::F1O2:
            case operation::F1O3:
            case operation::W:
            case operation::PI:
            case operation::PO:
            case operation::ONE:
            case operation::ZERO:
            case operation::BUF:
                continue;
            default:
            {
                if (strg->operation_counter[oc] != 0lu)
                    return false;
            }
        }
    }

    return true;
}

bool logic_network::is_AOIG() const noexcept
{
    for (auto&& oc : iter::range(strg->operation_counter.size()))
    {
        switch (oc)
        {
            case operation::AND:
            case operation::OR:
            case operation::NOT:
            case operation::F1O2:
            case operation::F1O3:
            case operation::W:
            case operation::PI:
            case operation::PO:
            case operation::ONE:
            case operation::ZERO:
            case operation::BUF:
                continue;
            default:
            {
                if (strg->operation_counter[oc] != 0lu)
                    return false;
            }
        }
    }

    return true;
}

bool logic_network::is_MAOIG() const noexcept
{
    for (auto&& oc : iter::range(strg->operation_counter.size()))
    {
        switch (oc)
        {
            case operation::MAJ:
            case operation::AND:
            case operation::OR:
            case operation::NOT:
            case operation::F1O2:
            case operation::F1O3:
            case operation::W:
            case operation::PI:
            case operation::PO:
            case operation::ONE:
            case operation::ZERO:
            case operation::BUF:
                continue;
            default:
            {
                if (strg->operation_counter[oc] != 0lu)
                    return false;
            }
        }
    }

    return true;
}

std::string logic_network::get_port_name(const vertex v) const noexcept
{
    try
    {
        return strg->io_port_map.left.at(v);
    }
    catch (const std::out_of_range&)
    {
        return "";
    }
}

std::string logic_network::get_name() const noexcept
{
    return strg->name;
}

std::vector<logic_network::edge_path> logic_network::get_all_paths(const vertex v, const bool ios, const bool consts) noexcept
{
    if (get_in_degree(v) == 0u)
        return std::vector<edge_path>{edge_path{}};

    std::vector<edge_path> paths{};
    for (auto&& e : in_edges(v, ios, consts))
    {
        auto ps = get_all_paths(source(e), ios, consts);
        for (auto& p : ps)
            p.push_back(e);

        paths.insert(paths.end(), ps.cbegin(), ps.cend());
    }

    return paths;
}

void logic_network::substitute() noexcept
{
    auto reduce_gate_inputs = []()
    {
        // TODO when lorina supports multi-input gates
    };

    auto decompose = [this]()
    {
        auto decompose_xor = [this](const vertex v_XOR)
        {
            auto v_FO_1  = create_logic_vertex(operation::F1O2);
            auto v_FO_2  = create_logic_vertex(operation::F1O2);
            auto v_AND_1 = create_logic_vertex(operation::AND);
            auto v_AND_2 = create_logic_vertex(operation::AND);
            auto v_NOT   = create_logic_vertex(operation::NOT);
            auto v_OR    = create_logic_vertex(operation::OR);

            add_edge(v_FO_1, v_AND_1);
            add_edge(v_FO_1, v_OR);
            add_edge(v_FO_2, v_AND_1);
            add_edge(v_FO_2, v_OR);
            add_edge(v_AND_1, v_NOT);
            add_edge(v_NOT, v_AND_2);
            add_edge(v_OR, v_AND_2);

            auto iaop = get_inv_adjacent_vertices(v_XOR);
            auto iao = iaop.begin();
            add_edge(*iao, v_FO_1);
            ++iao;
            add_edge(*iao, v_FO_2);

            for (auto&& ao : get_adjacent_vertices(v_XOR))
                add_edge(v_AND_2, ao);

            remove_logic_vertex(v_XOR);
        };

        auto lvs = vertices();
        auto is_composed_vertex = [this](const vertex _v){return get_op(_v) == operation::XOR /* || ... */;};

        auto comp_vertex = std::find_if(lvs.begin(), lvs.end(), is_composed_vertex);
        while (comp_vertex != lvs.end())
        {
            if (get_op(*comp_vertex) == operation::XOR)
                decompose_xor(*comp_vertex);
            // else if (get_op(*comp_vertex) == operation:: ...)

            comp_vertex = std::find_if(lvs.begin(), lvs.end(), is_composed_vertex);
        }
    };

    auto add_fan_outs = [this]()
    {
        for (auto&& v : get_vertices())
        {
            if (get_out_degree(v) > 1u && get_op(v) != operation::F1O2)
            {
                auto predecessor = v;
                std::vector<vertex> vv{};
                std::vector<edge> ve{};
                for (auto&& ae : out_edges(v, true, true))
                {
                    vv.push_back(target(ae));
                    ve.push_back(ae);
                }

                for (auto i : iter::range(vv.size()))
                {
                    if (i + 1 == vv.size())
                        add_edge(predecessor, vv[i]);
                    else
                    {
                        auto fan_out = create_logic_vertex(operation::F1O2);

                        add_edge(predecessor, fan_out);
                        add_edge(fan_out, vv[i]);

                        predecessor = fan_out;
                    }
                }
                for (auto& e : ve)
                    remove_edge(e);
            }
        }
    };

    reduce_gate_inputs();
    decompose();
    add_fan_outs();
}

void logic_network::write_network(std::ostream& os) noexcept
{
    std::stringstream graph{};

    graph << "digraph G {\n";

    for (auto&& v : vertices(true, true))
        graph << fmt::format("{} [label=<<B>{}</B><br/>{}>];\n", v, v, name_str(get_op(v)));

    for (auto&& e : edges(true, true))
        graph << fmt::format("{}->{};\n", source(e), target(e));

    graph << "}\n";

    os << graph.str() << std::endl;
}

void logic_network::increment_op_counter(const operation o) noexcept
{
    ++strg->operation_counter[o];
}

void logic_network::decrement_op_counter(const operation o) noexcept
{
    --strg->operation_counter[o];
}
