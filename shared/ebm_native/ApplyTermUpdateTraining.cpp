// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <ebm@koch.ninja>

#include "precompiled_header_cpp.hpp"

#include <stddef.h> // size_t, ptrdiff_t

#include "logging.h" // EBM_ASSERT
#include "zones.h"

#include "approximate_math.hpp"
#include "ebm_stats.hpp"

namespace DEFINED_ZONE_NAME {
#ifndef DEFINED_ZONE_NAME
#error DEFINED_ZONE_NAME must be defined
#endif // DEFINED_ZONE_NAME

// C++ does not allow partial function specialization, so we need to use these cumbersome static class functions to do partial function specialization

template<ptrdiff_t cCompilerClasses>
class ApplyTermUpdateTrainingZeroFeatures final {
public:

   ApplyTermUpdateTrainingZeroFeatures() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      UNUSED(runtimeBitPack);
      UNUSED(aInputData);

      static_assert(IsClassification(cCompilerClasses), "must be classification");
      static_assert(!IsBinaryClassification(cCompilerClasses), "must be multiclass");
      static constexpr size_t cCompilerScores = GetCountScores(cCompilerClasses);

      FloatFast aLocalExpVector[cCompilerScores];
      FloatFast * const aExps = 
         k_dynamicClassification == cCompilerClasses ? aMulticlassMidwayTemp : aLocalExpVector;

      const ptrdiff_t cClasses = GET_COUNT_CLASSES(cCompilerClasses, cRuntimeClasses);
      const size_t cScores = GetCountScores(cClasses);
      EBM_ASSERT(1 <= cSamples);

      EBM_ASSERT(nullptr != aUpdateScores);

      FloatFast * pGradientAndHessian = aGradientAndHessian;
      const StorageDataType * pTargetData = reinterpret_cast<const StorageDataType *>(aTargetData);
      FloatFast * pSampleScore = aSampleScore;
      const FloatFast * const pSampleScoresEnd = pSampleScore + cSamples * cScores;
      do {
         size_t targetData = static_cast<size_t>(*pTargetData);
         ++pTargetData;

         const FloatFast * pUpdateScore = aUpdateScores;
         FloatFast * pExp = aExps;
         FloatFast sumExp = FloatFast { 0 };
         size_t iScore = 0;
         do {
            // TODO : because there is only one bin for a zero feature feature group, we could move these values to the stack where the
            // compiler could reason about their visibility and optimize small arrays into registers
            const FloatFast updateScore = *pUpdateScore;
            ++pUpdateScore;
            // this will apply a small fix to our existing TrainingSampleScores, either positive or negative, whichever is needed
            const FloatFast sampleScore = *pSampleScore + updateScore;
            *pSampleScore = sampleScore;
            ++pSampleScore;
            const FloatFast oneExp = ExpForMulticlass<false>(sampleScore);
            *pExp = oneExp;
            ++pExp;
            sumExp += oneExp;
            ++iScore;
         } while(iScore < cScores);
         pExp -= cScores;
         iScore = 0;
         do {
            FloatFast gradient;
            FloatFast hessian;
            EbmStats::InverseLinkFunctionThenCalculateGradientAndHessianMulticlass(
               sumExp,
               *pExp,
               targetData,
               iScore,
               gradient,
               hessian
            );
            ++pExp;
            *pGradientAndHessian = gradient;
            *(pGradientAndHessian + 1) = hessian;
            pGradientAndHessian += 2;
            ++iScore;
         } while(iScore < cScores);
      } while(pSampleScoresEnd != pSampleScore);
   }
};

#ifndef EXPAND_BINARY_LOGITS
template<>
class ApplyTermUpdateTrainingZeroFeatures<2> final {
public:

   ApplyTermUpdateTrainingZeroFeatures() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      UNUSED(cRuntimeClasses);
      UNUSED(runtimeBitPack);
      UNUSED(aMulticlassMidwayTemp);
      UNUSED(aInputData);

      EBM_ASSERT(1 <= cSamples);
      EBM_ASSERT(nullptr != aUpdateScores);

