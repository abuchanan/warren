#ifndef WARREN_FASTAREADER_H
#define WARREN_FASTAREADER_H

#include <istream>
#include <fstream>
#include <string>

#include "warren/Fasta.h"

using std::istream;
using std::string;

class FastaReader
{
    istream& input;

    public:
        typedef Fasta Record;

        FastaReader (istream& in) : input(in) {};

        bool read (Fasta& record)
        {
            Fasta temp;
            int c;

            // The following is a DFA for parsing a FASTA record
            enum State
            {
                Init,
                Header,
                Seq
            };
            State state = Init;

            c = input.get();
            while (input.good())
            {
                if (state == Init && c == '>') state = Header;

                else if (state == Header)
                {
                    if (c == '\n') state = Seq;
                    else temp.header += c;
                }

                else if (state == Seq)
                {
                    if (c == '>') 
                    {
                        input.unget();
                        break;
                    }
                    else if (c != '\n') temp.sequence += c;
                }
                c = input.get();
            }

            if (state == Seq)
            {
                record = temp;
                return true;
            }

            return false;
        }
};

class FastaPairReader
{
    FastaReader reader;

    public:
        typedef FastaPair Record;

        FastaPairReader (istream& in) : reader(in) {};

        bool read (FastaPair& record)
        {
            Fasta a;
            Fasta b;
            if (reader.read(a) && reader.read(b))
            {
                record.a = a;
                record.b = b;
                return true;
            }
            return false;
        }
};

#endif
