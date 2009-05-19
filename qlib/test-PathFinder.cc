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
#include "BitmapIndex.hh"
#include "PathFinder.hh"
#include "test-lib.hh"

#undef NDEBUG
#include <cassert>

#include <boost/graph/adjacency_list.hpp>

template <typename TGraph>
void testFinder(const TGraph &graph) {
    using namespace boost;

    typedef BitmapFilter<TGraph>                        TFilter;
    typedef boost::graph_traits<TFilter>                Traits;
    typedef typename Traits::vertex_descriptor          TVertex;
    typedef BitmapIndexer<TGraph>                       TIndexer;
    typedef PathFinder<TGraph, VoidPathFinder>          TPathFinder;
    typedef typename TPathFinder::TPath                 TPath;
    typedef typename TPathFinder::TPathSet              TPathSet;
    typedef typename TPathFinder::TPathList             TPathList;

    const size_t nVert = num_vertices(graph);

    TIndexer indexer(graph);
    indexer.build();

    // for each source vertex
    for (TVertex src = 0; src < nVert; ++src) {
        // compute all paths beginning in source vertex (UniqEdgePath)
        PathFinder<TGraph, UniqEdgePath> srcFinderE(graph);
        srcFinderE.compute(src);

        // compute all paths beginning in source vertex (UniqVertexPath)
        PathFinder<TGraph, UniqVertexPath> srcFinderV(graph);
        srcFinderV.compute(src);

        // for each destination vertex
        for (TVertex dst = 0; dst < nVert; ++dst) {
            // obtain pruned graph
            TFilter filter(indexer, src, dst);

            // VoidPathFinder never finds anything
            PathFinder<TGraph, VoidPathFinder> voidFinder(graph);
            TPathList list;
            voidFinder.compute(src);
            voidFinder.paths(dst, list);
            assert(0 == list.size());

            // compute paths between source and destinations (UniqEdgePath)
            PathFinder<TFilter, UniqEdgePath> boundedFinderE(filter);
            boundedFinderE.compute(src);

            // compute paths between source and destinations (UniqVertexPath)
            PathFinder<TFilter, UniqVertexPath> boundedFinderV(filter);
            boundedFinderV.compute(src);

            // obtains computed paths between src and dest for all variants
            TPathSet fullE, fullV, boundedE, boundedV;
            srcFinderE.paths(dst, fullE);
            srcFinderV.paths(dst, fullV);
            boundedFinderE.paths(dst, boundedE);
            boundedFinderV.paths(dst, boundedV);

            // basic assertions
            assert(fullE.size() == boundedE.size());
            assert(fullV.size() == boundedV.size());
            assert(fullE.size() <= boundedE.size());
            assert(src != dst || 1 == fullV.size());

            // TODO
        }
    }
}

int main(int, char *[]) {
    const size_t nVert = 15;
    const size_t nBranch = 6;

    // test with SimpleTestGraph
    for (unsigned step = 0; step < 2; ++step) {
        SimpleTestGraph graph(nVert, nBranch, step);
        testFinder(graph);
    }

    // test with FullyConnectedGraph
    for (unsigned step = 0; step < 2*3; ++step) {
        const size_t nVert = 1 + (step >> 1);
        FullyConnectedGraph graph(nVert, step & 1);
        testFinder(graph);
    }

    return 0;
}