      FloatFast * pGradientAndHessian = aGradientAndHessian;
      const StorageDataType * pTargetData = reinterpret_cast<const StorageDataType *>(aTargetData);
      FloatFast * pSampleScore = aSampleScore;
      const FloatFast * const pSampleScoresEnd = pSampleScore + cSamples;
      const FloatFast updateScore = aUpdateScores[0];
      do {
         size_t targetData = static_cast<size_t>(*pTargetData);
         ++pTargetData;
         // this will apply a small fix to our existing TrainingSampleScores, either positive or negative, whichever is needed
         const FloatFast sampleScore = *pSampleScore + updateScore;
         *pSampleScore = sampleScore;
         ++pSampleScore;
         const FloatFast gradient = EbmStats::InverseLinkFunctionThenCalculateGradientBinaryClassification(sampleScore, targetData);
         *pGradientAndHessian = gradient;
         *(pGradientAndHessian + 1) = EbmStats::CalculateHessianFromGradientBinaryClassification(gradient);
         pGradientAndHessian += 2;
      } while(pSampleScoresEnd != pSampleScore);
   }
};
#endif // EXPAND_BINARY_LOGITS

template<>
class ApplyTermUpdateTrainingZeroFeatures<k_regression> final {
public:

   ApplyTermUpdateTrainingZeroFeatures() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      UNUSED(cRuntimeClasses);
      UNUSED(runtimeBitPack);
      UNUSED(aMulticlassMidwayTemp);
      UNUSED(aSampleScore);
      UNUSED(aInputData);
      UNUSED(aTargetData);

      EBM_ASSERT(1 <= cSamples);
      EBM_ASSERT(nullptr != aUpdateScores);

      // no hessian for regression
      FloatFast * pGradient = aGradientAndHessian;
      const FloatFast * const pGradientsEnd = pGradient + cSamples;
      const FloatFast updateScore = aUpdateScores[0];
      do {
         // this will apply a small fix to our existing TrainingSampleScores, either positive or negative, whichever is needed
         const FloatFast gradient = EbmStats::ComputeGradientRegressionMSEFromOriginalGradient(*pGradient, updateScore);
         *pGradient = gradient;
         ++pGradient;
      } while(pGradientsEnd != pGradient);
   }
};

template<ptrdiff_t cPossibleClasses>
class ApplyTermUpdateTrainingZeroFeaturesTarget final {
public:

   ApplyTermUpdateTrainingZeroFeaturesTarget() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      static_assert(IsClassification(cPossibleClasses), "cPossibleClasses needs to be a classification");
      static_assert(cPossibleClasses <= k_cCompilerClassesMax, "We can't have this many items in a data pack.");

      EBM_ASSERT(IsClassification(cRuntimeClasses));
      EBM_ASSERT(cRuntimeClasses <= k_cCompilerClassesMax);

      if(cPossibleClasses == cRuntimeClasses) {
         ApplyTermUpdateTrainingZeroFeatures<cPossibleClasses>::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      } else {
         ApplyTermUpdateTrainingZeroFeaturesTarget<
            cPossibleClasses + 1
         >::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      }
   }
};

template<>
class ApplyTermUpdateTrainingZeroFeaturesTarget<k_cCompilerClassesMax + 1> final {
public:

   ApplyTermUpdateTrainingZeroFeaturesTarget() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      static_assert(IsClassification(k_cCompilerClassesMax), "k_cCompilerClassesMax needs to be a classification");

      EBM_ASSERT(IsClassification(cRuntimeClasses));
      EBM_ASSERT(k_cCompilerClassesMax < cRuntimeClasses);

      ApplyTermUpdateTrainingZeroFeatures<k_dynamicClassification>::Func(
         cRuntimeClasses,
         runtimeBitPack,
         aMulticlassMidwayTemp,
         aUpdateScores,
         cSamples,
         aInputData,
         aTargetData,
         aSampleScore,
         aGradientAndHessian
      );
   }
};

