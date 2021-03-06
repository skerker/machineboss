#ifndef ASSEMBLY_INCLUDED
#define ASSEMBLY_INCLUDED

#include <random>

#include "eval.h"
#include "seqpair.h"
#include "logsumexp.h"

struct CompactMachinePath {
  typedef EvaluatedMachineState::TransIndex TransIndex;
  typedef Envelope::InputIndex InputIndex;

  vguard<TransIndex> trans;
  
  void readJson (const json&);
  void writeJson (ostream&) const;
  void updateLen();
  
  MachinePath toMachinePath (const Machine&) const;
  static CompactMachinePath fromMachinePath (const MachinePath&, const Machine&);
};

struct CompactLocalMachinePath : CompactMachinePath {
  InputIndex start, len;

  void readJson (const json&);
  void writeJson (ostream&) const;
};

struct Assembly {
  typedef CompactMachinePath::TransIndex TransIndex;
  typedef CompactLocalMachinePath::InputIndex InputIndex;

  Machine generator, error;
  CompactMachinePath generatorPath;
  vguard<CompactLocalMachinePath> errorPaths;

  EvaluatedMachine evalGen, evalErr;
  vguard<vguard<double> > genSumTrans;

  // I/O and initialization
  void readJson (const json&);
  void writeJson (ostream&) const;

  void validate() const;
  void evaluateMachines();

  // Accessors
  vguard<OutputSymbol> sequence() const;
  
  // Scoring
  LogProb logProb() const;  // log P(sequence,annotation,alignments|machines)

  // Helpers
  static size_t nSequenceMoves (size_t maxResampledTransitions, const CompactMachinePath&);
  static vguard<OutputSymbol> getOutputSeq (const MachinePath&, size_t transStart, size_t transLen);
  static InputIndex getOutputSeqLen (const MachinePath&, size_t transLen);
  static StateIndex getPathState (const MachinePath&, size_t nTrans);
  
  // MCMC
  void resampleSequence (mt19937&, size_t maxResampledTransitions);
  void resampleAlignment (mt19937&, size_t maxAlignSlideWidth);
  void resampleIdentifiedAlignment (mt19937&, size_t nAlign, size_t maxAlignSlideWidth);
  void resampleAnnotation (mt19937&);

  // Compression
  uint8_t* writeEncoded() const;
  void readDecoded (uint8_t*);
};

#endif /* ASSEMBLY_INCLUDED */
