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

#ifndef SHOW_LOOKUP_PROGRESS
#   define SHOW_LOOKUP_PROGRESS 0
#endif

#include "BitmapIndex.hh"
#include "CallGraph.hh"
#include "Cgt.hh"
#include "PathFinder.hh"
#include "SymbolMap.hh"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <boost/regex.hpp>

#define REGEX_ID "[A-Za-z_][A-Za-z0-9_]*"

static const char hstFile[] = ".cgq_history";

template <typename TGraph, typename TPathSet>
inline void writePaths(
        const TGraph &graph,
        const TPathSet &pathSet, 
        std::ostream &str)
{
    using namespace boost;

    typedef property_map<TGraph, FncProp>               TPropMapping;
    typedef typename TPropMapping::const_type           TProp;
    TProp prop = get(FncProp(), graph);

    unsigned cnt = 0;
    typename TPathSet::iterator i;
    for(i = pathSet.begin(); i!= pathSet.end(); ++i) {
        typedef typename TPathSet::value_type           TPath;
        typedef typename TPath::const_iterator          TPathIterator;

        const TPath &path = *i;
        TPathIterator ei = path.begin();
        if (ei == path.end())
            continue;

        str << "path #" << cnt++ << ":" << std::endl;
        PFnc fnc = get(prop, source(*ei, graph));
        str << "  " << fnc << std::endl;

        for (; ei != path.end(); ++ei) {
            PFnc fnc = get(prop, target(*ei, graph));
            str << "  " << fnc << std::endl;
        }
        str << std::endl;
    }
}

template <typename TGraph, typename TIndexer, typename TSymbolMap>
class PathLookup {
    public:
        typedef boost::graph_traits<TGraph>             Traits;
        typedef typename Traits::vertex_descriptor      TVertex;
        typedef typename TSymbolMap::TVertexList        TVertexList;

    public:
        PathLookup(const TGraph &graph, TIndexer &indexer, TSymbolMap &sMap):
            graph_(graph),
            indexer_(indexer),
            sMap_(sMap),
            fncProp_(get(FncProp(), graph)),
            all_(num_vertices(graph))
        {
            typename Traits::vertex_iterator vi, vi_end;
            tie(vi, vi_end) = vertices(graph);
            std::copy(vi, vi_end, all_.begin());
        }

        bool lookup (const std::string &line)
        {
            using std::string;

            std::istringstream str(line);

            string src;
            if (!(str>>src)) {
                std::cerr << Color(C_LIGHT_RED) << "can't read src symbol"
                    << Color(C_NO_COLOR) << std::endl;
                return false;
            }

            string dst;
            if (!(str>>dst)) {
                std::cerr << Color(C_LIGHT_RED) << "can't read dst symbol"
                    << Color(C_NO_COLOR) << std::endl;
                return false;
            }

            const TVertexList &srcList =
                (src == string("*")) ? all_ : sMap_[src];
            if (!srcList.size()) {
                std::cerr << Color(C_LIGHT_RED) << "symbol not found: "
                    << Color(C_NO_COLOR) << src << std::endl;
                return false;
            }

            const TVertexList &dstList =
                (dst == string("*")) ? all_ : sMap_[dst];
            if (!dstList.size()) {
                std::cerr << Color(C_LIGHT_RED) << "symbol not found: "
                    << Color(C_NO_COLOR) << dst << std::endl;
                return false;
            }

            return lookup(srcList, dstList);
        }

