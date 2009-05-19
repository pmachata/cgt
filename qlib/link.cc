/*
 * Copyright (C) 2009 Kamil Dudka <kdudka@redhat.com>
 *
 * This file is part of cgt (Call Graph Tools).
 *
 * cgt is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * cgt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cgt.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.hh"

#include "CallGraph.hh"
#include "Cgt.hh"
#include "Color.hh"
#include "Linker.hh"
#include "VertexFilter.hh"

#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
    Color::enable(true);

    typedef CgtGraphBuilder<CallGraph> TBuilder;
    Linker<CallGraph> linker;

    char **args = argv + 1;
    const char *inputFile;
    while ((inputFile = *args++)) {
        std::fstream str(inputFile, std::ios::in);
        if (!str) {
            std::cerr << Color(C_LIGHT_RED) << "can't open " << Color(C_NO_COLOR)
                << inputFile << std::endl;
            continue;
        }
#if DEBUG_SHOW_CG_FILE
        std::cerr << "--- "
            << Color(C_YELLOW) << inputFile << Color(C_NO_COLOR)
            << std::endl;
#endif

        // parse input
        typedef CallGraph TGraph;
        TGraph graph;

        CgtGraphBuilder<TGraph> builder(graph);
        CgtReader reader(&builder);
        reader.read(str, false);

        // link
        VertexFilter<CallGraph, DropUnusedDeclarations> fg(graph);
        linker.link(fg);

        str.close();
    }

    // write output
    CgtWriter writer(std::cout);
    write(linker.output(), writer);

    return 0;
}
