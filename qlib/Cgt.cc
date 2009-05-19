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
#include "Cgt.hh"

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#ifndef DEBUG_DEMANGLE
#   define DEBUG_DEMANGLE 0
#endif

#define REGEX_ID "[A-Za-z_][A-Za-z0-9_]*"

// part of non-public include/demangle.h from binutils
extern "C" {
    extern char* cplus_demangle(const char *mangled, int options);
#define DMGL_NO_OPTS	 0		/* For readability... */
#define DMGL_PARAMS	 (1 << 0)	/* Include function args */
#define DMGL_ANSI	 (1 << 1)	/* Include const, volatile, etc */
#define DMGL_JAVA	 (1 << 2)	/* Demangle as Java rather than C++. */
#define DMGL_VERBOSE	 (1 << 3)	/* Include implementation details.  */
#define DMGL_TYPES	 (1 << 4)	/* Also try to demangle type encodings.  */
#define DMGL_RET_POSTFIX (1 << 5)       /* Print function return types (when
                                           present) after function signature */
}

void demangle(PFnc fnc) {
    const char *demangled = cplus_demangle(fnc->name.c_str(), DMGL_PARAMS);
    if (demangled) {
#if DEBUG_DEMANGLE
        std::cerr
            << Color(C_LIGHT_BLUE) << fnc->name
            << Color(C_NO_COLOR) << " --> "
            << Color(C_YELLOW) << demangled
            << Color(C_NO_COLOR)
            << std::endl << std::endl;
#endif
        fnc->name = demangled;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// CgtReader implemetation
struct CgtReader::Private {
    ICgtReaderListener      *listener;

    const boost::regex      reDecl;
    const boost::regex      reDef;
    const boost::regex      reFile;
    const boost::regex      reHeader;
    const boost::regex      reVar;

    Private(ICgtReaderListener *listener_):
        listener(listener_),

        reDecl("^([0-9]+) \\(([0-9]+)\\) @decl((?: @static:?)?) (" REGEX_ID
                ")$"),

        reDef("^([0-9]+) \\(([0-9]+)\\)((?: @static:?)?) (" REGEX_ID ") *"
                "((?:(?: [0-9]+:?)|(?: \\*:?):?)*)$"),

        reFile("^F (.*)$"),

        reHeader("^.*\\.h$"),    // FIXME: add another header extensions

        reVar("^[0-9]+ \\([0-9]+\\)(?: @decl:?)?(?: @static:?)? @var"
                "(?: @static:?)? " REGEX_ID "$")
    {
    }
};

CgtReader::CgtReader(ICgtReaderListener *listener):
    d(new Private(listener))
{
}

CgtReader::~CgtReader() {
    delete d;
}

bool CgtReader::read(std::istream &input, bool performDemangle) {
    // TODO: use an optimized (one-pass) scanner

    using namespace boost;
    using std::string;

    smatch result;
    string fileName;
    bool fileIsHeader = false;
    string line;

    while (std::getline(input, line)) {

        // match: function declaration
        if (regex_match(line, result, d->reDecl)) {
            PFnc fnc(new Fnc);
            fnc->name = result[4];
            *(fnc->loc.file) = fileName;
            fnc->loc.lineno = lexical_cast<long>(result[2]);
            fnc->isGlobal = string(result[3]).empty();
            if (performDemangle)
                demangle(fnc);
            d->listener->addFnc(lexical_cast<TFncId>(result[1]), fnc);
            continue;
        }

        // match: function definition
        if (regex_match(line, result, d->reDef)) {
            TFncId caller = lexical_cast<TFncId>(result[1]);
            PFnc fnc(new Fnc);
            fnc->name = result[4];
            *(fnc->loc.file) = fileName;
            fnc->loc.lineno = lexical_cast<long>(result[2]);
            fnc->isGlobal = string(result[3]).empty()
                // FIXME: this is workaround for "static inline..."
                || fileIsHeader;
            fnc->isDefined = true;
            if (performDemangle)
                demangle(fnc);
            d->listener->addFnc(caller, fnc);

            const string &calleeList = result[5];
            const char *c = calleeList.c_str();
            while (*c) {
                for(; *c && isspace(*c); ++c);
                if (*c == '*') {
                    c++;
                } else {
                    string callee;
                    for(; *c && !isspace(*c); ++c)
                        callee.push_back(*c);
                    d->listener->addCall(caller, lexical_cast<TFncId>(callee));
                }
            }
            continue;
        }

        // match: original file
        if (regex_match(line, result, d->reFile)) {
            fileName = result[1];
            fileIsHeader = regex_match(fileName, d->reHeader);
            continue;
        }

        // match: var declaration/definition
        if (regex_match(line, d->reVar)) {
#if DEBUG_SHOW_VARS
            std::cerr << Color(C_YELLOW) << "Var: " << Color(C_NO_COLOR)
                << line << std::endl;
#else
            // ignore silently
#endif
            continue;
        }

        // no match
#if DEBUG_SHOW_UNHANDLED
        std::cerr << Color(C_LIGHT_RED) << "Unhandled: " << Color(C_NO_COLOR)
            << line << std::endl;
#endif
    }

    // FIXME: return false if any error is detected
    return true;
}

// /////////////////////////////////////////////////////////////////////////////
// CgtWriter implementation
struct CgtWriter::Private {
    typedef std::map<TFncId, TFncId> TMap;

    std::ostream    &output;
    TMap            idMap;
    TFncId          lastId;

    Private(std::ostream &output_):
        output(output_),
        lastId(1)
    {
    }

    TFncId mapId(TFncId origId) {
        TMap::iterator i = idMap.find(origId);
        if (i != idMap.end())
            return i->second;

        TFncId id = lastId++;
        idMap[origId] = id;
        return id;
    }
};

CgtWriter::CgtWriter(std::ostream &output):
    d(new Private(output))
{
}

CgtWriter::~CgtWriter() {
    delete d;
}

void CgtWriter::writeFile(std::string fileName) {
    d->output << "F " << fileName << std::endl;
}

void CgtWriter::writeFnc(TFncId id, PFnc fnc) {
    d->output << d->mapId(id) << " (" << fnc->loc.lineno << ") ";
    if (!fnc->isGlobal)
        d->output << "@static ";
    if (!fnc->isDefined)
        d->output << "@decl ";
    d->output << fnc->name;
}

void CgtWriter::writeCall(TFncId target, PFnc) {
    d->output << " " << d->mapId(target);
}

void CgtWriter::writeFncEnd() {
    d->output << std::endl;
}
