#include "utility.hpp"

// Needed for automatic name demangling, but not all that portable
#include <cxxabi.h>
#include <execinfo.h>

namespace vg {

char reverse_complement(const char& c) {
    switch (c) {
        case 'A': return 'T'; break;
        case 'T': return 'A'; break;
        case 'G': return 'C'; break;
        case 'C': return 'G'; break;
        case 'N': return 'N'; break;
        // Handle the GCSA2 start/stop characters.
        case '#': return '$'; break;
        case '$': return '#'; break;
        default: return 'N';
    }
}

string reverse_complement(const string& seq) {
    string rc;
    rc.assign(seq.rbegin(), seq.rend());
    for (auto& c : rc) {
        switch (c) {
        case 'A': c = 'T'; break;
        case 'T': c = 'A'; break;
        case 'G': c = 'C'; break;
        case 'C': c = 'G'; break;
        case 'N': c = 'N'; break;
        // Handle the GCSA2 start/stop characters.
        case '#': c = '$'; break;
        case '$': c = '#'; break;
        default: break;
        }
    }
    return rc;
}

int get_thread_count(void) {
    int thread_count = 1;
#pragma omp parallel
    {
#pragma omp master
        thread_count = omp_get_num_threads();
    }
    return thread_count;
}

std::vector<std::string> &split_delims(const std::string &s, const std::string& delims, std::vector<std::string> &elems) {
    char* tok;
    char cchars [s.size()+1];
    char* cstr = &cchars[0];
    strcpy(cstr, s.c_str());
    tok = strtok(cstr, delims.c_str());
    while (tok != NULL) {
        elems.push_back(tok);
        tok = strtok(NULL, delims.c_str());
    }
    return elems;
}
std::vector<std::string> split_delims(const std::string &s, const std::string& delims) {
    std::vector<std::string> elems;
    return split_delims(s, delims, elems);
}

const std::string sha1sum(const std::string& data) {
    SHA1 checksum;
    checksum.update(data);
    return checksum.final();
}

const std::string sha1head(const std::string& data, size_t head) {
    return sha1sum(data).substr(0, head);
}

string wrap_text(const string& str, size_t width) {
    stringstream w;
    size_t j = 0;
    for (auto c : str) {
        if (j++ > 50) {
            if (c == ' ') {
                w << "\n";
                j = 0;
            } else {
                w << c;
            }
        } else {
            w << c;
        }
    }
    return w.str();
}

bool allATGC(string& s) {
    for (string::iterator c = s.begin(); c != s.end(); ++c) {
        char b = *c;
        if (b != 'A' && b != 'T' && b != 'G' && b != 'C') {
            return false;
        }
    }
    return true;
}

void mapping_cigar(const Mapping& mapping, vector<pair<int, char> >& cigar) {
    for (const auto& edit : mapping.edit()) {
        if (edit.from_length() && edit.from_length() == edit.to_length()) {
// *matches* from_length == to_length, or from_length > 0 and offset unset
            // match state
            cigar.push_back(make_pair(edit.from_length(), 'M'));
        } else {
            // mismatch/sub state
// *snps* from_length == to_length; sequence = alt
            if (edit.from_length() == edit.to_length()) {
                cigar.push_back(make_pair(edit.from_length(), 'M'));
            } else if (edit.from_length() == 0 && edit.sequence().empty()) {
// *skip* from_length == 0, to_length > 0; implies "soft clip" or sequence skip
                cigar.push_back(make_pair(edit.to_length(), 'S'));
            } else if (edit.from_length() > edit.to_length()) {
// *deletions* from_length > to_length; sequence may be unset or empty
                int32_t del = edit.from_length() - edit.to_length();
                int32_t eq = edit.to_length();
                if (eq) cigar.push_back(make_pair(eq, 'M'));
                cigar.push_back(make_pair(del, 'D'));
            } else if (edit.from_length() < edit.to_length()) {
// *insertions* from_length < to_length; sequence contains relative insertion
                int32_t ins = edit.to_length() - edit.from_length();
                int32_t eq = edit.from_length();
                if (eq) cigar.push_back(make_pair(eq, 'M'));
                cigar.push_back(make_pair(ins, 'I'));
            }
        }
    }
}

string mapping_string(const string& source, const Mapping& mapping) {
    string result;
    int p = mapping.position().offset();
    for (const auto& edit : mapping.edit()) {
        // mismatch/sub state
// *matches* from_length == to_length, or from_length > 0 and offset unset
// *snps* from_length == to_length; sequence = alt
        // mismatch/sub state
        if (edit.from_length() == edit.to_length()) {
            if (!edit.sequence().empty()) {
                result += edit.sequence();
            } else {
                result += source.substr(p, edit.from_length());
            }
            p += edit.from_length();
        } else if (edit.from_length() == 0 && edit.sequence().empty()) {
// *skip* from_length == 0, to_length > 0; implies "soft clip" or sequence skip
            //cigar.push_back(make_pair(edit.to_length(), 'S'));
        } else if (edit.from_length() > edit.to_length()) {
// *deletions* from_length > to_length; sequence may be unset or empty
            result += edit.sequence();
            p += edit.from_length();
        } else if (edit.from_length() < edit.to_length()) {
// *insertions* from_length < to_length; sequence contains relative insertion
            result += edit.sequence();
            p += edit.from_length();
        }
    }
    return result;
}

string cigar_string(vector<pair<int, char> >& cigar) {
    vector<pair<int, char> > cigar_comp;
    pair<int, char> cur = make_pair(0, '\0');
    for (auto& e : cigar) {
        if (cur == make_pair(0, '\0')) {
            cur = e;
        } else {
            if (cur.second == e.second) {
                cur.first += e.first;
            } else {
                cigar_comp.push_back(cur);
                cur = e;
            }
        }
    }
    cigar_comp.push_back(cur);
    stringstream cigarss;
    for (auto& e : cigar_comp) {
        cigarss << e.first << e.second;
    }
    return cigarss.str();
}

// Demangle the name in thsi stack trace frame if we can find the API to do so.
string demangle_frame(string mangled) {
    // Demangle the name in a stack trace line as seen at
    // <http://panthema.net/2008/0901-stacktrace-demangled/>
    
    // Name is module(function+offset) [address] in standard format.
    // For example: 
    // ../createIndex/createIndex(_Z12make_tempdirv+0x1a4) [0x46e8f4]
    // We need to find the start and end of the function part. Sometimes
    // the parens can just be empty, so we need to handle that too.
    
    // Where is the close paren, reading from the right?
    size_t closeParen = 0;
    // Where is the plus, reading from the right? If there is no plus in
    // the parens, set to 0.
    size_t plus = 0;
    // Where is the open paren, reading from right to left?
    size_t openParen = 0;
    
    for(size_t j = mangled.size() - 1; j != (size_t) -1; j--) {
        // Scan from right to left.
        
        if(closeParen == 0 && mangled[j] == ')') {
            // We found the rightmost close paren
            closeParen = j;
        } else if(j < closeParen && plus == 0 && mangled[j] == '+') {
            // We found the + to the left of the close paren.
            plus = j;
        } else if(j < closeParen && openParen == 0 && mangled[j] == '(') {
            // We found the open paren to the left of the close paren.
            openParen = j;
            
            // We're done parsing.
            break;
        }
    }
    
    if(openParen == 0 || closeParen == 0 || plus == 0) {
        // We couldn't pull out a name and address. Either we have a
        // nonstandard format or we have empty parens.
        
        // Just use the default trace message
        return mangled;
    } else {
        // We did parse out stuff!
        
        // Take everything before the open paren.
        string demangled = mangled.substr(0, openParen + 1);
        
        // Grab the function name
        string functionName = mangled.substr(openParen + 1, plus - (openParen + 1));
        
        // Make a place for the demangling function to save its status
        int status;
        
        // Do the demangling
        char* demangledName = abi::__cxa_demangle(functionName.c_str(), NULL, NULL, &status);
        
        if(status != 0) {
            // If we couldn't demangle the name, just use the mangled name.
            return mangled;
        }
        
        // Add the (probably) demangled name, a "+", and the rest of the
        // message.
        demangled += string(demangledName) + "+" + mangled.substr(plus + 1);
        
        if(status == 0) {
            // We got a demangled name we need to clean up.
            free(demangledName);
        }
        
        return demangled;
    }
}

void emit_stacktrace() {
    // How many frames can we handle?
    const size_t MAX_FRAMES = 100;
    
    // This holds the stack frames
    void *frames[MAX_FRAMES];
    
    // And this holds how many there actually are, which comes out of the
    // function that gets the frames.
    size_t framesUsed = backtrace(frames, MAX_FRAMES);
    
    cerr << "Stack trace:" << endl;
        
    char** traceMessages = backtrace_symbols(frames, framesUsed);
    
    for(size_t i = 0; i < framesUsed; i++) {
        // Print a demangled version of every frame            
        cerr << demangle_frame(traceMessages[i]) << endl;
        // Separate frames because damangled can be long.
        cerr << "=================" << endl;
    }
    
    // Free our stacktrace memory.
    free(traceMessages);
    
}

}
