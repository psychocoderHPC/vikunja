//
// Created by mewes30 on 19.12.18.
//

#pragma once

#include <vikunja/mem/iterator/PolicyBasedBlockIterator.hpp>
#include <alpaka/alpaka.hpp>

namespace vikunja {
namespace reduce {
namespace detail {

    template<typename TRed, uint64_t size>
    struct sharedStaticArray
    {
        TRed data[size];

        ALPAKA_FN_HOST_ACC ALPAKA_FN_INLINE TRed &operator[](uint64_t index) {
            return data[index];
        }
        ALPAKA_FN_HOST_ACC ALPAKA_FN_INLINE const TRed &operator[](uint64_t index) const {
            return data[index];
        }
    };

    // TODO: move TFunc to operator()
    template<uint64_t TBlockSize, typename TMemAccessPolicy, typename TRed>
    struct BlockThreadReduceKernel {
        template<typename TAcc, typename TIdx,
                typename TInputIterator, typename TOutputIterator, typename TTransformFunc, typename TFunc>
        ALPAKA_FN_ACC void operator()(TAcc const &acc,
                TInputIterator const &source,
                TOutputIterator const &destination,
                TIdx const &n,
                TTransformFunc const &transformFunc,
                TFunc const &func) const  {
            // Shared Mem
            auto &sdata(
                    alpaka::block::shared::st::allocVar<sharedStaticArray<TRed, TBlockSize>,
                    __COUNTER__>(acc));


            // blockIdx.x
            auto blockIndex = (alpaka::idx::getIdx<alpaka::Grid, alpaka::Blocks>(acc)[0]);
            // threadIdx.x
            auto threadIndex = (alpaka::idx::getIdx<alpaka::Block, alpaka::Threads>(acc)[0]);
            // blockIdx.x * TBlocksize + threadIdx.x
            auto indexInBlock(alpaka::idx::getIdx<alpaka::Grid, alpaka::Threads>(acc)[0]);

            using MemPolicy = TMemAccessPolicy;
            vikunja::mem::iterator::PolicyBasedBlockIterator<MemPolicy, TAcc, TInputIterator> iter(source, acc, n, TBlockSize);
            auto startIndex = MemPolicy::getStartIndex(acc, static_cast<TIdx>(n), static_cast<TIdx>(TBlockSize));
            //auto endIndex = MemPolicy::getEndIndex(acc, static_cast<TIdx>(n), static_cast<TIdx>(TBlockSize));
            //auto stepSize = MemPolicy::getStepSize(acc, static_cast<TIdx>(n), static_cast<TIdx>(TBlockSize));
            // WARNING: in theory, one might return here, but then the cpu kernels get stuck on the syncthreads.
            // TODO: however, now an undefined memory access occurs if iter >= iter.end()
            // fix this or discuss at least
            /*if(iter >= iter.end()) {
               // return;
            }*/
            if(startIndex < n) {
                auto tSum = transformFunc(*iter);
                ++iter;
                while(iter + 3 < iter.end()) {
                    tSum = func(func(func(func(tSum, transformFunc(*iter) ), transformFunc(*(iter + 1)) ), transformFunc(*(iter + 2)) ), transformFunc(*(iter + 3)) );
                    iter += 4;
                }
                while(iter < iter.end()) {
                    tSum = func(tSum, transformFunc(*iter));
                    ++iter;
                }
                // This condition actually relies on the memory access pattern.
                // When gridStriding is used, the first n threads always get the first n values,
                // but when the linearMemAccess is used, they do not.
                // This is circumvented by now that if the block size is bigger than the problem size, a sequential algorithm is used.
                if(MemPolicy::isValidThreadResult(acc, static_cast<TIdx>(n), static_cast<TIdx>(n))) {
                    sdata[threadIndex] = tSum;
                }
            }

            alpaka::block::sync::syncBlockThreads(acc); // sync: after sdataMapping

            // blockReduce
            // unroll for better performance
            for(TIdx bs = TBlockSize, bSup = (TBlockSize + 1) / 2;
            bs > 1; bs = bs / 2, bSup = (bs + 1) / 2) {

                bool condition = threadIndex < bSup && // only first half of block is working
                         (threadIndex + bSup) < TBlockSize && // index for second half must be in bounds
                         (indexInBlock + bSup) < n; // if element in second half has ben initialized before
                if(condition) {
                    sdata[threadIndex] = func(sdata[threadIndex], sdata[threadIndex + bSup]);
                }
                alpaka::block::sync::syncBlockThreads(acc); // sync: block reduce loop
            }
            if(threadIndex == 0) {
                *(destination + blockIndex) = sdata[0];
            }
        }
    };
}
}
}