/*******************************************************************************
 Copyright 2006-2012 Lukas Käll <lukas.kall@scilifelab.se>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 *******************************************************************************/

#include "Caller.h"
#include "Version.h"
#ifndef _WIN32
  #include "unistd.h"
#endif
#include <iomanip>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

Caller::Caller() :
    pNorm_(NULL), pCheck_(NULL), protEstimator_(NULL), tabInput_(true), oswInput_(true), 
    oswLevel_("MS2"), readStdIn_(false), inputFN_(""), xmlSchemaValidation_(true), 
    tabOutputFN_(""), xmlOutputFN_(""), weightOutputFN_(""),
    psmResultFN_(""), peptideResultFN_(""), proteinResultFN_(""), 
    decoyPsmResultFN_(""), decoyPeptideResultFN_(""), decoyProteinResultFN_(""),
    xmlPrintDecoys_(false), xmlPrintExpMass_(true), reportUniquePeptides_(true), 
    targetDecoyCompetition_(false), useMixMax_(false), inputSearchType_("auto"),
    selectionFdr_(0.01), testFdr_(0.01), numIterations_(10), maxPSMs_(0u),
    selectedCpos_(0.0), selectedCneg_(0.0),
    reportEachIteration_(false), quickValidation_(false) {
}

Caller::~Caller() {
  if (pNorm_) {
    delete pNorm_;
  }
  pNorm_ = NULL;
  if (pCheck_) {
    delete pCheck_;
  }
  pCheck_ = NULL;
  if (protEstimator_) {
    delete protEstimator_;
  }
  protEstimator_ = NULL;
}

string Caller::extendedGreeter(time_t& startTime) {
  ostringstream oss;
  char* host = getenv("HOSTNAME");
  oss << greeter();
  oss << "Issued command:" << endl << call_ << endl;
  oss << "Started " << ctime(&startTime) << endl;
  oss.seekp(-1, ios_base::cur);
  if (host) oss << " on " << host << endl;
  oss << "Hyperparameters: selectionFdr=" << selectionFdr_
      << ", Cpos=" << selectedCpos_ << ", Cneg=" << selectedCneg_
      << ", maxNiter=" << numIterations_ << endl;
  return oss.str();
}

string Caller::greeter() {
  ostringstream oss;
  oss << "Percolator version " << VERSION << ", ";
  oss << "Build Date " << __DATE__ << " " << __TIME__ << endl;
  oss << "Copyright (c) 2006-9 University of Washington. All rights reserved.\n"
      << "Written by Lukas Käll (lukall@u.washington.edu) in the\n"
      << "Department of Genome Sciences at the University of Washington.\n";
  return oss.str();
}

