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

#ifndef VERTEX_FILTER_H
#define VERTEX_FILTER_H

#include "config.h"

#include <boost/graph/filtered_graph.hpp>

/**
 * VertexFilter stands for a factor defined by a vertex predicate. This template
 * is implemented as a wrapper around boost::filtered_graph.
 * @param TGraph Type of original graph to filter.
 * @param TPred Type of predicate which defines the vertex set. This
 * class/template must define a method bool operator() (TVertex) const which
 * returns true for vertices belonging to subgraph, false otherwise.
 */
template <typename TGraph, template <typename> class TPred>
class VertexFilter:
    public boost::filtered_graph<TGraph, boost::keep_all, TPred<TGraph> >
{
    typedef boost::filtered_graph<TGraph, boost::keep_all, TPred<TGraph> >
        TFilteredGraph;

    public:
        /**
         * Simplified initialization; predicate is initialized by a reference
         * to original graph.
         * @param graph Original graph which oughts to be filtered.
         */
        VertexFilter(const TGraph &graph):
            TFilteredGraph(graph, boost::keep_all(), TPred<TGraph>(graph))
        {
        }

        /**
         * @param graph Original graph which oughts to be filtered.
         * @param pred Reference to TPred predicate which should be used
         * as vertices filter.
         */
        VertexFilter(const TGraph &graph, const TPred<TGraph> &pred):
            TFilteredGraph(graph, boost::keep_all(), pred)
        {
        }
};

#endif // VERTEX_FILTER_H