template<ptrdiff_t cCompilerClasses, ptrdiff_t compilerBitPack>
class ApplyTermUpdateTrainingInternal final {
public:

   ApplyTermUpdateTrainingInternal() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      static_assert(IsClassification(cCompilerClasses), "must be classification");
      static_assert(!IsBinaryClassification(cCompilerClasses), "must be multiclass");
      static constexpr size_t cCompilerScores = GetCountScores(cCompilerClasses);

      FloatFast aLocalExpVector[cCompilerScores];
      FloatFast * const aExps = 
         k_dynamicClassification == cCompilerClasses ? aMulticlassMidwayTemp : aLocalExpVector;

      const ptrdiff_t cClasses = GET_COUNT_CLASSES(cCompilerClasses, cRuntimeClasses);
      const size_t cScores = GetCountScores(cClasses);
      EBM_ASSERT(1 <= cSamples);

      const size_t cItemsPerBitPack = GET_ITEMS_PER_BIT_PACK(compilerBitPack, runtimeBitPack);
      EBM_ASSERT(1 <= cItemsPerBitPack);
      EBM_ASSERT(cItemsPerBitPack <= k_cBitsForStorageType);
      const size_t cBitsPerItemMax = GetCountBits(cItemsPerBitPack);
      EBM_ASSERT(1 <= cBitsPerItemMax);
      EBM_ASSERT(cBitsPerItemMax <= k_cBitsForStorageType);
      const StorageDataType maskBits = (~StorageDataType { 0 }) >> (k_cBitsForStorageType - cBitsPerItemMax);

      EBM_ASSERT(nullptr != aUpdateScores);

      FloatFast * pGradientAndHessian = aGradientAndHessian;
      const StorageDataType * pInputData = aInputData;
      const StorageDataType * pTargetData = reinterpret_cast<const StorageDataType *>(aTargetData);
      FloatFast * pSampleScore = aSampleScore;

      // this shouldn't overflow since we're accessing existing memory
      const FloatFast * const pSampleScoresTrueEnd = pSampleScore + cSamples * cScores;
      const FloatFast * pSampleScoresExit = pSampleScoresTrueEnd;
      const FloatFast * pSampleScoresInnerEnd = pSampleScoresTrueEnd;
      if(cSamples <= cItemsPerBitPack) {
         goto one_last_loop;
      }
      pSampleScoresExit = pSampleScoresTrueEnd - ((cSamples - 1) % cItemsPerBitPack + 1) * cScores;
      EBM_ASSERT(pSampleScore < pSampleScoresExit);
      EBM_ASSERT(pSampleScoresExit < pSampleScoresTrueEnd);

      do {
         pSampleScoresInnerEnd = pSampleScore + cItemsPerBitPack * cScores;
         // jumping back into this loop and changing pSampleScoresInnerEnd to a dynamic value that isn't compile time determinable causes this 
         // function to NOT be optimized for templated cItemsPerBitPack, but that's ok since avoiding one unpredictable branch here is negligible
      one_last_loop:;
         // we store the already multiplied dimensional value in *pInputData
         StorageDataType iTensorBinCombined = *pInputData;
         ++pInputData;
         do {
            size_t targetData = static_cast<size_t>(*pTargetData);
            ++pTargetData;

            const size_t iTensorBin = static_cast<size_t>(maskBits & iTensorBinCombined);
            const FloatFast * pUpdateScore = &aUpdateScores[iTensorBin * cScores];
            FloatFast * pExp = aExps;
            FloatFast sumExp = 0;
            size_t iScore = 0;
            do {
               const FloatFast updateScore = *pUpdateScore;
               ++pUpdateScore;
               // this will apply a small fix to our existing TrainingSampleScores, either positive or negative, whichever is needed
               const FloatFast sampleScore = *pSampleScore + updateScore;
               *pSampleScore = sampleScore;
               ++pSampleScore;
               const FloatFast oneExp = ExpForMulticlass<false>(sampleScore);
               *pExp = oneExp;
               ++pExp;
               sumExp += oneExp;
               ++iScore;
            } while(iScore < cScores);
            pExp -= cScores;
            iScore = 0;
            do {
               FloatFast gradient;
               FloatFast hessian;
               EbmStats::InverseLinkFunctionThenCalculateGradientAndHessianMulticlass(
                  sumExp,
                  *pExp,
                  targetData,
                  iScore,
                  gradient,
                  hessian
               );
               ++pExp;
               *pGradientAndHessian = gradient;
               *(pGradientAndHessian + 1) = hessian;
               pGradientAndHessian += 2;
               ++iScore;
            } while(iScore < cScores);

            iTensorBinCombined >>= cBitsPerItemMax;
         } while(pSampleScoresInnerEnd != pSampleScore);
      } while(pSampleScoresExit != pSampleScore);

