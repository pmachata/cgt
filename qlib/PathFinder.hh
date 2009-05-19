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

#ifndef PATH_FINDER_H
#define PATH_FINDER_H

#include "config.hh"

#ifndef DEBUG_PATH_FINDER
#   define DEBUG_PATH_FINDER 0
#endif

#include <map>
#include <set>
#include <stack>
#include <vector>

#include <boost/graph/adjacency_list.hpp>

#if DEBUG_PATH_FINDER
#   include <iostream>
#endif

/**
 * PathFinder finds paths in oriented graph. It computes all paths in the graph
 * staring in given vertex on call of the compute method(). This computation is
 * time-consuming and memory-exhausting for non-trivial graphs, use it
 * carefully. It is highly recommended to filter the graph before processing
 * it by the PathFinder. Once the paths are computed for all vertices, they are
 * held by object till its destruction. You can ask for computed paths
 * by the method paths().
 *
 * Two types of PathFinder are implemented yet. You can choose what the 'path'
 * means - it can take care about unique set of edges or unique set of vertices.
 * It is obvious that the result of second type is the subset of the first. Type
 * of path finder is selected by template argument TFinderType and then
 * available as public typedef Type.
 *
 * In both types a path is defined by a sequence of graph's edges and stored
 * in TPath container available as public typedef. List of such paths is stored
 * in cointainer named TPathList.
 *
 * @param TGraph Type of graph, boost::adjacency_list is supported now.
 * @param TFinderType Type of PathFinder, UniqEdgePath and UniqVertexPath are
 * supported now.
 */
template <typename TGraph, template <typename> class TFinderType>
class PathFinder {
    public:
        typedef TFinderType<TGraph>                     Type;
        typedef typename boost::graph_traits<TGraph>    Traits;
        typedef typename Traits::vertex_descriptor      TVertex;
        typedef typename Traits::edge_descriptor        TEdge;
        typedef typename std::vector<TEdge>             TPath;
        typedef typename std::vector<TPath>             TPathList;
        typedef typename std::set<TPath>                TPathSet;

    public:
        /**
         * @param graph Graph to search. The given graph is cloned on object's
         * initialization. This is no waste since the path-related data are much
         * more complex then the graph itself.
         */
        PathFinder(const TGraph &graph):
            graph_(graph),
            storage_(num_vertices(graph))
        {
#if DEBUG_PATH_FINDER
            using namespace boost;

            nVert_ = 0;
            typename Traits::vertex_iterator vi, vi_end;
            for (tie(vi, vi_end) = vertices(graph); vi != vi_end; ++vi)
                ++nVert_;

            if (nVert_) {
                std::cerr << "--- starting finder for " << nVert_
                    << " vertices " << std::flush;
            }
#endif
        }

        /**
         * Compute all paths (of specified type) in the graph starting in given
         * vertex. This operation can take very long time for non-trivial
         * graphs, but is always guaranteed to end in a finite time.
         *
         * It is safe to call this operation multiple times for @b the @b same
         * @b vertex, but not useful at all to call it for different vertices.
         *
         * @param vertex Vertex where all paths begin.
         */
        void compute(TVertex vertex) {
            using namespace boost;

            // initialize start vertex
            Type::initStartVertex(storage_, vertex, TPath());

            // working stack
            typedef typename std::stack<TVertex> TStack;
            TStack stack;
            stack.push(vertex);

            // stack loop
            while (!stack.empty()) {
                TVertex current = stack.top();
                stack.pop();

#if DEBUG_PATH_FINDER
                if (nVert_)
                    std::cerr << "." << std::flush;
#endif

                // for all paths leading to current vertex
                TPathMap &map = storage_[current];
                for (TPathMapIterator mi = map.begin(); mi != map.end(); ++mi)
                {
                    const TPath &path = mi->first;
                    TUniqSet &uset = mi->second;

                    // for all successors of current vertex
                    typename Traits::out_edge_iterator ei, ei_end;
                    for(tie(ei, ei_end) = out_edges(current, graph_);
                            ei != ei_end; ++ei)
                    {
                        if (!Type::isUniq (uset, *ei, graph_))
                            // skip duplicated path
                            // according to type of PathFinder
                            continue;

                        // newPath = path + edge
                        TPath newPath(path);
                        newPath.push_back(*ei);

                        // add newPath to successor's paths
                        TVertex next = target(*ei, graph_);
                        if (PathFinder::addPath(next, newPath))

                            // schedule successor for (re)compute
                            stack.push(next);
                    }
                }
            }
#if DEBUG_PATH_FINDER
            if (nVert_)
                std::cerr << " done" << std::endl;
#endif
        }

        /**
         * Return all paths (of selected type) leading to given vertex. It makes
         * no sense to call this method before compute().
         * @param vertex All returned paths will end in this vertex.
         * @param list Reference to container for results. If the container is
         * not empty, result will be appended to end of the container.
         */
        void paths(TVertex vertex, TPathList &list) {
            TPathMap &map = storage_[vertex];
            for (TPathMapIterator i = map.begin(); i != map.end(); ++i)
                list.push_back(i->first);
        }

