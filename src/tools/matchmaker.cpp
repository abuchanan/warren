#include <algorithm>
#include <iostream>
#include <map>
#include <locale>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "bamtools/api/BamWriter.h"

#include "tclap/CmdLine.h"

#include "warren/BamMultiReader.h"
#include "warren/Feature.h"
#include "warren/Splat.h"

#define VERSION "0.1"

#define DEFAULT_MIN_GAP 100
#define DEFAULT_MAX_GAP 10000

using std::cerr;
using std::endl;
using std::map;
using std::set;
using std::string;
using std::stringstream;
using std::vector;

using BamTools::BamWriter;

struct GroupKey
{
    string refID;
    char mateID;
    bool rev;

    bool operator< (const GroupKey &other) const
    {
        return refID < other.refID 
               || ( refID == other.refID && mateID < other.mateID )
               || ( refID == other.refID && mateID == other.mateID && rev < other.rev );
    }
};

typedef std::multimap<GroupKey, Alignment> Group;
typedef std::pair<Group::iterator, Group::iterator> GroupRange;

int MIN_GAP;
int MAX_GAP;

// global Bam writers for convenience
BamWriter ValidOut;

void processGroupRange (GroupRange& range_a, GroupRange& range_b, int& valid_count);

void processGroup (Group& group, set<string>& refs);

void parseID (string& id, string& groupID, char& mateID);

bool isValidPair(Alignment& a, Alignment& b);

void initMate(Alignment&a, Alignment& x);

void makePair(Alignment& a, Alignment& b, Alignment& x, Alignment& y);

int main (int argc, char * argv[])
{
    vector<string> inputFilenames;
    string combinedOutFilename, alignmentsOutFilename;

    try
    {
        TCLAP::CmdLine cmd("Program description", ' ', VERSION);

        TCLAP::ValueArg<string> combinedOutputArg("o", "out", 
            "Combined output filename (BAM format)", true, "", "combined.bam", cmd);

        TCLAP::ValueArg<int> minInsertArg("n", "min-insert", 
            "Minimum insert size", false, DEFAULT_MIN_GAP, "min insert size", cmd);

        TCLAP::ValueArg<int> maxInsertArg("x", "max-insert", 
            "Maximum insert size", false, DEFAULT_MAX_GAP, "max insert size", cmd);

        TCLAP::MultiArg<string> inputArgs("b", "bam",
            "Input BAM file", true,
            "input.bam", cmd);

        cmd.parse(argc, argv);

        combinedOutFilename = combinedOutputArg.getValue();

        MIN_GAP = minInsertArg.getValue();
        MAX_GAP = maxInsertArg.getValue();
        inputFilenames = inputArgs.getValue();

    } catch (TCLAP::ArgException &e) {
        cerr << "Error: " << e.error() << " " << e.argId() << endl;
    }

    // TODO require that alignments are sorted by name

    BamMultiReader reader;
    reader.Open(inputFilenames);

    if (!ValidOut.Open(combinedOutFilename, reader.GetHeader(),
                       reader.GetReferenceData()))
    {
        cerr << ValidOut.GetErrorString() << endl;
        return 1;
    }

    string current, prev;
    char mateID;
    Group group;
    set<string> references;

    Alignment a;
    while (reader.GetNextAlignment(a))
    {
        parseID(a.Name, current, mateID);

        if (current.compare(prev) && prev.size() > 0)
        {
            processGroup(group, references);
            group.clear();
            references.clear();
        }

        references.insert(a.RefName);

        GroupKey key;
        key.refID = a.RefName;
        key.mateID = mateID;
        key.rev = a.IsReverseStrand();

        group.insert( std::make_pair( key, a ) );

        prev = current;
    }
    processGroup(group, references);
}

