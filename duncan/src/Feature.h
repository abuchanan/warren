#ifndef _GFF_FEATURE_H
#define _GFF_FEATURE_H

#include <string>;
#include <vector>;

#include "Attribute.h";


using std::string;

namespace GFF {

    class Feature {
        public:
            string seqid;
            string source;
            string type;
            unsigned int start;
            unsigned int end;
            string score;
            char strand;
            char phase;
            std::vector<GFF::Attribute> attributes;

            bool hasStrand( void );
            bool isRevStrand( void );
            bool hasScore( void );
            double getScore( void );
            int getLength( void );
            string toString( void );
            bool fromString( string );

            string attributesToString( void );
    };
}

#endif
