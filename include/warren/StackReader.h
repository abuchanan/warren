#ifndef WARREN_STACKREADER_H
#define WARREN_STACKREADER_H

#include <istream>
#include <fstream>
#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include "warren/Feature.h"
#include "warren/tokenizer.h"

using std::istream;
using std::string;

class StackReader
{
    istream& input;

    public:
        StackReader ();
        StackReader (istream& in) : input(in) {};

        bool open (const char* file_path)
        {
            std::ifstream in(file_path);
            return in.good();
        }

        bool open (string file_path)
        {
            return open(file_path.c_str());
        }

        bool read (Feature& f)
        {
            string line;

            while (std::getline(input, line).good())
            {
                if (boost::starts_with(line, "@"))
                {
                    std::vector<string> cols;
                    tokenizer tokens(line, separator("\t"));
                    cols.insert(cols.end(), tokens.begin(), tokens.end());

                    f.seqid = cols.at(0).substr(1);            
                    f.type = "splice_junction";
                    f.start = boost::lexical_cast<int>(cols.at(2));
                    f.end = boost::lexical_cast<int>(cols.at(3));

                    return true;
                }
            }
            return false;
        }
};

#endif