bool Caller::parseOptions(int argc, char **argv) {
  ostringstream callStream;
  callStream << argv[0];
  for (int i = 1; i < argc; i++) {
    callStream << " " << argv[i];
  }
  callStream << endl;
  call_ = callStream.str();
  call_ = call_.substr(0,call_.length()-1); // trim ending carriage return
  ostringstream intro, endnote;
  intro << greeter() << "\nUsage:\n";
  intro << "   percolator [-X pout.xml] [other options] pin.tsv\n";
  intro << "pin.tsv is the tab delimited output file generated by e.g. sqt2pin;\n";
  intro << "  The tab delimited fields should be:\n";
  intro << "    id <tab> label <tab> scannr <tab> feature1 <tab> ... <tab>\n";
  intro << "    featureN <tab> peptide <tab> proteinId1 <tab> .. <tab> proteinIdM\n";
  intro << "  Labels are interpreted as 1 -- positive set and test set, -1 -- negative set.\n";
  intro << "  When the --doc option the first and second feature should contain\n";
  intro << "  the retention time and difference between observed and calculated mass;\n";
  intro << "pout.xml is where the output will be written (ensure to have read\n";
  intro << "and write access on the file)." << std::endl;
  // init
  CommandLineParser cmd(intro.str());
  // available lower case letters:
  // available upper case letters:
  cmd.defineOption("X",
      "xmloutput",
      "Path to xml-output (pout) file.",
      "filename");
  cmd.defineOption("",
      "stdinput",
      "Read percolator tab-input format (pin-tab) from standard input",
      "",
      TRUE_IF_SET);
  cmd.defineOption("e",
      "stdinput-xml",
      "Read percolator xml-input format (pin-xml) from standard input",
      "",
      TRUE_IF_SET);
  cmd.defineOption("Z",
      "decoy-xml-output",
      "Include decoys (PSMs, peptides and/or proteins) in the xml-output. Only available if -X is set.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("p",
      "Cpos",
      "Cpos, penalty for mistakes made on positive examples. Set by cross validation if not specified.",
      "value");
  cmd.defineOption("n",
      "Cneg",
      "Cneg, penalty for mistakes made on negative examples. Set by cross validation if not specified or if -p is not specified.",
      "value");
  cmd.defineOption("t",
      "testFDR",
      "False discovery rate threshold for evaluating best cross validation result and reported end result. Default = 0.01.",
      "value");
  cmd.defineOption("F",
      "trainFDR",
      "False discovery rate threshold to define positive examples in training. Set to testFDR if 0. Default = 0.01.",
      "value"); 
  cmd.defineOption("i",
      "maxiter",
      "Maximal number of iterations. Default = 10.",
      "number");
  cmd.defineOption("N",
      "subset-max-train",
      "Only train an SVM on a subset of <x> PSMs, and use the resulting score vector to evaluate the other PSMs. Recommended when analyzing huge numbers (>1 million) of PSMs. When set to 0, all PSMs are used for training as normal. Default = 0.",
      "number");
  cmd.defineOption("x",
      "quick-validation",
      "Quicker execution by reduced internal cross-validation.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("J",
      "tab-out",
      "Output computed features to given file in pin-tab format.",
      "filename");
  cmd.defineOption("j",
      "tab-in [default]",
      "Input file given in pin-tab format. This is the default setting, flag only present for backwards compatibility.",
      "filename");
  cmd.defineOption("OI",
      "osw-in",
      "Input file given in OpenSWATH OSW format.",
      "filename");
  cmd.defineOption("OL",
      "osw-level [default: MS2]",
      "Data-level (MS1 [MS1], MS2 [MS2] or Transitions [T]) for OpenSWATH.",
      "level");
  cmd.defineOption("k",
      "xml-in",
      "Input file given in deprecated pin-xml format generated by e.g. sqt2pin with the -k option",
      "filename");
  cmd.defineOption("w",
      "weights",
      "Output final weights to given file",
      "filename");
  cmd.defineOption("W",
      "init-weights",
      "Read initial weights from given file (one per line)",
      "filename");
  cmd.defineOption("V",
      "default-direction",
      "Use given feature name as initial search direction, can be negated to indicate that a lower value is better.",
      "[-]?featureName");
  cmd.defineOption("v",
      "verbose",
      "Set verbosity of output: 0=no processing info, 5=all. Default = 2",
      "level");
  cmd.defineOption("o",
      "no-terminate",
      "Do not stop execution when encountering questionable SVM inputs or results.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("u",
      "unitnorm",
      "Use unit normalization [0-1] instead of standard deviation normalization",
      "",
      TRUE_IF_SET);
  cmd.defineOption("R",
      "test-each-iteration",
      "Measure performance on test set each iteration",
      "",
      TRUE_IF_SET);
  cmd.defineOption("O",
      "override",
      "Override error check and do not fall back on default score vector in case of suspect score vector from SVM.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("S",
      "seed",
      "Set seed of the random number generator. Default = 1",
      "value");
  cmd.defineOption("D",
      "doc",
      "Include description of correct features, i.e. features describing the difference between the observed and predicted isoelectric point, retention time and precursor mass.",
      "",
      MAYBE,
      "15");
  cmd.defineOption("K",
      "klammer",
      "Retention time features are calculated as in Klammer et al. Only available if -D is set.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("r",
      "results-peptides",
      "Output tab delimited results of peptides to a file instead of stdout (will be ignored if used with -U option)",
      "filename");
  cmd.defineOption("B",
      "decoy-results-peptides",
      "Output tab delimited results for decoy peptides into a file (will be ignored if used with -U option)",
      "filename");
  cmd.defineOption("m",
      "results-psms",
      "Output tab delimited results of PSMs to a file instead of stdout",
      "filename");
  cmd.defineOption("M",
      "decoy-results-psms",
      "Output tab delimited results for decoy PSMs into a file",
      "filename");
  cmd.defineOption("U",
      "only-psms",
      "Do not remove redundant peptides, keep all PSMS and exclude peptide level probabilities.",
      "",
      FALSE_IF_SET);
  cmd.defineOption("y",
      "post-processing-mix-max",
      "Use the mix-max method to assign q-values and PEPs. Note that this option only has an effect if the input PSMs are from separate target and decoy searches. This is the default setting.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("Y",
      "post-processing-tdc",
      "Replace the mix-max method by target-decoy competition for assigning q-values and PEPs. If the input PSMs are from separate target and decoy searches, Percolator's SVM scores will be used to eliminate the lower scoring target or decoy PSM(s) of each scan+expMass combination. If the input PSMs are detected to be coming from a concatenated search, this option will be turned on automatically, as this is incompatible with the mix-max method. In case this detection fails, turn this option on explicitly.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("I",
      "search-input",
      "Specify the type of target-decoy search: \"auto\" (Percolator attempts to detect the search type automatically), \"concatenated\" (single search on concatenated target-decoy protein db) or \"separate\" (two searches, one against target and one against decoy protein db). Default = \"auto\".",
      "value");
  cmd.defineOption("s",
      "no-schema-validation",
      "Skip validation of input file against xml schema.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("f",
      "picked-protein",
      "Use the picked protein-level FDR to infer protein probabilities. Provide the fasta file as the argument to this flag, which will be used for protein grouping based on an in-silico digest. If no fasta file is available or protein grouping is not desired, set this flag to \"auto\" to skip protein grouping.",
      "value");
  cmd.defineOption("A",
      "fido-protein",
      "Use the Fido algorithm to infer protein probabilities",
      "",
      TRUE_IF_SET);
  cmd.defineOption("l",
      "results-proteins",
      "Output tab delimited results of proteins to a file instead of stdout (Only valid if option -A or -f is active)",
      "filename");
  cmd.defineOption("L",
      "decoy-results-proteins",
      "Output tab delimited results for decoy proteins into a file (Only valid if option -A or -f is active)",
      "filename");
  cmd.defineOption("P",
      "protein-decoy-pattern",
      "Define the text pattern to identify decoy proteins in the database. Default = \"random_\".",
      "value");
  cmd.defineOption("z",
      "protein-enzyme",
      "Type of enzyme \"no_enzyme\",\"elastase\",\"pepsin\",\"proteinasek\",\"thermolysin\",\"trypsinp\",\"chymotrypsin\",\"lys-n\",\"lys-c\",\"arg-c\",\"asp-n\",\"glu-c\",\"trypsin\". Default=\"trypsin\".",
      "",
      "trypsin");
  /*cmd.defineOption("Q",
      "fisher-pval-cutoff",
      "The p-value cutoff for peptides when inferring proteins with fisher's method. Default = 1.0",
      "value");*/
  cmd.defineOption("c",
      "protein-report-fragments",
      "By default, if the peptides associated with protein A are a proper subset of the peptides associated with protein B, then protein A is eliminated and all the peptides are considered as evidence for protein B. Note that this filtering is done based on the complete set of peptides in the database, not based on the identified peptides in the search results. Alternatively, if this option is set and if all of the identified peptides associated with protein B are also associated with protein A, then Percolator will report a comma-separated list of protein IDs, where the full-length protein B is first in the list and the fragment protein A is listed second. Commas inside protein IDs will be replaced by semicolons. Not available for Fido.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("g",
      "protein-report-duplicates",
      "If this option is set and multiple database proteins contain exactly the same set of peptides, then the IDs of these duplicated proteins will be reported as a comma-separated list, instead of the default behavior of randomly discarding all but one of the proteins. Commas inside protein IDs will be replaced by semicolons. Not available for Fido.",
      "",
      TRUE_IF_SET);
  /*cmd.defineOption("I",
      "protein-absence-ratio",
      "The ratio of absent proteins, used for calculating protein-level q-values with a null hypothesis of \"Protein P is absent\". This uses the \"classic\" protein FDR in favor of the \"picked\" protein FDR.",
      "value"); // EXPERIMENTAL PHASE */
  cmd.defineOption("a",
      "fido-alpha",
      "Set Fido's probability with which a present protein emits an associated peptide. \
       Set by grid search if not specified.",
      "value");
  cmd.defineOption("b",
      "fido-beta",
      "Set Fido's probability of creation of a peptide from noise. Set by grid search if not specified.",
      "value");
  cmd.defineOption("G",
      "fido-gamma",
      "Set Fido's prior probability that a protein is present in the sample. Set by grid search if not specified.",
      "value");
  cmd.defineOption("q",
      "fido-empirical-protein-q",        
      "Output empirical p-values and q-values for Fido using target-decoy analysis to XML output (only valid if -X flag is present).",
      "",
      TRUE_IF_SET);
  cmd.defineOption("d",
      "fido-gridsearch-depth",
      "Setting the gridsearch-depth to 0 (fastest), 1 or 2 (slowest) controls how much computational time is required for the estimation of alpha, beta and gamma parameters for Fido. Default = 0.",
      "value");
  cmd.defineOption("T",
      "fido-fast-gridsearch",
      "Apply the specified threshold to PSM, peptide and protein probabilities to obtain a faster estimate of the alpha, beta and gamma parameters. Default = 0; Recommended when set = 0.2.",
      "value");
  cmd.defineOption("C",
      "fido-no-split-large-components",        
      "Do not approximate the posterior distribution by allowing large graph components to be split into subgraphs. The splitting is done by duplicating peptides with low probabilities. Splitting continues until the number of possible configurations of each subgraph is below 2^18.",
      "",
      TRUE_IF_SET);
  cmd.defineOption("E",
      "fido-protein-truncation-threshold",
      "To speed up inference, proteins for which none of the associated peptides has a probability exceeding the specified threshold will be assigned probability = 0. Default = 0.01.",
      "value");
  cmd.defineOption("H",
      "fido-gridsearch-mse-threshold",
      "Q-value threshold that will be used in the computation of the MSE and ROC AUC score in the grid search. Recommended 0.05 for normal size datasets and 0.1 for large datasets. Default = 0.1",
      "value");
  /*cmd.defineOption("Q",
      "fido-protein-group-level-inference",
      "Uses protein group level inference, each cluster of proteins is either present or not, therefore when grouping proteins discard all possible combinations for each group.(Only valid if option -A is active and -N is inactive).",
      "",
      TRUE_IF_SET);
  */
  
  // finally parse and handle return codes (display help etc...)
  cmd.parseArgs(argc, argv);
  
  if (cmd.optionSet("v")) {
    Globals::getInstance()->setVerbose(cmd.getInt("v", 0, 10));
  }
  
  if (cmd.optionSet("o")) {
    Globals::getInstance()->setNoTerminate(true);
  }
  
  // now query the parsing results
  if (cmd.optionSet("X")) xmlOutputFN_ = cmd.options["X"];
  
  // filenames for outputting results to file
  if (cmd.optionSet("m")) psmResultFN_ = cmd.options["m"];
  if (cmd.optionSet("M")) decoyPsmResultFN_ = cmd.options["M"];
  
  if (cmd.optionSet("U")) {
    // the different "hacks" below are mainly to keep backwards compatibility with old Mascot versions
    if (cmd.optionSet("A")){
      cerr
      << "ERROR: The -U option cannot be used in conjunction with -A: peptide level statistics\n"
      << "are needed to calculate protein level ones.";
      return 0;
    }
    reportUniquePeptides_ = false;
    
    if (cmd.optionSet("r")) {
      if (!cmd.optionSet("m")) {
        if (VERB > 0) {
          cerr
          << "WARNING: The -r option cannot be used in conjunction with -U: no peptide level statistics\n"
          << "are calculated, redirecting PSM level statistics to provided file instead." << endl;
        }
        psmResultFN_ = cmd.options["r"];
      } else {
        cerr
        << "WARNING: The -r option cannot be used in conjunction with -U: no peptide level statistics\n"
        << "are calculated, ignoring -r option." << endl;
      }
    }
    if (cmd.optionSet("B")) {
      if (!cmd.optionSet("M")) {
        if (VERB > 0) {
          cerr
          << "WARNING: The -B option cannot be used in conjunction with -U: no peptide level statistics\n"
          << "are calculated, redirecting decoy PSM level statistics to provided file instead." << endl;
        }
        decoyPsmResultFN_ = cmd.options["B"]; 
      } else {
        cerr
        << "WARNING: The -B option cannot be used in conjunction with -U: no peptide level statistics\n"
        << "are calculated, ignoring -B option." << endl;
      }
    }
  } else {
    if (cmd.optionSet("r")) peptideResultFN_ = cmd.options["r"];
    if (cmd.optionSet("B")) decoyPeptideResultFN_ = cmd.options["B"];
  }

  if (cmd.optionSet("A") || cmd.optionSet("f")) {
  
    ProteinProbEstimator::setCalcProteinLevelProb(true);
    
    // Confidence estimation options (general protein prob options)
    bool protEstimatorOutputEmpirQVal = false;
    bool protEstimatorTrivialGrouping = true; // cannot be set on cmd line
    std::string protEstimatorDecoyPrefix = "random_";
    double protEstimatorAbsenceRatio = 1.0;
    //if (cmd.optionSet("I")) protEstimatorAbsenceRatio = cmd.getDouble("I", 0.0, 1.0);
    protEstimatorOutputEmpirQVal = cmd.optionSet("q");
    if (cmd.optionSet("P")) protEstimatorDecoyPrefix = cmd.options["P"];
    //if (cmd.optionSet("Q")) protEstimatorTrivialGrouping = false;
    
    // Output file options
    if (cmd.optionSet("l")) proteinResultFN_ = cmd.options["l"];
    if (cmd.optionSet("L")) decoyProteinResultFN_ = cmd.options["L"];
    
    if (cmd.optionSet("A")) {
      /*fido parameters*/
      
      // General Fido options
      double fidoAlpha = -1;
      double fidoBeta = -1;
      double fidoGamma = -1;
      if (cmd.optionSet("a")) fidoAlpha = cmd.getDouble("a", 0.00, 1.0);
      if (cmd.optionSet("b")) fidoBeta = cmd.getDouble("b", 0.00, 1.0);
      if (cmd.optionSet("G")) fidoGamma = cmd.getDouble("G", 0.00, 1.0);
      
      // Options for controlling speed
      bool fidoNoPartitioning = false; // cannot be set on cmd line
      bool fidoNoClustering = false; // cannot be set on cmd line
      unsigned fidoGridSearchDepth = 0;
      bool fidoNoPruning = false;
      double fidoGridSearchThreshold = 0.0;
      double fidoProteinThreshold = 0.01;
      double fidoMseThreshold = 0.1;
      if (cmd.optionSet("d")) fidoGridSearchDepth = cmd.getInt("d", 0, 4);
      if (cmd.optionSet("T")) fidoGridSearchThreshold = cmd.getDouble("T", 0.0, 1.0);
      if (cmd.optionSet("C")) fidoNoPruning = true;
      if (cmd.optionSet("E")) fidoProteinThreshold = cmd.getDouble("E", 0.0, 1.0);
      if (cmd.optionSet("H")) fidoMseThreshold = cmd.getDouble("H",0.001,1.0);
      
      protEstimator_ = new FidoInterface(fidoAlpha, fidoBeta, fidoGamma, 
                fidoNoClustering, fidoNoPartitioning, fidoNoPruning,
                fidoGridSearchDepth, fidoGridSearchThreshold,
                fidoProteinThreshold, fidoMseThreshold,
                protEstimatorAbsenceRatio, protEstimatorOutputEmpirQVal, 
                protEstimatorDecoyPrefix, protEstimatorTrivialGrouping);
    } else if (cmd.optionSet("f")) {  
      std::string fastaDatabase = cmd.options["f"];
      
      // default options
      double pickedProteinPvalueCutoff = 1.0;
      bool pickedProteinReportFragmentProteins = false;
      bool pickedProteinReportDuplicateProteins = false;
      if (cmd.optionSet("z")) {
        Enzyme::setEnzyme(cmd.options["z"]);
      }      
      //if (cmd.optionSet("Q")) pickedProteinPvalueCutoff = cmd.getDouble("Q", 0.0, 1.0);
      if (cmd.optionSet("c")) pickedProteinReportFragmentProteins = true;
      if (cmd.optionSet("g")) pickedProteinReportDuplicateProteins = true;
      
      protEstimator_ = new PickedProteinInterface(fastaDatabase, pickedProteinPvalueCutoff,
          pickedProteinReportFragmentProteins, pickedProteinReportDuplicateProteins,
          protEstimatorTrivialGrouping, protEstimatorAbsenceRatio, 
          protEstimatorOutputEmpirQVal, protEstimatorDecoyPrefix);
    }
  }
  
  if (cmd.optionSet("k")) {
    tabInput_ = false;
    inputFN_ = cmd.options["k"];
  }

  if (cmd.optionSet("OI")) {
    tabInput_ = false;
    oswInput_ = true;
    reportUniquePeptides_ = false;
    inputSearchType_ = "separate";
    inputFN_ = cmd.options["OI"];
    oswLevel_ = cmd.options["OL"];
    // SanityCheck::setInitDefaultDirName("VAR_INTENSITY_SCORE");
  }
  
  if (cmd.optionSet("e")) {
    readStdIn_ = true;
    tabInput_ = false;
  }
  
  if (cmd.optionSet("j")) {
    tabInput_ = true;
    inputFN_ = cmd.options["j"];
  }
  
  if (cmd.optionSet("")) {
    readStdIn_ = true;
    tabInput_ = true;
  }
  
  if (cmd.optionSet("p")) {
    selectedCpos_ = cmd.getDouble("p", 0.0, 1e127);
  }
  if (cmd.optionSet("n")) {
    selectedCneg_ = cmd.getDouble("n", 0.0, 1e127);
    if (selectedCpos_ == 0) {
      std::cerr << "WARNING: the positive penalty(cpos) is 0, therefore both the "  
               << "positive and negative penalties are going "
               << "to be cross-validated. The option --Cneg has to be used together "
               << "with the option --Cpos" << std::endl;
    }
  }
  if (cmd.optionSet("J")) {
    tabOutputFN_ = cmd.options["J"];
  }
  
  if (cmd.optionSet("w")) {
    weightOutputFN_ = cmd.options["w"];
  }
  if (cmd.optionSet("W")) {
    SanityCheck::setInitWeightFN(cmd.options["W"]);
  }
  if (cmd.optionSet("V")) {
    SanityCheck::setInitDefaultDirName(cmd.options["V"]);
  }
  if (cmd.optionSet("u")) {
    Normalizer::setType(Normalizer::UNI);
  }
  if (cmd.optionSet("O")) {
    SanityCheck::setOverrule(true);
  }
  if (cmd.optionSet("R")) {
    reportEachIteration_ = true;
  }
  if (cmd.optionSet("x")) {
    quickValidation_ = true;
  }
  if (cmd.optionSet("F")) {
    selectionFdr_ = cmd.getDouble("F", 0.0, 1.0);
  }
  if (cmd.optionSet("t")) {
    testFdr_ = cmd.getDouble("t", 0.0, 1.0);
  }
  if (cmd.optionSet("i")) {
    numIterations_ = cmd.getInt("i", 0, 1000);
  }
  if (cmd.optionSet("N")) {
    maxPSMs_ = cmd.getInt("N", 0, 100000000);
  }
  if (cmd.optionSet("S")) {
    PseudoRandom::setSeed(cmd.getInt("S", 1, 20000));
  }
  if (cmd.optionSet("D")) {
    DataSet::setCalcDoc(true);
    DescriptionOfCorrect::setDocType(cmd.getInt("D", 0, 15));
  }
  if (cmd.optionSet("K")) {
    DescriptionOfCorrect::setKlammer(true);
  }
  if (cmd.optionSet("s")) {
    xmlSchemaValidation_ = false;
  }
  if (cmd.optionSet("Z")) {
    xmlPrintDecoys_ = true;
  }
  if (cmd.optionSet("y")) {
    if (cmd.optionSet("Y")) {
      std::cerr << "Error: the -Y/-post-processing-tdc and "
        << "-y/-post-processing-mix-max options were both set. "
        << "Use only one of these options at a time." << std::endl;
      return 0;
    }
    useMixMax_ = true;
  } else if (cmd.optionSet("Y")) {
    targetDecoyCompetition_ = true;
  }
  if (cmd.optionSet("I")) {
    inputSearchType_ = cmd.options["I"];
    if (inputSearchType_ == "concatenated") {
      if (useMixMax_) {
        std::cerr << "Error: concatenated search specified for -I/-search-input"
            << " is incompatible with the specified -y/-post-processing-mix-max "
            << "option." << std::endl;
        return 0;
      }
      targetDecoyCompetition_ = false;
      useMixMax_ = false;
    } else if (inputSearchType_ == "separate") {
      if (!targetDecoyCompetition_) {
        useMixMax_ = true;
      }
    } else if (inputSearchType_ != "auto") {
      std::cerr << "Error: the -I/-search-input option has to be one out of "
                << "\"concatenated\", \"separate\" or \"auto\"." << std::endl;
      return 0;
    }
  }
  // if there are no arguments left...
  if (cmd.arguments.size() == 0) {
    if(!cmd.optionSet("j") && !cmd.optionSet("k") && !cmd.optionSet("e") && !cmd.optionSet("OI") && !cmd.optionSet("")){ // unless the input comes from -j, -k or -e option
      cerr << "Error: too few arguments.";
      cerr << "\nInvoke with -h option for help\n";
      return 0; // ...error
    }
  }
  // if there is one argument left...
  if (cmd.arguments.size() == 1) {
    tabInput_ = true;
    inputFN_ = cmd.arguments[0]; // then it's the pin input
    if (cmd.optionSet("k") || cmd.optionSet("j")){ // and if the tab input is also present
      cerr << "Error: use one of either pin-xml or tab-delimited input format.";
      cerr << "\nInvoke with -h option for help.\n";
      return 0; // ...error
    }
    if (cmd.optionSet("e") || cmd.optionSet("")){ // if stdin pin file is present
      cerr << "Error: the pin file has already been given as stdinput argument.";
      cerr << "\nInvoke with -h option for help.\n";
      return 0; // ...error
    }
  }
  // if there is more then one argument left...
  if (cmd.arguments.size() > 1) {
    cerr << "Error: too many arguments.";
    cerr << "\nInvoke with -h option for help\n";
    return 0; // ...error
  }

  return true;
}

