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

#ifndef SYMBOL_MAP_H
#define SYMBOL_MAP_H

#include "config.hh"
#include "CallGraph.hh"

#include <boost/graph/adjacency_list.hpp>

#include <map>
#include <string>
#include <vector>

template <typename TGraph>
class SymbolMap {
    public:
        typedef typename boost::graph_traits<TGraph>        Traits;
        typedef typename Traits::vertex_descriptor          TVertex;
        typedef typename std::vector<TVertex>               TVertexList;
        typedef typename std::map<std::string, TVertexList> TSymbolMap;

    public:
        SymbolMap(const TGraph &graph) {
            using namespace boost;

            typedef property_map<TGraph, FncProp>           TPropMapping;
            typedef typename TPropMapping::const_type       TProp;
            TProp prop = get(FncProp(), graph);

            typename Traits::vertex_iterator vi, vi_end;
            for(tie(vi, vi_end) = vertices(graph); vi != vi_end; ++vi) {
                PFnc fnc = get(prop, *vi);
                map_[fnc->name].push_back(*vi);
            }
        }

        const TVertexList& operator[] (const std::string &symbol) {
            return map_[symbol];
        }
    private:
        TSymbolMap map_;
};

#endif // SYMBOL_MAP_H
