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

#ifndef LINKER_H
#define LINKER_H

#include "config.hh"
#include "CallGraph.hh"
// FIXME: do not write directly to stderr
#include "Color.hh"

// FIXME: do not write directly to stderr
#include <iostream>
#include <map>
#include <string>

#include <boost/graph/adjacency_list.hpp>

/// call graph linker
template <typename TGraph>
class Linker {
    public:
        typedef typename boost::graph_traits<TGraph>    Traits;
        typedef typename Traits::vertex_descriptor      TVertex;

    public:
        Linker():
            fncProp_(get(FncProp(), graph_))
        {
        }

        const TGraph& output() const {
            return graph_;
        }

        template <typename TChunk>
        void link(TChunk &chunk) {
            using namespace boost;

            typedef graph_traits<TChunk>                TChunkTraits;
            typedef std::map<TVertex, TVertex>          TVertexMap;
            TVertexMap vertexMap;

            // for each vertex
            typename TChunkTraits::vertex_iterator vi, vi_end;
            for(tie(vi, vi_end) = vertices(chunk); vi != vi_end; ++vi) {
                TVertex v = *vi;
                PFnc fnc = get(get(FncProp(), chunk), v);
                if (!fnc->isGlobal) {
                    // static declaration/definition
                    vertexMap[v] = add_vertex(fnc, graph_);
                    continue;
                }

                std::string &symbol = fnc->name;
                typename TSymbolTable::iterator i = symbolTable_.find(symbol);
                if (i == symbolTable_.end()) {
                    // add function to symbol table
                    TVertex mv = add_vertex(fnc, graph_);
                    symbolTable_[symbol] = mv;
                    vertexMap[v] = mv;
                    continue;
                }
                TVertex masterVertex = i->second;
                vertexMap[v] = masterVertex;
                PFnc prev = get(fncProp_, masterVertex);
                if (fnc->isDefined) {
                    // function already in symbol table

                    if (prev->isDefined) {
                        // function redefinition
                        if (fnc->loc != prev->loc) {
                            // FIXME: do not write directly to stderr
                            std::cerr << Color(C_LIGHT_RED)
                                << "Redefinition: " << fnc
                                << Color(C_NO_COLOR) << std::endl;
                            std::cerr << Color(C_LIGHT_PURPLE)
                                << "Previous definition: " << prev
                                << Color(C_NO_COLOR) << std::endl;
                        }
                    } else {
                        // rewrite declaration by definition
                        put(fncProp_, masterVertex, fnc);
                    }
                }
            }

            // edge (call) property
            typedef property_map<TChunk, CallProp>      TEdgePropMapping;
            typedef typename TEdgePropMapping::type     TEdgeProp;
            TEdgeProp edgeProp = get(CallProp(), chunk);

            // for each edge (call)
            typename TChunkTraits::edge_iterator ei, ei_end;
            for(tie(ei, ei_end) = edges(chunk); ei != ei_end; ++ei) {
                add_edge(vertexMap[source(*ei, chunk)],
                    vertexMap[target(*ei, chunk)], get(edgeProp, *ei),
                    graph_);
            }
        }
    private:
        typedef boost::property_map<TGraph, FncProp>    TPropMapping;
        typedef typename TPropMapping::type             TProp;
        typedef typename std::map<std::string, TVertex> TSymbolTable;

        TGraph          graph_;
        TProp           fncProp_;
        TSymbolTable    symbolTable_;
};

#endif // LINKER_H