/** Calculates the PSM and/or peptide probabilities
 * @param isUniquePeptideRun boolean indicating if we want peptide or PSM probabilities
 * @param procStart clock time when process started
 * @param procStartClock clock associated with procStart
 * @param diff runtime of the calculations
 */
void Caller::calculatePSMProb(Scores& allScores, bool isUniquePeptideRun, 
    time_t& procStart, clock_t& procStartClock, double& diff){
  // write output (cerr or xml) if this is the unique peptide run and the
  // reportUniquePeptides_ option was switched on OR if this is not the unique
  // peptide run and the option was switched off
  bool writeOutput = (isUniquePeptideRun == reportUniquePeptides_);
  
  if (reportUniquePeptides_ && VERB > 0 && writeOutput) {
    cerr << "Tossing out \"redundant\" PSMs keeping only the best scoring PSM "
        "for each unique peptide." << endl;
  }
  
  if (isUniquePeptideRun) {
    allScores.weedOutRedundant();
  } else if (targetDecoyCompetition_) {
    allScores.weedOutRedundantTDC();
    if (VERB > 0) {
      std::cerr << "Selected best-scoring PSM per scan+expMass"
        << " (target-decoy competition): "
        << allScores.posSize() << " target PSMs and " 
        << allScores.negSize() << " decoy PSMs." << std::endl;
    }
  }
  
  if (VERB > 0 && writeOutput) {
    if (useMixMax_) {
      std::cerr << "Selecting pi_0=" << allScores.getPi0() << std::endl;
    }
    std::cerr << "Calculating q values." << std::endl;
  }
  
  int foundPSMs = allScores.calcQ(testFdr_);
  
  if (VERB > 0 && writeOutput) {
    if (useMixMax_) {
      std::cerr << "New pi_0 estimate on final list yields ";
    } else {
      std::cerr << "Final list yields ";
    }
    std::cerr << foundPSMs << " target " << (reportUniquePeptides_ ? "peptides" : "PSMs") 
              << " with q<" << testFdr_ << "." << endl;
    std::cerr << "Calculating posterior error probabilities (PEPs)." << std::endl;
  }
  
  allScores.calcPep();
  
  if (VERB > 1 && writeOutput) {
    time_t end;
    time(&end);
    diff = difftime(end, procStart);
    ostringstream timerValues;
    timerValues.precision(4);
    timerValues << "Processing took " << ((double)(clock() - procStartClock)) / (double)CLOCKS_PER_SEC
                << " cpu seconds or " << diff << " seconds wall clock time." << endl;
    std::cerr << timerValues.str();
  }
  
  std::string targetFN, decoyFN;
  if (isUniquePeptideRun) {
    targetFN = peptideResultFN_;
    decoyFN = decoyPeptideResultFN_;
  } else {
    targetFN = psmResultFN_;
    decoyFN = decoyPsmResultFN_;
  }
  
  if (oswInput_) {
    allScores.reportOSW(inputFN_, oswLevel_);
  }

  else {
    if (!targetFN.empty()) {
      ofstream targetStream(targetFN.c_str(), ios::out);
      allScores.print(NORMAL, targetStream);
    } else if (writeOutput) {
      allScores.print(NORMAL);
    }
    if (!decoyFN.empty()) {
      ofstream decoyStream(decoyFN.c_str(), ios::out);
      allScores.print(SHUFFLED, decoyStream);
    }
  }

}

