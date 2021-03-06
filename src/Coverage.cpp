#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "bamtools/api/BamAux.h"

#include "warren/Alignment.h"
#include "warren/Coverage.h"

using std::endl;
using std::vector;
using std::string;

int Coverage::get (string ref_name, int pos)
{
    return coverages.find(ref_name)->second.at(pos - 1);
}

void Coverage::set (string ref_name, int pos, int value)
{
    coverages.find(ref_name)->second.at(pos - 1) = value;
}

void Coverage::increment (string ref_name, int pos, int value)
{
    set(ref_name, pos, get(ref_name, pos) + value);
}

void Coverage::setMinReferenceLength (string ref_name, int length)
{
    coverages_t::iterator coverage = coverages.find(ref_name);
    if (coverage == coverages.end())
    {
        coverages.insert(make_pair(ref_name, vector<int>(length, 0)));
    }
    else if (coverage->second.size() < length)
    {
        coverage->second.resize(length, 0);
    }
}

void Coverage::add (Alignment& alignment)
{
    int pos = alignment.position();
    for (vector<CigarOp>::iterator op = alignment.CigarData.begin(); 
         op != alignment.CigarData.end(); ++op)
    {
        if (op->Type == 'M')
        {
            add(alignment.RefName, pos, (int)op->Length);
        }
        pos += op->Length;
    }
}

void Coverage::add (string ref_name, int start, int length)
{
    setMinReferenceLength(ref_name, start + length - 1);

    for (int i = start; i < start + length; ++i)
    {
        increment(ref_name, i);
    }
}

void Coverage::load (std::istream& input)
{
    string line;
    string ref_name;
    int i = 1;

    while (std::getline(input, line).good())
    {
        size_t pos = line.find("\t");
        if (pos != string::npos)
        {
            i = 1;
            ref_name = line.substr(0, pos);
            int length = atoi(line.substr(pos + 1).c_str());
            setMinReferenceLength(ref_name, length);
        }
        else if (!line.empty())
        {
            set(ref_name, i, atoi(line.c_str()));
            ++i;
        }
    }
}

void Coverage::toOutputStream (std::ostream& output)
{
    for (coverages_t::iterator it = coverages.begin(); it != coverages.end(); ++it)
    {
        output << it->first << "\t" << it->second.size() << std::endl;
        for (int i = 0; i < it->second.size(); ++i)
        {
            output << it->second.at(i) << std::endl;
        }
    }
}

void Coverage::toString (string& output)
{
    std::stringstream stream;
    toOutputStream(stream);
    output = stream.str();
}