      // first time through?
      if(pSampleScoresTrueEnd != pSampleScore) {
         pSampleScoresInnerEnd = pSampleScoresTrueEnd;
         pSampleScoresExit = pSampleScoresTrueEnd;
         goto one_last_loop;
      }
   }
};

#ifndef EXPAND_BINARY_LOGITS
template<ptrdiff_t compilerBitPack>
class ApplyTermUpdateTrainingInternal<2, compilerBitPack> final {
public:

   ApplyTermUpdateTrainingInternal() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      UNUSED(cRuntimeClasses);
      UNUSED(aMulticlassMidwayTemp);
      EBM_ASSERT(1 <= cSamples);

      const size_t cItemsPerBitPack = GET_ITEMS_PER_BIT_PACK(compilerBitPack, runtimeBitPack);
      EBM_ASSERT(1 <= cItemsPerBitPack);
      EBM_ASSERT(cItemsPerBitPack <= k_cBitsForStorageType);
      const size_t cBitsPerItemMax = GetCountBits(cItemsPerBitPack);
      EBM_ASSERT(1 <= cBitsPerItemMax);
      EBM_ASSERT(cBitsPerItemMax <= k_cBitsForStorageType);
      const StorageDataType maskBits = (~StorageDataType { 0 }) >> (k_cBitsForStorageType - cBitsPerItemMax);

      EBM_ASSERT(nullptr != aUpdateScores);

      FloatFast * pGradientAndHessian = aGradientAndHessian;
      const StorageDataType * pInputData = aInputData;
      const StorageDataType * pTargetData = reinterpret_cast<const StorageDataType *>(aTargetData);
      FloatFast * pSampleScore = aSampleScore;

      // this shouldn't overflow since we're accessing existing memory
      const FloatFast * const pSampleScoresTrueEnd = pSampleScore + cSamples;
      const FloatFast * pSampleScoresExit = pSampleScoresTrueEnd;
      const FloatFast * pSampleScoresInnerEnd = pSampleScoresTrueEnd;
      if(cSamples <= cItemsPerBitPack) {
         goto one_last_loop;
      }
      pSampleScoresExit = pSampleScoresTrueEnd - ((cSamples - 1) % cItemsPerBitPack + 1);
      EBM_ASSERT(pSampleScore < pSampleScoresExit);
      EBM_ASSERT(pSampleScoresExit < pSampleScoresTrueEnd);

      do {
         pSampleScoresInnerEnd = pSampleScore + cItemsPerBitPack;
         // jumping back into this loop and changing pSampleScoresInnerEnd to a dynamic value that isn't compile time determinable causes this 
         // function to NOT be optimized for templated cItemsPerBitPack, but that's ok since avoiding one unpredictable branch here is negligible
      one_last_loop:;
         // we store the already multiplied dimensional value in *pInputData
         StorageDataType iTensorBinCombined = *pInputData;
         ++pInputData;
         do {
            size_t targetData = static_cast<size_t>(*pTargetData);
            ++pTargetData;

            const size_t iTensorBin = static_cast<size_t>(maskBits & iTensorBinCombined);

            const FloatFast updateScore = aUpdateScores[iTensorBin];
            // this will apply a small fix to our existing TrainingSampleScores, either positive or negative, whichever is needed
            const FloatFast sampleScore = *pSampleScore + updateScore;
            *pSampleScore = sampleScore;
            ++pSampleScore;
            const FloatFast gradient = EbmStats::InverseLinkFunctionThenCalculateGradientBinaryClassification(sampleScore, targetData);

            *pGradientAndHessian = gradient;
            *(pGradientAndHessian + 1) = EbmStats::CalculateHessianFromGradientBinaryClassification(gradient);
            pGradientAndHessian += 2;

            iTensorBinCombined >>= cBitsPerItemMax;
         } while(pSampleScoresInnerEnd != pSampleScore);
      } while(pSampleScoresExit != pSampleScore);