/** 
 * Calculates the protein probabilites by calling Fido and directly writes 
 * the results to XML
 */
void Caller::calculateProteinProbabilities(Scores& allScores) {
  time_t startTime;
  clock_t startClock;
  time(&startTime);
  startClock = clock();  

  if (VERB > 0) {
    cerr << "\nCalculating protein level probabilities.\n";
    cerr << protEstimator_->printCopyright();
  }
  
  protEstimator_->initialize(allScores);
  
  if (VERB > 1) {
    std::cerr << "Initialized protein inference engine." << std::endl;
  }
  
  protEstimator_->run();
  
  if (VERB > 1) {
    std::cerr << "Computing protein probabilities." << std::endl;
  }
  
  protEstimator_->computeProbabilities();
  
  if (VERB > 1) {
    std::cerr << "Computing protein statistics." << std::endl;
  }
  
  protEstimator_->computeStatistics();
  
  time_t procStart;
  clock_t procStartClock = clock();
  time(&procStart);
  double diff_time = difftime(procStart, startTime);
  
  if (VERB > 1) {  
    ostringstream timerValues;
    timerValues.precision(4);
    timerValues << "Estimating protein probabilities took : "
      << ((double)(procStartClock - startClock)) / (double)CLOCKS_PER_SEC
      << " cpu seconds or " << diff_time << " seconds wall clock time." << endl;
    std::cerr << timerValues.str();
  }
  
  protEstimator_->printOut(proteinResultFN_, decoyProteinResultFN_);
}

