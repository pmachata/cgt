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

#ifndef TEST_LIB_H
#define TEST_LIB_H

#include "config.h"
#include <boost/graph/adjacency_list.hpp>


class SimpleTestGraph: public
    boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS>
{
    public:
        typedef boost::graph_traits<SimpleTestGraph>    Traits;
        typedef Traits::vertex_descriptor               TVertex;

    public:
        SimpleTestGraph(size_t nVert, size_t nBranch, bool reversed = false) {
            using namespace boost;

            // vertices
            for (TVertex v = 0; v < nVert; ++v)
                add_vertex(*this);

            // "forward" edges
            for (TVertex v = 0; v < nBranch; ++v) {
                TVertex i = v << 1;
                TVertex left = i + 1;
                TVertex right = i + 2;
                if (nVert <= right)
                    continue;

                addEdge(v, left, reversed);
                addEdge(v, right, reversed);
            }

            // "backward" edges
            for (TVertex v = 0; v < nBranch; ++v) {
                TVertex i = v << 1;
                TVertex left = i + 1;
                TVertex right = i + 2;
                if (nVert <= right)
                    continue;

                const size_t C = nVert - 1;
                addEdge(C - v, C - left, reversed);
                addEdge(C - v, C - right, reversed);
            }
        }
    private:
        void addEdge(TVertex a, TVertex b, bool reversed) {
            add_edge(reversed ? b:a, reversed ? a:b, *this);
        }
};

class FullyConnectedGraph: public
    boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS>
{
    public:
        typedef boost::graph_traits<SimpleTestGraph>    Traits;
        typedef Traits::vertex_descriptor               TVertex;

    public:
        FullyConnectedGraph(size_t nVert, bool shortLoops = true) {
            using namespace boost;

            // vertices
            for (TVertex v = 0; v < nVert; ++v)
                add_vertex(*this);

            // edges
            for (TVertex src = 0; src < nVert; ++src)
                for (TVertex dst = 0; dst < nVert; ++dst)
                    if (src != dst || shortLoops)
                        add_edge(src, dst, *this);
        }
};

#endif // TEST_LIB_H