        bool lookup (const TVertexList &src, const TVertexList &dst)
        {
            using namespace boost;

            typedef BitmapFilter<TGraph>                        TFilter;
            typedef PathFinder<TFilter, UniqEdgePath>           TFinder;
            typedef typename TFinder::TPathList                 TPathList;
            typedef typename TPathList::value_type              TPath;
            typedef typename std::set<TPath>                    TPathSet;

#if SHOW_LOOKUP_PROGRESS
            size_t total = src.size()*dst.size();
            std::cerr << "--- searching paths for " << total << " vertex pairs ... ";
            bool showProgress = ttyname(STDERR_FILENO);
            if (showProgress)
                std::cerr << " 0.0%" << std::flush;
            size_t i = 0;
            size_t percLast = 0;
#endif

            TPathSet pathSet;

            typename TVertexList::const_iterator si;
            for(si = src.begin(); si != src.end(); ++si) {
                typename TVertexList::const_iterator di;
                for(di = dst.begin(); di != dst.end(); ++di) {
                    TFilter factor(indexer_, *si, *di);
                    TFinder finder(factor);
                    finder.compute(*si);

                    TPathList paths;
                    finder.paths(*di, paths);

                    typename TPathList::const_iterator i;
                    for (i = paths.begin(); i != paths.end(); ++i)
                        pathSet.insert(*i);

#if SHOW_LOOKUP_PROGRESS
                    if (showProgress) {
                        ++i;
                        size_t perc = i*1000/total;
                        if (perc != 1000 && perc != percLast) {
                            percLast = perc;
                            float f = static_cast<float>(perc)/10;
                            std::cerr << std::string(5, '\b') << std::fixed
                                << std::setw(4) << std::setprecision(1)
                                << f << "%" << std::flush;
                        }
                    }
#endif
                }
            }

#if SHOW_LOOKUP_PROGRESS
            if (showProgress)
                std::cerr << std::string(5, '\b');
            std::cerr << "done" << std::endl;
#endif

            writePaths(graph_, pathSet, std::cout);
            return true;
        }
    private:
        typedef boost::property_map<TGraph, FncProp>    TPropMapping;
        typedef typename TPropMapping::const_type       TProp;

        const TGraph        &graph_;
        TIndexer            &indexer_;
        TSymbolMap          &sMap_;
        TProp               fncProp_;
        TVertexList         all_;
};

template <typename TGraph, typename TIndexer, typename TSymbolMap>
class VertexLookup {
    public:
        typedef boost::graph_traits<TGraph>             Traits;
        typedef typename Traits::vertex_descriptor      TVertex;
        typedef typename TSymbolMap::TVertexList        TVertexList;

    public:
        VertexLookup(const TGraph &graph, TIndexer &indexer, TSymbolMap &sMap):
            graph_(graph),
            indexer_(indexer),
            sMap_(sMap),
            fncProp_(get(FncProp(), graph)),
            all_(num_vertices(graph))
        {
            typename Traits::vertex_iterator vi, vi_end;
            tie(vi, vi_end) = vertices(graph);
            std::copy(vi, vi_end, all_.begin());
        }

        bool lookup(const std::string &symbol, int level = 0) {
            const TVertexList &vertexList =
                (symbol == "*") ? all_ : sMap_[symbol];

            if (!vertexList.size()) {
                std::cerr << Color(C_LIGHT_RED) << "symbol not found: "
                    << Color(C_NO_COLOR) << symbol << std::endl;
                return false;
            }

            size_t nVert = all_.size();
            typename TVertexList::const_iterator vi;
            for(vi = vertexList.begin(); vi != vertexList.end(); ++vi) {
                std::cout << get(fncProp_, *vi) << std::endl;
                if (!level)
                    continue;

                if (1 < level) {
                    typedef typename TIndexer::TIndex TIndex;
                    const TIndex &idxIn = indexer_.index(*vi, TIndexer::IN);
                    for(TVertex v = 0; v < nVert; ++v)
                        if (idxIn[v])
                            std::cout << "  <---- " << get(fncProp_, v) << std::endl;

                    const TIndex &idxOut = indexer_.index(*vi, TIndexer::OUT);
                    for(TVertex v = 0; v < nVert; ++v)
                        if (idxOut[v])
                            std::cout << "  ----> " << get(fncProp_, v) << std::endl;

                    continue;
                }

                typename Traits::in_edge_iterator ii, ii_end;
                for(tie(ii, ii_end) = in_edges(*vi, graph_); ii != ii_end; ++ii) {
                    TVertex v = source(*ii, graph_);
                    std::cout << "  <-- " << get(fncProp_, v) << std::endl;
                }

                typename Traits::out_edge_iterator oi, oi_end;
                for(tie(oi, oi_end) = out_edges(*vi, graph_); oi != oi_end; ++oi) {
                    TVertex v = target(*oi, graph_);
                    std::cout << "  --> " << get(fncProp_, v) << std::endl;
                }
            }
            return true;
        }