        /**
         * Return all paths (of selected type) leading to given vertex. It makes
         * no sense to call this method before compute().
         * @param vertex All returned paths will end in this vertex.
         * @param set Reference to container for results.
         */
        void paths(TVertex vertex, TPathSet &set) {
            TPathMap &map = storage_[vertex];
            for (TPathMapIterator i = map.begin(); i != map.end(); ++i)
                set.insert(i->first);
        }

    private:
        typedef typename Type::TUniq                    TUniq;
        typedef typename Type::TUniqSet                 TUniqSet;
        typedef typename std::map<TPath, TUniqSet>      TPathMap;
        typedef typename TPathMap::iterator             TPathMapIterator;
        typedef typename std::vector<TPathMap>          TStorage;
        TGraph      graph_;
        TStorage    storage_;
#if DEBUG_PATH_FINDER
        size_t      nVert_;
#endif

    private:
        /**
         * Add path to map for given vertex and index it accordingly to type of
         * PathFinder.
         * @param vertex Save a path which leads to this vertex.
         * @param path Path to save to vertex.
         * @return Return true, if path was not in the map before.
         */
        bool addPath(TVertex vertex, const TPath &path) {
            // check if path is already stored
            TPathMap &map = storage_[vertex];
            TPathMapIterator im = map.find(path);
            if (map.end() != im)
                return false;

            // add path and its index
            TUniqSet &uset = map[path];
            Type::indexPath(uset, path, graph_);
            return true;
        }
};

/**
 * This template specifies the type of PathFinder, do not use it separately.
 * While using this type of PathFinder, all edges in a path are unique. You'll
 * find a superset of paths found by the UniqVertexPath.
 */
template <typename TGraph>
struct UniqEdgePath {
    typedef typename boost::graph_traits<TGraph>        Traits;
    typedef typename Traits::edge_descriptor            TUniq;
    typedef typename std::set<TUniq>                    TUniqSet;

    template <typename TStorage, typename TVertex, typename TPath>
    static void initStartVertex(
            TStorage        &storage,
            TVertex         vertex,
            const TPath     &path)
    {
        storage[vertex][path] = TUniqSet();
    }

    template <typename TEdge>
    static bool isUniq (TUniqSet &uset, TEdge edge, TGraph &) {
        return uset.end() == uset.find(edge);
    }

    template <typename TPath>
    static void indexPath(TUniqSet &uset, const TPath &path, TGraph &) {
        typename TPath::const_iterator i;
        for (i = path.begin(); i != path.end(); ++i)
            uset.insert(*i);
    }
};

/**
 * This template specifies the type of PathFinder, do not use it separately.
 * While * using this type of PathFinder, all vertices in a path are unique.
 * You'll find a subset of paths found by the UniqEdgePathPath.
 */
template <typename TGraph>
struct UniqVertexPath {
    typedef typename boost::graph_traits<TGraph>        Traits;
    typedef typename Traits::vertex_descriptor          TUniq;
    typedef typename std::set<TUniq>                    TUniqSet;

    template <typename TStorage, typename TVertex, typename TPath>
    static void initStartVertex(
            TStorage        &storage,
            TVertex         vertex,
            const TPath     &path)
    {
        TUniqSet &uset = storage[vertex][path];
        uset.insert(vertex);
    }

    template <typename TEdge>
    static bool isUniq (TUniqSet &uset, TEdge edge, TGraph &graph) {
        typename Traits::vertex_descriptor vertex = target(edge, graph);
        return uset.end() == uset.find(vertex);
    }

    template <typename TPath>
    static void indexPath(TUniqSet &uset, const TPath &path, TGraph &graph) {
        typename TPath::const_iterator i = path.begin();
        if (i == path.end())
            return;

        uset.insert(source(*i, graph));
        for (; i != path.end(); ++i)
            uset.insert(target(*i, graph));
    }
};

/**
 * This template specifies a dummy type of PathFinder, which never finds
 * anything. This type of PathFinder is useful to run at all. But it can be used
 * obtain public typedef which are not dependent on type of the PathFinder.
 */
template <typename TGraph>
struct VoidPathFinder {
    typedef typename boost::graph_traits<TGraph>        Traits;
    typedef void*                                       TUniq;
    typedef void*                                       TUniqSet;

    template <typename TStorage, typename TVertex, typename TPath>
    static void initStartVertex(TStorage &, TVertex, const TPath &) { }

    template <typename TEdge>
    static bool isUniq (TUniqSet &, TEdge, TGraph &) {
        return false;
    }

    template <typename TPath>
    static void indexPath(TUniqSet &, const TPath &, TGraph &) { }
};

#endif // PATH_FINDER_H
