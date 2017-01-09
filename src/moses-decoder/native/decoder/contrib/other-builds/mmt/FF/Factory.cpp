#include "util/exception.hh"
#include "moses/FF/Factory.h"
#include "moses/StaticData.h"

#ifdef SAPT
#include "moses/TranslationModel/PhraseDictionarySADB.h"
#endif

#include "moses/FF/LexicalReordering/LexicalReordering.h"

#include "moses/FF/UnknownWordPenaltyProducer.h"
#include "moses/FF/DistortionScoreProducer.h"
#include "moses/FF/WordPenaltyProducer.h"
#include "moses/FF/InputFeature.h"
#include "moses/FF/PhrasePenalty.h"
#include "moses/FF/DynamicCacheBasedLanguageModel.h"
#include "moses/FF/SkeletonStatelessFF.h"
#include "moses/FF/SkeletonStatefulFF.h"
#include "moses/Syntax/InputWeightFF.h"
#include "moses/Syntax/RuleTableFF.h"

#include "moses/LM/Ken.h"
#ifdef LM_IRST
#include "moses/LM/IRST.h"
#endif

#ifdef LM_MMTILM
#include "moses/LM/MMTInterpolatedLM.h"
#endif

#include "util/exception.hh"

#include <vector>

namespace Moses
{

class FeatureFactory
{
public:
  /** Uses the given functor for feature setup. */
  FeatureFactory(FeatureSetup &featureSetup): DefaultSetup(featureSetup) {}
  virtual ~FeatureFactory() {}

  virtual void Create(const std::string &line) = 0;

protected:
  FeatureSetup& DefaultSetup;
};


/**
 * Default functor for registering features globally in moses using StaticData and FeatureFunction statics.
 */
class FeatureDefaultSetup : public FeatureSetup {
public:
  virtual void operator()(FeatureFunction *feature)
  {
    FeatureFunction::Register(feature);

    StaticData &static_data = StaticData::InstanceNonConst();
    const std::string &featureName = feature->GetScoreProducerDescription();
    std::vector<float> weights = static_data.GetParameter()->GetWeights(featureName);


    if (feature->GetNumScoreComponents()) {
      if (weights.size() == 0) {
        weights = feature->DefaultWeights();
        if (weights.size() == 0) {
          TRACE_ERR("WARNING: No weights specified in config file for FF "
                    << featureName << ". This FF does not supply default values.\n"
                    << "WARNING: Auto-initializing all weights for this FF to 1.0");
          weights.assign(feature->GetNumScoreComponents(),1.0);
        } else {
          VERBOSE(2,"WARNING: No weights specified in config file for FF "
                    << featureName << ". Using default values supplied by FF.");
        }
      }
      UTIL_THROW_IF2(weights.size() != feature->GetNumScoreComponents(),
                     "FATAL ERROR: Mismatch in number of features and number "
                     << "of weights for Feature Function " << featureName
                     << " (features: " << feature->GetNumScoreComponents()
                     << " vs. weights: " << weights.size() << ")");
      static_data.SetWeights(feature, weights);
    } else if (feature->IsTuneable())
      static_data.SetWeights(feature, weights);
  }
};

namespace
{

template <class F>
class DefaultFeatureFactory : public FeatureFactory
{
public:
  DefaultFeatureFactory(FeatureSetup &featureSetup): FeatureFactory(featureSetup) {}

  void Create(const std::string &line) {
    DefaultSetup(new F(line));
  }
};

class KenFactory : public FeatureFactory
{
public:
  KenFactory(FeatureSetup &featureSetup): FeatureFactory(featureSetup) {}

