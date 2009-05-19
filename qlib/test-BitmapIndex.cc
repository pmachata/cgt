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
#include "test-lib.hh"

#undef NDEBUG
#include <cassert>
#include <vector>

#include <boost/graph/adjacency_list.hpp>

template <typename TIndexer>
class IndexChecker {
    public:
        typedef typename TIndexer::TVertex              TVertex;
        typedef typename std::vector<TBitmapIndex>      TIndexList;

    public:
        IndexChecker(TIndexer &indexer):
            indexer_(indexer),
            nVert_(num_vertices(indexer_.graph())),
            in_(nVert_),
            out_(nVert_)
        {
            for (TVertex vertex = 0; vertex < nVert_; ++vertex) {
                in_[vertex] = indexer.index(vertex, TIndexer::IN);
                out_[vertex] = indexer.index(vertex, TIndexer::OUT);
            }
        }

        void check() {
            for (TVertex vertex = 0; vertex < nVert_; ++vertex) {
                assert (in_[vertex] == indexer_.index(vertex, TIndexer::IN));
                assert (out_[vertex] == indexer_.index(vertex, TIndexer::OUT));
            }
        }
    private:
        TIndexer    &indexer_;
        size_t      nVert_;
        TIndexList  in_;
        TIndexList  out_;
};

void checkStability() {
    using namespace boost;

    typedef SimpleTestGraph                             TGraph;
    typedef boost::graph_traits<TGraph>                 Traits;
    typedef Traits::vertex_descriptor                   TVertex;
    typedef BitmapIndexer<TGraph>                       TIndexer;

    // check indexes stability
    const size_t nVert = 31;
    const size_t nBranch = 16;
    for (unsigned bi = 0; bi < nBranch << 1; ++bi) {
        TGraph graph(nVert, bi >> 1, bi & 1);
        TIndexer indexer(graph);

        // 1st request one-after-one
        IndexChecker<TIndexer> checker(indexer);

        // 2nd request one-after-one
        checker.check();

        // build already built
        indexer.build();
        checker.check();

        // clean the request one-after-one
        indexer.clear();
        checker.check();

        // clean, build and then check
        indexer.clear();
        indexer.build();
        checker.check();
    }
}

void checkOnList() {
    using namespace boost;

    typedef adjacency_list<vecS, vecS, bidirectionalS>  TGraph;
    typedef BitmapIndexer<TGraph>                       TIndexer;
    typedef BitmapFilter<TGraph>                        TFilter;
    typedef boost::graph_traits<TFilter>                Traits;
    typedef Traits::vertex_descriptor                   TVertex;

    const size_t nVert = 128;

    TGraph graph;
    add_vertex(graph);
    for (TVertex vertex = 1; vertex < nVert; ++vertex) {
        add_vertex(graph);
        add_edge(vertex - 1, vertex, graph);
    }

    TIndexer indexer(graph);

    // run twice (standalone built indexes and all-in-one built indexes)
    for(unsigned step = 0; step < 2; ++step) {
        for (TVertex vertex = 0; vertex < nVert; ++vertex) {
            // test pruning
            Traits::vertex_iterator vi, vi_end;
            TFilter both(indexer, vertex, vertex);
            tie(vi, vi_end) = vertices(both);
            assert(vi != vi_end);
            assert(++vi == vi_end);

            // test backward reachability index
            unsigned cnt = 0;
            TFilter in(graph, indexer.index(vertex, TIndexer::IN));
            for(tie(vi, vi_end) = vertices(in); vi != vi_end; ++vi) {
                assert(*vi < vertex);
                ++cnt;
            }
            assert(cnt == vertex);

            // test forward reachability index
            cnt = 0;
            TFilter out(graph, indexer.index(vertex, TIndexer::OUT));
            for(tie(vi, vi_end) = vertices(out); vi != vi_end; ++vi) {
                assert(vertex < *vi);
                ++cnt;
            }
            assert(cnt + vertex + 1 == nVert);
        }

        // try again with pre-built indexes
        indexer.clear();
        indexer.build();
    }
}

int main(int, char *[]) {
    checkStability();
    checkOnList();

    return 0;
}