    private:
        typedef boost::property_map<TGraph, FncProp>    TPropMapping;
        typedef typename TPropMapping::const_type       TProp;

        const TGraph        &graph_;
        TIndexer            &indexer_;
        TSymbolMap          &sMap_;
        TProp               fncProp_;
        TVertexList         all_;
};

class StopWatch {
        static const long RATIO = CLOCKS_PER_SEC/1000L;
    public:
        void start() {
            start_ = clock();
        }
        void stop() {
            diff_ = clock() - start_;
        }
        long getTimeElapsed() const {
            return diff_/RATIO;
        }
    private:
        clock_t start_;
        clock_t diff_;
};
std::ostream& operator<<(std::ostream &str, const StopWatch &watch) {
    str << std::fixed << std::setw(8) << std::setprecision(2)
        << static_cast<double>(watch.getTimeElapsed())/1000
        << "s";

    return str;
}

template <typename TGraph, typename TIndexer, typename TSymbolMap>
class CmdHandler {
    public:
        CmdHandler(const TGraph &graph, TIndexer &indexer, TSymbolMap &sMap):
            graph_(graph),
            indexer_(indexer),
            sMap_(sMap),
            lVertex_(graph, indexer, sMap),
            lPath_(graph, indexer, sMap),
            rePath_( "^ *(?:\\*|(?:" REGEX_ID ":?):?) *(?:\\*|(?:" REGEX_ID
                    ":?):?) *$"),
            reVertex_( "^\\? *((?:\\*|(?:" REGEX_ID ":?):?)) *$"),
            reVertexAdj_("^\\?\\? *((?:\\*|(?:" REGEX_ID ":?):?)) *$"),
            reDeepVertex_( "^\\?\\?\\? *((?:\\*|(?:" REGEX_ID ":?):?)) *$")
        {
        }

        bool handleTimed (const std::string &line) {
            watch_.start();
            bool status = handle(line);
            watch_.stop();
            std::cerr << "--- query completed in " << watch_ << std::endl;
            return status;
        }

        bool handle (const std::string &line) {
            using namespace boost;
            smatch result;

            if (regex_match(line, rePath_))
                return lPath_.lookup(line);

            if (regex_match(line, result, reVertex_))
                return lVertex_.lookup(result[1]);

            if (regex_match(line, result, reVertexAdj_))
                return lVertex_.lookup(result[1], 1);

            if (regex_match(line, result, reDeepVertex_))
                return lVertex_.lookup(result[1], 2);

            if (line == std::string("!index")) {
                std::cerr << "--- building OUT indexes" << std::flush;
                indexer_.build(TIndexer::OUT);
                std::cerr << std::endl;
                return true;
            }

            if (line == std::string("!rindex")) {
                std::cerr << "--- building IN indexes" << std::flush;
                indexer_.build(TIndexer::IN);
                std::cerr << std::endl;
                return true;
            }

            std::cerr << Color(C_LIGHT_RED) << "unknow command: "
                << Color(C_NO_COLOR) << line << std::endl;
            return false;
        }
    private:
        typedef VertexLookup<TGraph, TIndexer, TSymbolMap>  TVertexLookup;
        typedef PathLookup<TGraph, TIndexer, TSymbolMap>  TPathLookup;

        const TGraph        &graph_;
        TIndexer            &indexer_;
        TSymbolMap          &sMap_;

        TVertexLookup lVertex_;
        TPathLookup lPath_;
        StopWatch watch_;

        const boost::regex rePath_;
        const boost::regex reVertex_;
        const boost::regex reVertexAdj_;
        const boost::regex reDeepVertex_;
};