      // first time through?
      if(pSampleScoresTrueEnd != pSampleScore) {
         pSampleScoresInnerEnd = pSampleScoresTrueEnd;
         pSampleScoresExit = pSampleScoresTrueEnd;
         goto one_last_loop;
      }
   }
};
#endif // EXPAND_BINARY_LOGITS

template<ptrdiff_t compilerBitPack>
class ApplyTermUpdateTrainingInternal<k_regression, compilerBitPack> final {
public:

   ApplyTermUpdateTrainingInternal() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      UNUSED(cRuntimeClasses);
      UNUSED(aMulticlassMidwayTemp);
      UNUSED(aTargetData);
      UNUSED(aSampleScore);
      EBM_ASSERT(1 <= cSamples);

      const size_t cItemsPerBitPack = GET_ITEMS_PER_BIT_PACK(compilerBitPack, runtimeBitPack);
      EBM_ASSERT(1 <= cItemsPerBitPack);
      EBM_ASSERT(cItemsPerBitPack <= k_cBitsForStorageType);
      const size_t cBitsPerItemMax = GetCountBits(cItemsPerBitPack);
      EBM_ASSERT(1 <= cBitsPerItemMax);
      EBM_ASSERT(cBitsPerItemMax <= k_cBitsForStorageType);
      const StorageDataType maskBits = (~StorageDataType { 0 }) >> (k_cBitsForStorageType - cBitsPerItemMax);

      EBM_ASSERT(nullptr != aUpdateScores);

      // No hessians for regression
      FloatFast * pGradient = aGradientAndHessian;
      const StorageDataType * pInputData = aInputData;

      // this shouldn't overflow since we're accessing existing memory
      const FloatFast * const pGradientTrueEnd = pGradient + cSamples;
      const FloatFast * pGradientExit = pGradientTrueEnd;
      const FloatFast * pGradientInnerEnd = pGradientTrueEnd;
      if(cSamples <= cItemsPerBitPack) {
         goto one_last_loop;
      }
      pGradientExit = pGradientTrueEnd - ((cSamples - 1) % cItemsPerBitPack + 1);
      EBM_ASSERT(pGradient < pGradientExit);
      EBM_ASSERT(pGradientExit < pGradientTrueEnd);

      do {
         pGradientInnerEnd = pGradient + cItemsPerBitPack;
         // jumping back into this loop and changing pSampleScoresInnerEnd to a dynamic value that isn't compile time determinable causes this 
         // function to NOT be optimized for templated cItemsPerBitPack, but that's ok since avoiding one unpredictable branch here is negligible
      one_last_loop:;
         // we store the already multiplied dimensional value in *pInputData
         StorageDataType iTensorBinCombined = *pInputData;
         ++pInputData;
         do {
            const size_t iTensorBin = static_cast<size_t>(maskBits & iTensorBinCombined);

            const FloatFast updateScore = aUpdateScores[iTensorBin];
            // this will apply a small fix to our existing TrainingSampleScores, either positive or negative, whichever is needed
            const FloatFast gradient = EbmStats::ComputeGradientRegressionMSEFromOriginalGradient(*pGradient, updateScore);

            *pGradient = gradient;
            ++pGradient;

            iTensorBinCombined >>= cBitsPerItemMax;
         } while(pGradientInnerEnd != pGradient);
      } while(pGradientExit != pGradient);

