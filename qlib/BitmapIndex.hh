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

#ifndef BITMAP_INDEX_H
#define BITMAP_INDEX_H

#include "config.hh"
#include "VertexFilter.hh"

#ifndef DEBUG_BITMAP_INDEX
#   define DEBUG_BITMAP_INDEX 0
#endif

#include <cassert>
#include <stack>
#include <vector>

#include <boost/dynamic_bitset.hpp>
#include <boost/graph/adjacency_list.hpp>

#if DEBUG_BITMAP_INDEX
#   include <iostream>
#endif

/**
 * Type of bitmap index. This type is used by BitmapIndexer to store (and
 * return) a reachability relation. It is also used by BitmapVertexPredicate
 * and BitmapFilter to define which vertices belongs to the factor. The bitset
 * must be always the same size as the count of vertices (of related graph).
 */
typedef boost::dynamic_bitset <> TBitmapIndex;

/**
 * BitmapIndexer class template is responsible for computing a reachability
 * relation for vertices in the graph. The reachability relation is stored
 * in the bit set TBitmapIndex. If a bit on position 'n' in the bit set is true,
 * it means the vertex 'n' is reachable.
 *
 * It works with oriented graphs and therefore the reachability can be tested
 * in two directions. So there are two types of bitmap indexes. The type OUT
 * means "all vertices reachable from the given vertex". The type IN (reverse
 * index) means "all vertices from which the given vertex is reachable". Both
 * types of indexes are maintained together by BitmapIndexer.
 *
 * Maximal memory complexity of this class is asymptomatically O(N^2) where
 * N is the number of vertices. It allocates N^2 bits for both indexes.
 *
 * @param TGraph Type of graph, boost::adjacency_list is supported.
 * @attention BitmapIndexer can't be used for sparse and/or filtered graphs. It
 * means vertex indexes must be continuous. If you need to index filtered graph,
 * please clone/pack it and then index it.
 */
template <typename TGraph>
class BitmapIndexer {
    public:
        typedef typename boost::graph_traits<TGraph>    Traits;
        typedef typename Traits::vertex_descriptor      TVertex;
        typedef TBitmapIndex                            TIndex;
        typedef enum { IN, OUT }                        EDirection;

    public:
        /**
         * The initialization is a cheap operation. There is nothing computed
         * and nothing huge allocated. All time-consuming and memory-exhausting
         * operations are delayed.
         * @param graph Reference to graph ought to be indexed. The graph object
         * is not cloned by this class and must stay valid and unchanged till
         * BitmapIndexer destruction.
         */
        BitmapIndexer(const TGraph &graph):
            graph_(graph)
        {
        }

        /**
         * This method is used by BitmapFilter class to reduce count of
         * arguments because there is always only one graph which BitmapIndexer
         * belongs to.
         * @return Return a reference to indexed graph.
         */
        const TGraph& graph() const {
            return graph_;
        }

        /**
         * Return reachability relation of given vertex expressed as bitmap
         * index. This operation is cheap if index is already computed.
         * Otherwise it has complexity of Depth Search (not DFS as not only
         * First vertex has to be found). Once computed index is held for next
         * request (if not cleared explicitly) and/or for faster all-in-one
         * indexes computation.
         * @param vertex Vertex to obtain index for.
         * @param dir Direction of index (IN or OUT).
         */
        const TIndex& index(TVertex vertex, EDirection dir) {
            // check range
            size_t nVert = boost::num_vertices(graph_);
            assert(vertex < nVert);

            // initialize storage if needed
            TIndexList &list = storage_[dir];
            if (list.size() != nVert)
                list.resize(nVert);

            buildIfNeeded(vertex, dir);
            return list[vertex];
        }

        /**
         * Build all indexes of both types. See void build(EDirection dir)
         * documentation.
         */
        void build() {
            BitmapIndexer::build(IN);
            BitmapIndexer::build(OUT);
        }

        /**
         * Build all indexes of specified type. This operation is used to save
         * computation time. It is much cheaper to compute all indexes at once
         * but more expensive than computing only one index. It is safe to call
         * this method multiple times and/or mix it with calls of the index()
         * method. It can be cheaper if some (or even all) indexes are already
         * computed. Complexity of the first run is asymptomatically O(N^2 * D)
         * where N is the count of vertices and D is the maximal length of path
         * in the graph, complexity of second run O(N^2).
         * @param dir Type of indexes to compute (IN our OUT).
         */
        void build(EDirection dir) {
            // initialize storage if needed
            size_t nVert = boost::num_vertices(graph_);
            TIndexList &list = storage_[dir];
            if (list.size() != nVert)
                list.resize(nVert);

            // allocate all indexes
            for (TVertex v = 0; v < nVert; ++v)
                buildIfNeeded(v, dir);
        }

        /**
         * Free all indexes.
         */
        void clear() {
            BitmapIndexer::clear(IN);
            BitmapIndexer::clear(OUT);
        }

        /**
         * Free indexes of specified type.
         * @param dir Type of indexes to free (IN or OUT).
         */
        void clear(EDirection dir) {
            storage_[dir].clear();
        }

    private:
        typedef typename std::vector<TIndex>            TIndexList;

        const TGraph    &graph_;
        TIndexList      storage_[2];

    private:
        void buildIfNeeded(TVertex vertex, EDirection dir) {
            size_t nVert = boost::num_vertices(graph_);
            TIndexList &list = storage_[dir];

            // check if index is available
            TIndex &index = list[vertex];
            if (index.size() != nVert) {
                // build index
                index.resize(nVert, false);
                BitmapIndexer::build(vertex, dir);
            }
        }

