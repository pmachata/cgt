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
#include "VertexFilter.hh"
#include "test-lib.hh"

#undef NDEBUG
#include <cassert>

#include <boost/graph/adjacency_list.hpp>

template <typename TGraph>
class EqualInOutDegree {
    public:
        typedef boost::graph_traits<TGraph>             Traits;
        typedef typename Traits::vertex_descriptor      TVertex;
    public:
        EqualInOutDegree(): graph_(0) { }
        EqualInOutDegree(const TGraph &graph):
            graph_(&graph)
        {
        }

        bool operator()(const TVertex &vertex) const {
            return boost::in_degree(vertex, *graph_)
                == boost::out_degree(vertex, *graph_);
        }
    private:
        const TGraph *graph_;
};

int main(int, char *[]) {
    using namespace boost;

    typedef SimpleTestGraph                             TGraph;
    typedef VertexFilter<TGraph, EqualInOutDegree>      TFilter;
    typedef graph_traits<TFilter>                       Traits;
    typedef Traits::vertex_descriptor                   TVertex;

    // create original graph
    SimpleTestGraph graph(15, 7);

    // create filter
    TFilter filter(graph);
    assert(num_vertices(graph) == num_vertices(filter));

    // check filter
    unsigned cnt = 0;
    Traits::vertex_iterator vi, vi_end;
    for(tie(vi, vi_end) = vertices(filter); vi != vi_end; ++vi) {
        TVertex v = *vi;
        assert(v != 0);
        assert(v != 7);
        assert(v != 14);
        ++cnt;
    }
    assert(cnt == 12);

    return 0;
}