      // first time through?
      if(pGradientTrueEnd != pGradient) {
         pGradientInnerEnd = pGradientTrueEnd;
         pGradientExit = pGradientTrueEnd;
         goto one_last_loop;
      }
   }
};

template<ptrdiff_t cPossibleClasses>
class ApplyTermUpdateTrainingNormalTarget final {
public:

   ApplyTermUpdateTrainingNormalTarget() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      static_assert(IsClassification(cPossibleClasses), "cPossibleClasses needs to be a classification");
      static_assert(cPossibleClasses <= k_cCompilerClassesMax, "We can't have this many items in a data pack.");

      EBM_ASSERT(IsClassification(cRuntimeClasses));
      EBM_ASSERT(cRuntimeClasses <= k_cCompilerClassesMax);

      if(cPossibleClasses == cRuntimeClasses) {
         ApplyTermUpdateTrainingInternal<cPossibleClasses, k_cItemsPerBitPackDynamic>::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      } else {
         ApplyTermUpdateTrainingNormalTarget<
            cPossibleClasses + 1
         >::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      }
   }
};

template<>
class ApplyTermUpdateTrainingNormalTarget<k_cCompilerClassesMax + 1> final {
public:

   ApplyTermUpdateTrainingNormalTarget() = delete; // this is a static class.  Do not construct

   INLINE_RELEASE_UNTEMPLATED static void Func(
      const ptrdiff_t cRuntimeClasses,
      const ptrdiff_t runtimeBitPack,
      FloatFast * const aMulticlassMidwayTemp,
      const FloatFast * const aUpdateScores,
      const size_t cSamples,
      const StorageDataType * const aInputData,
      const void * const aTargetData,
      FloatFast * const aSampleScore,
      FloatFast * const aGradientAndHessian
   ) {
      static_assert(IsClassification(k_cCompilerClassesMax), "k_cCompilerClassesMax needs to be a classification");

      EBM_ASSERT(IsClassification(cRuntimeClasses));
      EBM_ASSERT(k_cCompilerClassesMax < cRuntimeClasses);

      ApplyTermUpdateTrainingInternal<k_dynamicClassification, k_cItemsPerBitPackDynamic>::Func(
         cRuntimeClasses,
         runtimeBitPack,
         aMulticlassMidwayTemp,
         aUpdateScores,
         cSamples,
         aInputData,
         aTargetData,
         aSampleScore,
         aGradientAndHessian
      );
   }
};

extern void ApplyTermUpdateTraining(
   const ptrdiff_t cRuntimeClasses,
   const ptrdiff_t runtimeBitPack,
   FloatFast * const aMulticlassMidwayTemp,
   const FloatFast * const aUpdateScores,
   const size_t cSamples,
   const StorageDataType * const aInputData,
   const void * const aTargetData,
   FloatFast * const aSampleScore,
   FloatFast * const aGradientAndHessian
) {
   LOG_0(Trace_Verbose, "Entered ApplyTermUpdateTraining");

   if(k_cItemsPerBitPackNone == runtimeBitPack) {
      if(IsClassification(cRuntimeClasses)) {
         ApplyTermUpdateTrainingZeroFeaturesTarget<2>::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      } else {
         EBM_ASSERT(IsRegression(cRuntimeClasses));
         ApplyTermUpdateTrainingZeroFeatures<k_regression>::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      }
   } else {
      if(IsClassification(cRuntimeClasses)) {
         ApplyTermUpdateTrainingNormalTarget<2>::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      } else {
         EBM_ASSERT(IsRegression(cRuntimeClasses));
         ApplyTermUpdateTrainingInternal<k_regression, k_cItemsPerBitPackDynamic>::Func(
            cRuntimeClasses,
            runtimeBitPack,
            aMulticlassMidwayTemp,
            aUpdateScores,
            cSamples,
            aInputData,
            aTargetData,
            aSampleScore,
            aGradientAndHessian
         );
      }
   }

   LOG_0(Trace_Verbose, "Exited ApplyTermUpdateTraining");
}

} // DEFINED_ZONE_NAME