        /**
         * build one index using Depth Search
         * @param vertex Vertex to build index for.
         * @param dir Type of index to build (IN or OUT).
         */
        void build(TVertex vertex, EDirection dir) {
            using namespace boost;

            // check range
            size_t nVert = boost::num_vertices(graph_);
            assert(vertex < nVert);

            // obtain index reference
            TIndexList &list = storage_[dir];
            TIndex &index = list[vertex];

            // working stack
            typedef typename std::stack<TVertex> TStack;
            TStack stack;
            stack.push(vertex);

            // stack loop
            while (!stack.empty()) {
                TVertex current = stack.top();
                assert(current < nVert);
                stack.pop();

                if (dir == IN) {
                    // for all predecessors
                    typename Traits::in_edge_iterator ii, ii_end;
                    for(tie(ii, ii_end) = in_edges(current, graph_);
                            ii != ii_end; ++ii)
                    {
                        TVertex next = source(*ii, graph_);
                        if (!index[next])
                            // schedule vertex for next wheel
                            stack.push(next);

                        // mark vertex as reachable
                        index[next] = true;
                    }
                } else {
                    // for all successors
                    typename Traits::out_edge_iterator ii, ii_end;
                    for(tie(ii, ii_end) = out_edges(current, graph_);
                        ii != ii_end; ++ii)
                    {
                        TVertex next = target(*ii, graph_);
                        if (!index[next])
                            // schedule vertex for next wheel
                            stack.push(next);

                        // mark vertex as reachable
                        index[next] = true;
                    }
                }
            }
        }
};

/**
 * Vertex predicate using bitmap index. It can be used as parameter for
 * VertexFilter or boost::filtered_graph. Do not use it for already filtered
 * or sparse graphs. If you need to, please clone/pack it first. If you need to
 * combine two BitmapVertexPredicate predicates, consider bitwise and of their
 * bitmap.
 */
template <typename TGraph>
class BitmapVertexPredicate {
    public:
        typedef typename boost::graph_traits<TGraph>    Traits;
        typedef typename Traits::vertex_descriptor      TVertex;
        typedef TBitmapIndex                            TBitmap;
    public:
        /**
         * Dummy constructor internally used by boost::filtered_graph which
         * should be never used manually. It would most likely fail on assertion
         * for non-empty graph.
         */
        BitmapVertexPredicate() { }

        /**
         * @graph Original graph ought to be filtered. Do not use already
         * filtered and/or sparse graphs.
         * @param bitmap Bitmap to use as predicate.
         */
        BitmapVertexPredicate(const TGraph &graph, const TBitmap &bitmap):
            bitmap_(bitmap)
        {
            assert(bitmap.size() == boost::num_vertices(graph));
        }

        /**
         * the predicate defining special kind of factor where:
         * 1. all vertices are reachable from the src vertex
         * 2. from all vertices the dst vertex is reachable
         * src and dst vertices are included as well.
         * @param indexer BitmapIndexer like object used to obtain in/out bitmap
         * indexes.
         * @param src Source vertex.
         * @param dst Destination vertex.
         */
        template <typename TBitmapIndexer>
        BitmapVertexPredicate(TBitmapIndexer &indexer, TVertex src, TVertex dst):
            bitmap_(indexer.index(src, TBitmapIndexer::OUT))
        {
            bitmap_ &= indexer.index(dst, TBitmapIndexer::IN);
            bitmap_ [src] = true;
            bitmap_ [dst] = true;
        }

        /**
         * Effective method used by VertexFilter or boost::filtered_graph.
         * @param vertex Vertex asking for if it belongs to the filtered graph.
         * @return Return true if vertex belongs to filtered graph.
         */
        bool operator() (TVertex vertex) const {
            assert(vertex < bitmap_.size());
            return bitmap_[vertex];
        }
    private:
        TBitmap         bitmap_;
};

/**
 * Factor defined by bitmap specifying which vertices ought to be in the factor.
 * The filtered graph contains all vertices enabled by bitmap and all edges
 * which have both vertices in the filtered set of vertices.
 *
 * Do not use it for already filtered or sparse graphs. If you need to, please
 * clone/pack it first. If you need to combine two BitmapVertexPredicate
 * predicates, consider bitwise and of their bitmap.
 */
template <typename TGraph>
class BitmapFilter:
    public VertexFilter<TGraph, BitmapVertexPredicate>
{
    typedef VertexFilter<TGraph, BitmapVertexPredicate> TFilter;
    public:
        typedef boost::graph_traits<TGraph>             Traits;
        typedef BitmapVertexPredicate<TGraph>           TPred;
        typedef typename Traits::vertex_descriptor      TVertex;
        typedef typename TPred::TBitmap                 TBitmap;

    public:
        /**
         * @graph Original graph ought to be filtered. Do not use already
         * filtered and/or sparse graphs.
         * @param bitmap Bitmap to use as predicate.
         */
        BitmapFilter(const TGraph &graph, const TBitmap &bitmap):
            TFilter(graph, TPred(graph, bitmap))
        {
            assert(num_vertices(graph) == bitmap.size());
        }

        /**
         * This creates special kind of factor where:
         * 1. all vertices are reachable from the src vertex
         * 2. from all vertices the dst vertex is reachable
         * src and dst vertices are included as well.
         * @param indexer BitmapIndexer like object used to obtain in/out bitmap
         * indexes.
         * @param src Source vertex.
         * @param dst Destination vertex.
         */
        template <typename TBitmapIndexer>
        BitmapFilter(TBitmapIndexer &indexer, TVertex src, TVertex dst):
            TFilter(indexer.graph(), TPred(indexer, src, dst))
        {
        }
};

#endif // BITMAP_INDEX_H
