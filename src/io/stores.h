//
// Created by marcel on 16.07.18.
//

#ifndef FICTION_STORES_H
#define FICTION_STORES_H

#include "logic_network.h"
#include "fcn_gate_layout.h"
#include "fcn_cell_layout.h"
#include "svg_writer.h"
#include "fmt/format.h"
#include "fmt/ostream.h"
#include <alice/alice.hpp>

namespace alice
{
    /**
     * Logic networks.
     */
    ALICE_ADD_STORE(logic_network_ptr, "network", "w", "logic network", "logic networks")

    ALICE_PRINT_STORE(logic_network_ptr, os, ln)
    {
        ln->write_network(os);
    }

    ALICE_DESCRIBE_STORE(logic_network_ptr, ln)
    {
        return fmt::format("{} - I/O: {}/{}, #V: {}", ln->get_name(), ln->num_pis(),
                           ln->num_pos(), ln->vertex_count());
    }

    ALICE_PRINT_STORE_STATISTICS(logic_network_ptr, os, ln)
    {
        os << fmt::format("{} - I/O: {}/{}, #V: {}", ln->get_name(), ln->num_pis(),
                          ln->num_pos(), ln->vertex_count()) << std::endl;
    }

    ALICE_LOG_STORE_STATISTICS(logic_network_ptr, ln)
    {
        return nlohmann::json
        {
            {"name", ln->get_name()},
            {"inputs", ln->num_pis()},
            {"outputs", ln->num_pos()},
            {"vertices", ln->vertex_count()}
        };
    }


    /**
     * FCN gate layouts.
     */
    ALICE_ADD_STORE(fcn_gate_layout_ptr, "gate_layout", "g", "gate layout", "gate layouts")

    ALICE_PRINT_STORE(fcn_gate_layout_ptr, os, layout)
    {
        layout->write_layout(os);
    }

    ALICE_DESCRIBE_STORE(fcn_gate_layout_ptr, layout)
    {
        return fmt::format("{} - {} × {}", layout->get_name(), layout->x(), layout->y());
    }

    ALICE_PRINT_STORE_STATISTICS(fcn_gate_layout_ptr, os, layout)
    {
        auto [cp, tp] = layout->critical_path_length_and_throughput();
        os << fmt::format("{} - {} × {}, #G: {}, #W: {}, #C: {}, #L: {}, CP: {}, TP: 1/{}", layout->get_name(),
                          layout->x(), layout->y(), layout->gate_count(), layout->wire_count(),
                          layout->crossing_count(), layout->latch_count(), cp, tp) << std::endl;
    }

    ALICE_LOG_STORE_STATISTICS(fcn_gate_layout_ptr, layout)
    {
        auto area = layout->x() * layout->y();
        auto bb = layout->determine_bounding_box();
        auto [slow, fast] = layout->calculate_energy();
        auto gate_tiles = layout->gate_count();
        auto wire_tiles = layout->wire_count();
        auto crossings  = layout->crossing_count();
        auto [cp, tp] = layout->critical_path_length_and_throughput();

        return nlohmann::json
        {
            {"name", layout->get_name()},
            {"layout",
             {
                {"x-size", layout->x()},
                {"y-size", layout->y()},
                {"area", area}
             }
            },
            {"bounding box",
             {
                {"x-size", bb.x_size},
                {"y-size", bb.y_size},
                {"area", bb.area()}
             }
            },
            {"gate tiles", gate_tiles},
            {"wire tiles", wire_tiles},
            {"free tiles", area - (gate_tiles + wire_tiles - crossings)},  // free tiles in ground layer
            {"crossings", crossings},
            {"latches", layout->latch_count()},
            {"critical path", cp},
            {"throughput", "1/" + std::to_string(tp)},
            {"energy (meV, QCA)",
             {
                {"slow (25 GHz)", slow},
                {"fast (100 GHz)", fast}
             }
            }
        };
    }


    /**
     * FCN cell layouts.
     */
    ALICE_ADD_STORE(fcn_cell_layout_ptr, "cell_layout", "c", "cell layout", "cell layouts")

    ALICE_PRINT_STORE(fcn_cell_layout_ptr, os, layout)
    {
        layout->write_layout(os);
    }

    ALICE_DESCRIBE_STORE(fcn_cell_layout_ptr, layout)
    {
        return fmt::format("{} ({}) - {} × {}", layout->get_name(), layout->get_technology(), layout->x(), layout->y());
    }

    ALICE_PRINT_STORE_STATISTICS(fcn_cell_layout_ptr, os, layout)
    {
        os << fmt::format("{} ({}) - {} × {}, #Cells: {}", layout->get_name(), fcn::to_string(layout->get_technology()),
                          layout->x(), layout->y(), layout->cell_count()) << std::endl;
    }

    ALICE_LOG_STORE_STATISTICS(fcn_cell_layout_ptr, layout)
    {
        return nlohmann::json
        {
            {"name", layout->get_name()},
            {"technology", fcn::to_string(layout->get_technology())},
            {"layout",
             {
                {"x-size", layout->x()},
                {"y-size", layout->y()},
                {"area", layout->x() * layout->y()}
             }
            },
            {"cells", layout->cell_count()}
        };
    }

    template<>
    bool can_show<fcn_cell_layout_ptr>(std::string& extension, command& cmd)
    {
        extension = "svg";

        cmd.add_flag("--simple,-s", "Less detailed visualization "
                                    "(recommended for large layouts)")->group("cell_layout (-c)");

        return true;
    }

    template<>
    void show<fcn_cell_layout_ptr>(std::ostream& os, const fcn_cell_layout_ptr& element, const command& cmd)  // const & for pointer because alice says so...
    {
        try
        {
            os << svg::generate_svg_string(element, cmd.is_set("simple")) << std::endl;
        }
        catch (const std::invalid_argument& e)
        {
            cmd.env->out() << "[e] " << e.what() << std::endl;
        }
    }

}


#endif //FICTION_STORES_H