/** 
 * Executes the flow of the percolator process:
 * 1. reads in the input file
 * 2. trains the SVM
 * 3. calculate PSM probabilities
 * 4. (optional) calculate peptide probabilities
 * 5. (optional) calculate protein probabilities
 */
int Caller::run() {  
  time_t startTime;
  time(&startTime);
  clock_t startClock = clock();
  if (VERB > 0) {
    cerr << extendedGreeter(startTime);
  }
  
  int success = 0;
  std::ifstream fileStream;
  if (!readStdIn_) {
    if (!tabInput_ && !oswInput_) fileStream.exceptions(ifstream::badbit | ifstream::failbit);
    fileStream.open(inputFN_.c_str(), ios::in);
  } else if (maxPSMs_ > 0u) {
    maxPSMs_ = 0u;
    std::cerr << "Warning: cannot use subset-max-train (-N flag) when reading "
              << "from stdin, training on all data instead." << std::endl;
  }
  
  std::istream &dataStream = readStdIn_ ? std::cin : fileStream;
  
  XMLInterface xmlInterface(xmlOutputFN_, xmlSchemaValidation_, 
                            xmlPrintDecoys_, xmlPrintExpMass_);
  SetHandler setHandler(maxPSMs_);
  if (tabInput_) {
    if (VERB > 1) {
      std::cerr << "Reading tab-delimited input from datafile " << inputFN_ << std::endl;
    }
    success = setHandler.readTab(dataStream, pCheck_);
  } else if (oswInput_) {
    if (VERB > 1) {
      std::cerr << "Reading OSW input from datafile " << inputFN_ << std::endl;
    }
    success = setHandler.readOSW(inputFN_, oswLevel_, pCheck_);
  } else {
    if (VERB > 1) {
      std::cerr << "Reading pin-xml input from datafile " << inputFN_ << std::endl;
    }
    success = xmlInterface.readPin(dataStream, inputFN_, setHandler, pCheck_, protEstimator_);
  }
  
  // Reading input files (pin or temporary file)
  if (!success) {
    std::cerr << "ERROR: Failed to read in file, check if the correct " <<
                 "file-format was used.";
    return 0;
  }
  
  if (VERB > 2) {
    std::cerr << "FeatureNames::getNumFeatures(): "<< FeatureNames::getNumFeatures() << endl;
  }
  
  setHandler.normalizeFeatures(pNorm_);
  
  /*
  auto search-input detection cases:
  true search   detected search  mix-max  tdc  flag for mix-max       flag for tdc
  separate      separate         yes      yes  none (but -y allowed)  -Y
  separate      concatenated     yes      yes  -y (force)             -Y
  concatenated  concatenated     no       yes  NA                     none (but -Y allowed)
  concatenated  separate         no       yes  NA                     -Y (force)
  */
  if (inputSearchType_ == "auto") {
    if (pCheck_->concatenatedSearch()) {
      if (useMixMax_) {
        if (VERB > 0) {
          std::cerr << "Warning: concatenated search input detected, "
            << "but overridden by -y flag: using mix-max anyway." << std::endl;
        }
      } else {
        if (VERB > 0) {
          std::cerr << "Concatenated search input detected, skipping both " 
            << "target-decoy competition and mix-max." << std::endl;
        }
      }
    } else { // separate searches detected
      if (targetDecoyCompetition_) { // this also captures the case where input was in reality from concatenated search
        if (VERB > 0) {
          std::cerr << "Separate target and decoy search inputs detected, "
            << "using target-decoy competition on Percolator scores." << std::endl;
        }
      } else {
        useMixMax_ = true;
        if (VERB > 0) {
          std::cerr << "Separate target and decoy search inputs detected, "
            << "using mix-max method." << std::endl;
        }
      }
    }
  } else if (pCheck_->concatenatedSearch() && inputSearchType_ == "separate") {
    if (VERB > 0) {
      std::cerr << "Warning: concatenated search input detected, but "
        << "overridden by -I flag specifying separate searches." << std::endl;
    }
  } else if (!pCheck_->concatenatedSearch() && inputSearchType_ == "concatenated") {
    if (VERB > 0) {
      std::cerr << "Warning: separate searches input detected, but "
        << "overridden by -I flag specifying a concatenated search." << std::endl;
    }
  }
  assert(!(useMixMax_ && targetDecoyCompetition_));
  
  // Copy feature data pointers to Scores object
  Scores allScores(useMixMax_);
  allScores.fillFeatures(setHandler);
  
  if (VERB > 0 && useMixMax_ && 
        abs(1.0 - allScores.getTargetDecoySizeRatio()) > 0.1) {
    std::cerr << "Warning: The mix-max procedure is not well behaved when "
      << "# targets (" << allScores.posSize() << ") != "
      << "# decoys (" << allScores.negSize() << "). "
      << "Consider using target-decoy competition (-Y flag)." << std::endl;
  }
  
  CrossValidation crossValidation(quickValidation_, reportEachIteration_, 
                                  testFdr_, selectionFdr_, selectedCpos_, 
                                  selectedCneg_, numIterations_, useMixMax_);
  int firstNumberOfPositives = crossValidation.preIterationSetup(allScores, pCheck_, pNorm_, setHandler.getFeaturePool());
  if (VERB > 0) {
    cerr << "Found " << firstNumberOfPositives << " test set positives with q<"
        << testFdr_ << " in initial direction" << endl;
  }
  
  if (DataSet::getCalcDoc()) {
    setHandler.normalizeDOCFeatures(pNorm_);
  }
  
  time_t procStart;
  clock_t procStartClock = clock();
  time(&procStart);
  double diff = difftime(procStart, startTime);
  if (VERB > 1) cerr << "Reading in data and feature calculation took "
      << ((double)(procStartClock - startClock)) / (double)CLOCKS_PER_SEC
      << " cpu seconds or " << diff << " seconds wall clock time." << endl;
  
  if (tabOutputFN_.length() > 0) {
    setHandler.writeTab(tabOutputFN_, pCheck_);
  }
  
  // Do the SVM training
  crossValidation.train(pNorm_);
  
  if (weightOutputFN_.size() > 0) {
    ofstream weightStream(weightOutputFN_.c_str(), ios::out);
    crossValidation.printAllWeights(weightStream, pNorm_);
    weightStream.close();
  }
  
  // Calculate the final SVM scores and clean up structures
  crossValidation.postIterationProcessing(allScores, pCheck_);
  
  if (VERB > 0 && DataSet::getCalcDoc()) {
    crossValidation.printDOC();
  }
  
  if (setHandler.getMaxPSMs() > 0u) {
    if (VERB > 0) {
      cerr << "Scoring full list of PSMs with trained SVMs." << endl;
    }
    std::vector<double> rawWeights;
    crossValidation.getAvgWeights(rawWeights, pNorm_);
    setHandler.reset();
    allScores.reset();
    
    fileStream.clear();
    fileStream.seekg(0, ios::beg);
    if (tabInput_) {
      success = setHandler.readAndScoreTab(fileStream, rawWeights, allScores, pCheck_);
    } if (oswInput_) {
      success = setHandler.readAndScoreTab(fileStream, rawWeights, allScores, pCheck_);
    } else {
      success = xmlInterface.readAndScorePin(fileStream, rawWeights, allScores, inputFN_, setHandler, pCheck_, protEstimator_);
    }
        
    // Reading input files (pin or temporary file)
    if (!success) {
      std::cerr << "ERROR: Failed to read in file, check if the correct " <<
                   "file-format was used.";
      return 0;
    }
    
    if (VERB > 1) {
      cerr << "Evaluated set contained " << allScores.posSize()
          << " positives and " << allScores.negSize() << " negatives." << endl;
    }
    
    allScores.postMergeStep();
    allScores.calcQ(selectionFdr_);
    allScores.normalizeScores(selectionFdr_);
  }
  
  // calculate psms level probabilities TDA or TDC
  bool isUniquePeptideRun = false;
  calculatePSMProb(allScores, isUniquePeptideRun, procStart, procStartClock, diff);
#ifdef CRUX
  processPsmScores(allScores);
#endif
  if (xmlInterface.getXmlOutputFN().size() > 0){
    xmlInterface.writeXML_PSMs(allScores);
  }
  
  // calculate unique peptides level probabilities WOTE
  if (reportUniquePeptides_){
    isUniquePeptideRun = true;
    calculatePSMProb(allScores, isUniquePeptideRun, procStart, procStartClock, diff);
#ifdef CRUX
    processPeptideScores(allScores);
#endif
    if (xmlInterface.getXmlOutputFN().size() > 0){
      xmlInterface.writeXML_Peptides(allScores);
    }
  }
  
  // calculate protein level probabilities with FIDO
  if (ProteinProbEstimator::getCalcProteinLevelProb()){
    calculateProteinProbabilities(allScores);
#ifdef CRUX
    processProteinScores(protEstimator_);
#endif
    if (xmlInterface.getXmlOutputFN().size() > 0) {
      xmlInterface.writeXML_Proteins(protEstimator_);
    }
  }
  // write output to file
  xmlInterface.writeXML(allScores, protEstimator_, call_);
  Enzyme::destroy();
  return 1;
}