int main(int argc, char *argv[]) {
    using std::string;

    Color::enable(ttyname(STDERR_FILENO));

    // TODO: use getopt_long to read cmd-line args
    if (argc < 2)
        return 1;

    // open cg file
    const char *cgFile = argv[1];
    std::fstream str(cgFile, std::ios::in);
    if (!str) {
        std::cerr << Color(C_LIGHT_RED) << "can't open " << Color(C_NO_COLOR)
            << cgFile << std::endl;
        return 1;
    }

    // create call graph
    typedef CallGraph TGraph;
    TGraph graph;

    // parse input
    {
        std::cerr << "--- parsing " << cgFile << " ... " << std::flush;
        CgtGraphBuilder<TGraph> builder(graph);
        CgtReader reader(&builder);
        reader.read(str, true);
        std::cerr << "done" << std::endl;
    }

    // build symbol table
    std::cerr << "--- building symbol table ... " << std::flush;
    typedef SymbolMap<TGraph> TSymbolMap;
    TSymbolMap sMap(graph);
    std::cerr << "done" << std::endl;

    // bitmap indexer
    typedef BitmapIndexer<TGraph> TBitmapIndexer;
    TBitmapIndexer indexer(graph);

    // universal command handler
    typedef CmdHandler<TGraph, TBitmapIndexer, TSymbolMap> TCmdHandler;
    TCmdHandler cmd(graph, indexer, sMap);

    // determine terminal mode
    bool interactiveMode = ttyname(STDIN_FILENO) && ttyname(STDOUT_FILENO);
    if (interactiveMode)
        std::cerr << "--- working in interactive mode" << std::endl;
    else
        std::cerr << "--- working in batch mode" << std::endl;


    if (interactiveMode) {
        // interactive mode
        read_history(hstFile);
        std::ostringstream tmp;
        tmp
            << Color(C_LIGHT_GREEN) << "         !index" << Color(C_NO_COLOR)
            << " - build bitmap index (for out edges)" << std::endl
            << Color(C_LIGHT_GREEN) << "        !rindex" << Color(C_NO_COLOR)
            << " - build reverse bitmap index (for in edges)" << std::endl
            << Color(C_LIGHT_GREEN) << "        ?" << Color(C_NO_COLOR)
            << Color(C_YELLOW) << "symbol" << Color(C_NO_COLOR)
            << " - symbol lookup" << std::endl
            << Color(C_LIGHT_GREEN) << "       ??" << Color(C_NO_COLOR)
            << Color(C_YELLOW) << "symbol" << Color(C_NO_COLOR)
            << " - one level symbol lookup" << std::endl
            << Color(C_LIGHT_GREEN) << "      ???" << Color(C_NO_COLOR)
            << Color(C_YELLOW) << "symbol" << Color(C_NO_COLOR)
            << " - deep symbol lookup (requires bitmap index)" << std::endl
            << Color(C_YELLOW) << "symbol1 symbol2" << Color(C_NO_COLOR)
            << " - search all paths between symbols" << std::endl
            << Color(C_LIGHT_BLUE) << "              *" << Color(C_NO_COLOR)
            << " - all vertices" << std::endl
            << std::endl
            << Color(C_LIGHT_GREEN) << "query" << Color(C_NO_COLOR)
            << " > " << std::flush;
        string prompt(tmp.str());
        const char *line;
        while ((line = readline(prompt.c_str()))) {
            if (!*line)
                continue;

            add_history(line);
            cmd.handleTimed(line);
            std::cout << std::endl;
        }
        std::cout << std::endl;
        if (write_history(hstFile))
            std::cerr << Color(C_LIGHT_RED) << "unable to save history"
                << Color(C_NO_COLOR) << ", errno = " << errno << std::endl;
        clear_history();
    } else {
        // batch mode
        string line;
        while (getline(std::cin, line)) {
            std::cerr << ">>> " << line << std::endl;
            cmd.handleTimed(line);
            std::cerr << std::endl;
        }
    }

    return 0;
}