  void Create(const std::string &line) {
    DefaultSetup(ConstructKenLM(line));
  }
};

} // namespace

FeatureRegistry::FeatureRegistry()
{
  featureSetup_.reset(new FeatureDefaultSetup());
  AddFactories(*featureSetup_);
}

// TODO: use shared_ptr
FeatureRegistry::FeatureRegistry(SPTR<FeatureSetup> featureSetup)
{
  featureSetup_ = featureSetup;
  AddFactories(*featureSetup_);
}

void FeatureRegistry::AddFactories(FeatureSetup& setup)
{
// Feature with same name as class
#define MOSES_FNAME(name) Add(#name, new DefaultFeatureFactory< name >(setup));
// Feature with different name than class.
#define MOSES_FNAME2(name, type) Add(name, new DefaultFeatureFactory< type >(setup));

//  MOSES_FNAME2("PhraseDictionaryBinary", PhraseDictionaryTreeAdaptor);
//  MOSES_FNAME(PhraseDictionaryOnDisk);
//  MOSES_FNAME(PhraseDictionaryMemory);
//  MOSES_FNAME(PhraseDictionaryScope3);
//  MOSES_FNAME(PhraseDictionaryMultiModel);
//  MOSES_FNAME(PhraseDictionaryMultiModelCounts);
//  MOSES_FNAME(PhraseDictionaryGroup);
//  MOSES_FNAME(PhraseDictionaryALSuffixArray);
//  MOSES_FNAME(PhraseDictionaryDynSuffixArray);
//  MOSES_FNAME(PhraseDictionaryTransliteration);
//  MOSES_FNAME(PhraseDictionaryDynamicCacheBased);
//  MOSES_FNAME(PhraseDictionaryFuzzyMatch);
//  MOSES_FNAME(ProbingPT);
//  MOSES_FNAME(PhraseDictionaryMemoryPerSentence);
//  MOSES_FNAME2("RuleTable", Syntax::RuleTableFF);
//  MOSES_FNAME2("SyntaxInputWeight", Syntax::InputWeightFF);


#ifdef SAPT
  MOSES_FNAME2("SAPT",PhraseDictionarySADB);
#endif

//  MOSES_FNAME(GlobalLexicalModel);
  //MOSES_FNAME(GlobalLexicalModelUnlimited); This was commented out in the original
//  MOSES_FNAME(Model1Feature);
//  MOSES_FNAME(SourceWordDeletionFeature);
//  MOSES_FNAME(TargetWordInsertionFeature);
//  MOSES_FNAME(PhraseBoundaryFeature);
//  MOSES_FNAME(PhraseLengthFeature);
//  MOSES_FNAME(WordTranslationFeature);
//  MOSES_FNAME(TargetBigramFeature);
//  MOSES_FNAME(TargetNgramFeature);
//  MOSES_FNAME(PhrasePairFeature);
//  MOSES_FNAME(RulePairUnlexicalizedSource);
  MOSES_FNAME(LexicalReordering);
//  MOSES_FNAME2("Generation", GenerationDictionary);
//  MOSES_FNAME(BleuScoreFeature);
  MOSES_FNAME2("Distortion", DistortionScoreProducer);
  MOSES_FNAME2("WordPenalty", WordPenaltyProducer);
//  MOSES_FNAME(InputFeature);
//  MOSES_FNAME(OpSequenceModel);
  MOSES_FNAME(PhrasePenalty);
  MOSES_FNAME2("UnknownWordPenalty", UnknownWordPenaltyProducer);
//  MOSES_FNAME(ControlRecombination);
//  MOSES_FNAME(ConstrainedDecoding);
//  MOSES_FNAME(CoveredReferenceFeature);
//  MOSES_FNAME(SourceGHKMTreeInputMatchFeature);
//  MOSES_FNAME(SoftSourceSyntacticConstraintsFeature);
//  MOSES_FNAME(TargetConstituentAdjacencyFeature);
//  MOSES_FNAME(TargetPreferencesFeature);
//  MOSES_FNAME(TreeStructureFeature);
//  MOSES_FNAME(SoftMatchingFeature);
//  MOSES_FNAME(DynamicCacheBasedLanguageModel);
//  MOSES_FNAME(HyperParameterAsWeight);
//  MOSES_FNAME(SetSourcePhrase);
//  MOSES_FNAME(CountNonTerms);
//  MOSES_FNAME(ReferenceComparison);
//  MOSES_FNAME(RuleScope);
//  MOSES_FNAME(MaxSpanFreeNonTermSource);
//  MOSES_FNAME(NieceTerminal);
//  MOSES_FNAME(SparseHieroReorderingFeature);
//  MOSES_FNAME(SpanLength);
//  MOSES_FNAME(SyntaxRHS);
//  MOSES_FNAME(PhraseOrientationFeature);
//  MOSES_FNAME(UnalignedWordCountFeature);
//  MOSES_FNAME(DeleteRules);

  MOSES_FNAME(SkeletonStatelessFF);
  MOSES_FNAME(SkeletonStatefulFF);
//  MOSES_FNAME(SkeletonPT);

#ifdef LM_IRST
  MOSES_FNAME2("IRSTLM", LanguageModelIRST);
#endif
#ifdef LM_MMTILM
  MOSES_FNAME2("MMTILM", MMTInterpolatedLM);
#endif
  Add("KENLM", new KenFactory(setup));
}

FeatureRegistry::~FeatureRegistry()
{
}

void FeatureRegistry::Add(const std::string &name, FeatureFactory *factory)
{
  std::pair<std::string, boost::shared_ptr<FeatureFactory> > to_ins(name, boost::shared_ptr<FeatureFactory>(factory));
  UTIL_THROW_IF2(!registry_.insert(to_ins).second, "Duplicate feature name " << name);
}

namespace
{
class UnknownFeatureException : public util::Exception {};
}

void FeatureRegistry::Construct(const std::string &name, const std::string &line)
{
  Map::iterator i = registry_.find(name);
  UTIL_THROW_IF(i == registry_.end(), UnknownFeatureException, "Feature name " << name << " is not registered.");
  i->second->Create(line);
}

void FeatureRegistry::PrintFF() const
{
  std::vector<std::string> ffs;
  std::cerr << "Available feature functions:" << std::endl;
  Map::const_iterator iter;
  for (iter = registry_.begin(); iter != registry_.end(); ++iter) {
    const std::string &ffName = iter->first;
    ffs.push_back(ffName);
  }

  std::vector<std::string>::const_iterator iterVec;
  std::sort(ffs.begin(), ffs.end());
  for (iterVec = ffs.begin(); iterVec != ffs.end(); ++iterVec) {
    const std::string &ffName = *iterVec;
    std::cerr << ffName << " ";
  }

  std::cerr << std::endl;
}

} // namespace Moses
