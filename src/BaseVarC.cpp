#include <getopt.h>
#include <iterator>
#include <fstream>
#include <iostream>

#include "FastaReader.h"
#include "BamProcess.h"

static const char* BASEVARC_USAGE_MESSAGE = 
"Program: BaseVarC\n"
"Contact: Zilong Li [lizilong@bgi.com]\n"
"Usage  : BaseVarC <command> [options]\n\n"
"Commands:\n"
"           basetype       Variants Caller\n"
"           popmatrix      Create population matrix at specific positions.\n" 
"\nReport bugs to lizilong@bgi.com \n\n";

static const char* BASETYPE_MESSAGE = 
"Program: BaseVarC basetype\n"
"Contact: Zilong Li [lizilong@bgi.com]\n"
"Usage  : BaseVarC basetype [options]\n\n";

static const char* POPMATRIX_MESSAGE = 
"Program: BaseVarC popmatrix\n"
"Contact: Zilong Li [lizilong@bgi.com]\n"
"Usage  : BaseVarC popmatrix [options]\n\n"
"Commands:\n"
"  --bamlist,    -l        BAM/CRAM files list, one file per row.\n"
"  --posfile,    -p        Position file <CHRID POS REF ALT>\n"
"  --output,     -o        Output path(default stdout)\n"
"  --mapq,       -q <INT>  Mapping quality >= INT. [10]\n"
"  --verbose,    -v        Set verbose output\n"
"\nReport bugs to lizilong@bgi.com \n\n";

void runBaseType(int argc, char **argv);
void runPopMatrix(int argc, char **argv);
void subPopMatrix (const std::vector<std::string>& bams, const PosInfoVector& pv);
void parseOptions(int argc, char **argv, const char* msg);

namespace opt {
    static bool verbose = false;
    static int mapq;
    static std::string bamlst;
    static std::string reference;
    static std::string posfile;
    static std::string region;
    static std::string output;
}

static const char* shortopts = "hv:q:l:r:p:g:o";

static const struct option longopts[] = {
  { "help",                    no_argument, NULL, 'h' },
  { "verbose",                 no_argument, NULL, 'v' },
  { "mapq",                    required_argument, NULL, 'q' },
  { "bamlst",                  required_argument, NULL, 'l' },
  { "reference",               required_argument, NULL, 'r' },
  { "posfile",                 required_argument, NULL, 'p' },
  { "region",                  required_argument, NULL, 'g' },
  { "output",                  required_argument, NULL, 'o' },
  { NULL, 0, NULL, 0 }
};

int main(int argc, char** argv)
{
    if (argc <= 1 ) {
	std::cerr << BASEVARC_USAGE_MESSAGE;
	return 0;
    } else {
	std::string command(argv[1]);
	if (command == "help" || command == "--help") {
	    std::cerr << BASEVARC_USAGE_MESSAGE;
	    return 0;
	} else if (command == "basetype") {
	    runBaseType(argc - 1, argv + 1);
	} else if (command == "popmatrix") {
	    runPopMatrix(argc - 1, argv + 1);
	} else {
	    std::cerr << BASEVARC_USAGE_MESSAGE;
	    return 0;
	}
    }

    return 0;
}

void runBaseType(int argc, char **argv)
{
    parseOptions(argc, argv, BASETYPE_MESSAGE);
    std::cerr << "basetype start" << std::endl;
    //std::ifstream ibam(opt::bamlst);
    //std::vector<std::string> bams(std::istream_iterator<Line>{ibam},
    //	                          std::istream_iterator<Line>{});
    FastaReader fa;
    fa.GetTargetBase(opt::region, opt::reference);
}

void runPopMatrix (int argc, char **argv)
{
    parseOptions(argc, argv, POPMATRIX_MESSAGE);
    std::cerr << "popmatrix start" << std::endl;
    std::ifstream ibam(opt::bamlst);
    std::ifstream ipos(opt::posfile);
    if (!ibam.is_open() || !ipos.is_open()) {
	std::cerr << "bamlist or posifle can not be opend" << std::endl;
	exit(EXIT_FAILURE);
    }
    std::vector<std::string> bams(std::istream_iterator<Line>{ibam},
    	                          std::istream_iterator<Line>{});
    PosInfoVector pv;
    for (PosInfo p; ipos >> p;) pv.push_back(p);
    pv.shrink_to_fit();        // request for the excess capacity to be released

    std::string sep = "\t";
    std::ostringstream tmp;
    for (auto const& p: pv) {
	tmp << p.chr << sep;
    }
    std::cout << tmp.str() << std::endl;
    tmp.str("");
    for (auto const& p: pv) {
	tmp << p.pos << sep;
    }
    std::cout << tmp.str() << std::endl;
    tmp.str("");
    for (auto const& p: pv) {
	tmp << p.ref << sep;
    }
    std::cout << tmp.str() << std::endl;
    tmp.str("");
    for (auto const& p: pv) {
	tmp << p.alt << sep;
    }
    std::cout << tmp.str() << std::endl;
    tmp.str("");

    subPopMatrix(bams, pv);
    std::cerr << "popmatrix done" << std::endl;
}

void subPopMatrix (const std::vector<std::string>& bams, const PosInfoVector& pv)
{
    const int32_t N = bams.size();
    std::string rg = pv.front() + pv.back();

    uint32_t count = 0;
    for (int32_t i = 0; i < N; ++i) {
	BamProcess reader;
	if (!(++count % 1000)) std::cerr << "Processing the number " << count / 1000 << "k bam" << std::endl;
	if (!reader.Open(bams[i])) {
	    std::cerr << "ERROR: could not open file " << bams[i] << std::endl;
	    exit(EXIT_FAILURE);
	}
	SeqLib::GenomicRegion gr(rg, reader.Header());
	// bam is 0-based and rg pos is 1-based 
	gr.Pad(1000);
	reader.FindSnpAtPos(gr, pv);
	reader.PrintOut();
	if (!reader.Close()) {
	    std::cerr << "Warning: could not close file " << bams[i] << std::endl;
	}
    }
    
}

void parseOptions(int argc, char **argv, const char* msg)
{
    bool die = false;
    bool help = false;
    for (char c; (c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1;) {
	std::istringstream arg(optarg != NULL ? optarg : "");
	switch (c) {
	case 'v': opt::verbose = true; break;
	case 'q': arg >> opt::mapq; break;
	case 'l': arg >> opt::bamlst; break;
	case 'r': arg >> opt::reference; break;
	case 'p': arg >> opt::posfile; break;
	case 'g': arg >> opt::region; break;
	case 'o': arg >> opt::output; break;
	default: die = true;
	}
    }
    if (die || help) {
	std::cerr << msg;
	if (die)
	  exit(EXIT_FAILURE);
	else
	  exit(EXIT_SUCCESS);
    }
}