void parseID (string& id, string& groupID, char& mateID)
{
    groupID.clear();

    unsigned int i = 0;
    while (i < id.size()) {
        if (id.at(i) != ' ' && id.at(i) != '/') groupID += id.at(i);
        else {
            mateID = (i + 1 < id.size()) ? id.at(i + 1) : '0';
            i = id.size();
        }
        i++;
    }
}

bool isValidPair(Alignment& a, Alignment& b)
{
    Feature spacer;
    getSpacer(a, b, spacer);
    int gap = spacer.getLength();

    // Calculate the correct size of the insert
    // REMEMBER: negative gaps mean our values overlap

    // pair must be oriented correctly
    // (5'--------->3' 3'<---------5')
    return gap >= MIN_GAP && gap <= MAX_GAP 
           && !a.IsReverseStrand() && b.IsReverseStrand();
}

void initMate(Alignment&a, Alignment& x)
{
    x = a;  // TODO unit test this
    x.AddTag("XN", "Z", x.Name);

    string baseName;
    char ignore;

    parseID(x.Name, baseName, ignore);
    x.Name = baseName;

    x.Qualities.clear();
    x.SetIsPaired(true);
    x.SetIsMapped(true);
    x.SetIsMateMapped(true);
    x.SetIsSecondMate(true);
    x.SetIsProperPair(true);

    //Add Splat tag data
    if (isSplat(a))
    {
        string flanks;
        a.GetTag("XD", flanks);
        x.AddTag("XD", "Z", flanks);
    }
}

void makePair(Alignment& a, Alignment& b, Alignment& x, Alignment& y)
{
    initMate(a, x);
    initMate(b, y);

    Feature spacer;
    getSpacer(a, b, spacer);
    int insert = a.Length + spacer.getLength() + b.Length;

    // Add in mate pair info
    x.MateRefID = b.RefID;
    x.matePosition(b.position());
    x.InsertSize = insert;

    y.MateRefID = a.RefID;
    y.matePosition(a.position());
    y.InsertSize = -1 * insert;

    // Update Mapping information appropriately
    x.SetIsReverseStrand(a.IsReverseStrand());
    x.SetIsMateReverseStrand(b.IsReverseStrand());

    y.SetIsReverseStrand(b.IsReverseStrand());
    y.SetIsMateReverseStrand(a.IsReverseStrand());
}

void processGroupRange (GroupRange& range_a, GroupRange& range_b, int& valid_count)
{
    Group::iterator a_it;
    Group::iterator b_it;

    for (a_it = range_a.first; a_it != range_a.second; ++a_it)
    {
        for (b_it = range_b.first; b_it != range_b.second; ++b_it)
        {
            Alignment a = a_it->second;
            Alignment b = b_it->second;

            // ensure 'a' is the most 5' alignment
            if (b.position() < a.position())
            {
                std::swap(a, b);
            }

            if (isValidPair(a, b))
            {
                std::stringstream count_str;
                count_str << valid_count;
                Alignment x, y;
                makePair(a, b, x, y);
                x.Name += "-" + count_str.str();
                y.Name += "-" + count_str.str();
 
                ValidOut.SaveAlignment(x);
                ValidOut.SaveAlignment(y);

                valid_count++;
            }
        }
    }
}

void processGroup (Group& group, set<string>& refs)
{
    int valid_count = 1;
    GroupKey key_a;
    GroupKey key_b;
    GroupRange range_a;
    GroupRange range_b;

    for (set<string>::iterator reference = refs.begin();
         reference != refs.end(); ++reference )
    {
        key_a.refID = *reference;
        key_b.refID = *reference;

        key_a.mateID = '1';
        key_b.mateID = '2';

        key_a.rev = true;
        key_b.rev = false;

        range_a = group.equal_range(key_a);
        range_b = group.equal_range(key_b);

        processGroupRange(range_a, range_b, valid_count);

        key_a.rev = false;
        key_b.rev = true;

        range_a = group.equal_range(key_a);
        range_b = group.equal_range(key_b);

        processGroupRange(range_a, range_b, valid_count);
    }
}
