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

#ifndef CALLGRAPH_H
#define CALLGRAPH_H

#include "config.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <map>
#include <set>

/// symbol location
struct Location {
    typedef boost::shared_ptr<std::string> PString;
    PString         file;       ///< file name (shared string)
    long            lineno;     ///< line number

    Location():
        file(new std::string),
        lineno(-1)
    {
    }
};
inline bool operator==(Location &a, Location &b) {
    return *(a.file) == *(b.file)
        && a.lineno == b.lineno;
}
inline bool operator!=(Location &a, Location &b) {
    return !operator==(a, b);
}

/// per-function data (vertex properties)
struct Fnc {
    std::string     name;       ///< function name (identifier)
    Location        loc;        ///< symbol location
    bool            isGlobal;   ///< true if symbol name is globally valid
    bool            isDefined;  ///< true for definition, false for declaration

    Fnc():
        isGlobal(false),
        isDefined(false)
    {
    }
};
typedef boost::shared_ptr<Fnc> PFnc;

inline std::ostream &operator<< (std::ostream &str, PFnc fnc) {
    if (!fnc->isDefined)
        str << "@decl ";

    if (!fnc->isGlobal)
        str << "@static ";

    str << fnc->name
        << " (" << *(fnc->loc.file) << ":" << fnc->loc.lineno << ")";

    return str;
}

/// vertex property - pointer to Fnc object
struct FncProp {
    typedef boost::vertex_property_tag kind;
};
typedef boost::property<FncProp, PFnc> FncPropTag;

/// edge property - call location (FIXME: not used now)
struct CallProp {
    typedef boost::edge_property_tag kind;
};
typedef boost::property<CallProp, Location> CallPropTag;

/// call graph - defined as adjacency_list specialization
typedef boost::adjacency_list<
        boost::vecS,
        boost::vecS,
        boost::bidirectionalS,
        FncPropTag,
        CallPropTag>
    CallGraph;

/// write graph using given TWriter object
template <typename TGraph, typename TWriter>
static void write(const TGraph &graph, TWriter &builder) {
    using namespace boost;

    typedef graph_traits<TGraph>                        Traits;
    typedef typename Traits::vertex_descriptor          TVertex;
    typedef std::set<TVertex>                           TVertexSet;
    typedef std::map<std::string, TVertexSet>           TFileMap;

    // fnc property
    typedef property_map<TGraph, FncProp>               TPropMapping;
    typedef typename TPropMapping::const_type           TProp;
    TProp prop = get(FncProp(), graph);

    // sort by file name
    TFileMap fileMap;
    typename Traits::vertex_iterator vi, vi_end;
    for (tie(vi, vi_end) = vertices(graph); vi != vi_end; ++vi) {
        PFnc fnc = get(prop, *vi);
        const std::string &fileName = *(fnc->loc.file);
        fileMap[fileName].insert(*vi);
    }

    // for each file
    typename TFileMap::iterator it;
    for(it = fileMap.begin(); it != fileMap.end(); ++it) {
        builder.writeFile(it->first);
        TVertexSet &set = it->second;

        // for each function
        typename TVertexSet::iterator i;
        for(i = set.begin(); i!= set.end(); ++i) {
            TVertex v = *i;
            builder.writeFnc(v, get(prop, v));

            // for each call
            typename Traits::out_edge_iterator oi, oi_end;
            for (tie(oi, oi_end) = out_edges(v, graph); oi != oi_end; ++oi) {
                TVertex ov = target(*oi, graph);
                builder.writeCall(ov, get(prop, ov));
            }
            builder.writeFncEnd();
        }
    }
}

/// vertex predicate filtering unused declarations
template <typename TGraph>
class DropUnusedDeclarations {
    public:
        typedef typename boost::graph_traits<TGraph>    Traits;
        typedef typename Traits::vertex_descriptor      TVertex;

    public:
        DropUnusedDeclarations(): graph_(0) { }

        DropUnusedDeclarations(const TGraph &graph):
            graph_(&graph),
            fncProp_(boost::get(FncProp(), graph))
        {
        }

        bool operator()(const TVertex &vertex) const {
            return boost::get(fncProp_, vertex)->isDefined
                || (0 != boost::in_degree(vertex, *graph_))
                || (0 != boost::out_degree(vertex, *graph_));
        }

    private:
        typedef boost::property_map<TGraph, FncProp>    TPropMapping;
        typedef typename TPropMapping::const_type       TProp;

        const TGraph    *graph_;
        TProp           fncProp_;
};

#endif // CALLGRAPH_H
