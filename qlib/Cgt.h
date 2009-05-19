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

#ifndef CGT_H
#define CGT_H

#include "config.h"
#include "CallGraph.h"
#include "Color.h"

#include <iostream>
#include <map>

/// type used for function ID (cgt format)
typedef long TFncId;

void demangle(PFnc fnc);

class ICgtReaderListener {
    public:
        virtual ~ICgtReaderListener() { }
        virtual void addFnc(TFncId id, PFnc fnc) = 0;
        virtual void addCall(TFncId a, TFncId b) = 0;
};

/// cgt format reader
class CgtReader {
    public:
        CgtReader(ICgtReaderListener *);
        ~CgtReader();
        bool read(std::istream &, bool demangle);
    private:
        struct Private;
        Private *d;
};

/// cgt format writer
class CgtWriter {
    public:
        CgtWriter(std::ostream &output);
        ~CgtWriter();

        void writeFile(std::string);
        void writeFnc(TFncId, PFnc);
        void writeCall(TFncId target, PFnc);
        void writeFncEnd();
    private:
        struct Private;
        Private *d;
};

/// call graph builder for cgt format (used by parser)
template <typename TGraph>
class CgtGraphBuilder: public ICgtReaderListener {
    public:
        typedef typename boost::graph_traits<TGraph>    Traits;
        typedef typename Traits::vertex_descriptor      TVertex;

    public:
        CgtGraphBuilder(TGraph &graph):
            graph_(graph),
            fncProp_(get(FncProp(), graph))
        {
        }

        virtual void addFnc(TFncId id, PFnc fnc) {
            using namespace boost;

#if DEBUG_SHOW_VERTEX_PROPERTY
            std::cerr << Color(C_LIGHT_PURPLE) << "vertex property: " << Color(C_NO_COLOR)
                << id << " --> " << fnc << std::endl;
#endif
            TMapIterator iter = map_.find(id);
            if (iter != map_.end())
                put(fncProp_, iter->second, fnc);
            else
                map_[id] = add_vertex(fnc, graph_);
        }

        virtual void addCall(TFncId a, TFncId b) {
            TMapIterator ia = map_.find(a);
            if (ia == map_.end()) {
                // FIXME: throw std logic error?
                std::cerr << Color(C_LIGHT_RED) << "Internal error: "
                    << Color(C_NO_COLOR) << "source edge not found"
                    << std::endl;
                return;
            }
            add_edge(ia->second, addVertexIfNeeded(b), graph_);
        }

    private:
        TVertex addVertexIfNeeded(TFncId id) {
            TMapIterator iter = map_.find(id);
            if (iter != map_.end())
                return iter->second;

            TVertex vd = boost::add_vertex(PFnc(new Fnc), graph_);
#if DEBUG_SHOW_VERTEX_MAPPING
            std::cerr << Color(C_LIGHT_BLUE) << "vertex mapping: " << Color(C_NO_COLOR)
                << id << " --> " << vd << std::endl;
#endif
            map_[id] = vd;
            return vd;
        }

    private:
        typedef boost::property_map<TGraph, FncProp>    TPropMapping;
        typedef typename TPropMapping::type             TProp;
        typedef typename std::map<TFncId, TVertex>      TMap;
        typedef typename TMap::iterator                 TMapIterator;

        TMap        map_;
        TGraph      &graph_;
        TProp       fncProp_;
};

#endif // CGT_H
