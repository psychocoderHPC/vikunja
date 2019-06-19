#include <vikunja/test/AlpakaSetup.hpp>
#include <vikunja/reduce/detail/BlockThreadReduceKernel.hpp>
#include <alpaka/alpaka.hpp>
#include <alpaka/test/acc/Acc.hpp>
#include <alpaka/test/queue/Queue.hpp>
#include <catch2/catch.hpp>
#include <cstdlib>
#include <iostream>
#include <vikunja/reduce/reduce.hpp>
#include <type_traits>
#include <cstdio>
#include <vector>
#include <thread>

#if defined(VIKUNJA_REDUCE_COMPARING_BENCHMARKS) && defined(ALPAKA_ACC_GPU_CUDA_ENABLED)
#include <thrust/device_vector.h>
#include <thrust/reduce.h>
#include <thrust/functional.h>
#endif

struct TestTemplate {
private:
    const uint64_t memSize;
public:

    TestTemplate(uint64_t const memSize) : memSize(memSize) {}

    template<typename TAcc>
    void operator()() {
        using TRed = uint64_t;
        
        using Idx = alpaka::idx::Idx<TAcc>;
        using Dim = alpaka::dim::Dim<TAcc>;
        const Idx n = static_cast<Idx>(memSize);
        constexpr Idx blocksPerGrid = 8;
        constexpr Idx threadsPerBlock = 1;
        const Idx elementsPerThread = n / blocksPerGrid / threadsPerBlock + 1;

        using Vec = alpaka::vec::Vec<Dim, Idx>;
        constexpr Idx xIndex = Dim::value - 1u;

        Vec gridSize(Vec::all(static_cast<Idx>(1u)));
        Vec blockSize(Vec::all(static_cast<Idx>(1u)));
        Vec threadSize(Vec::all(static_cast<Idx>(1u)));

        Vec extent(Vec::all(static_cast<Idx>(1)));
        extent[xIndex] = n;

        gridSize[xIndex] = blocksPerGrid;
        blockSize[xIndex] = threadsPerBlock;
        threadSize[xIndex] = elementsPerThread;

        using DevAcc = alpaka::dev::Dev<TAcc>;
        using PltfAcc = alpaka::pltf::Pltf<DevAcc>;
        // Async queue makes things slower on CPU?
       // using QueueAcc = alpaka::test::queue::DefaultQueue<alpaka::dev::Dev<TAcc>>;
        using PltfHost = alpaka::pltf::PltfCpu;
        using DevHost = alpaka::dev::Dev<PltfHost>;
        using QueueAcc = //alpaka::queue::QueueCpuAsync;
                typename std::conditional<std::is_same<PltfAcc, alpaka::pltf::PltfCpu>::value, alpaka::queue::QueueCpuSync,
#ifdef  ALPAKA_ACC_GPU_CUDA_ENABLED
        alpaka::queue::QueueCudaRtSync
#elseif ALPAKA_ACC_GPU_HIP_ENABLED
        alpaka::queue::QueueHipRtSync
#else
        alpaka::queue::QueueCpuSync
#endif
        >::type;
        using QueueHost = alpaka::queue::QueueCpuSync;
        using WorkDiv = alpaka::workdiv::WorkDivMembers<Dim, Idx>;

        // Get the host device.
        DevHost devHost(
                alpaka::pltf::getDevByIdx<PltfHost>(0u));
        // Get a queue on the host device.
        QueueHost queueHost(
                devHost);
        // Select a device to execute on.
        DevAcc devAcc(
                alpaka::pltf::getDevByIdx<PltfAcc>(0u));
        // Get a queue on the accelerator device.
        QueueAcc queueAcc(
                devAcc);
        WorkDiv workdiv{
                gridSize,
                blockSize,
                threadSize
        };

        auto deviceMem(alpaka::mem::buf::alloc<TRed, Idx>(devAcc, extent));
        auto hostMem(alpaka::mem::buf::alloc<TRed, Idx>(devHost, extent));
        TRed* hostNative = alpaka::mem::view::getPtrNative(hostMem);
        for(Idx i = 0; i < n; ++i) {
            //std::cout << i << "\n";
            hostNative[i] = static_cast<TRed>(i + 1);
        }
        alpaka::mem::view::copy(queueAcc, deviceMem, hostMem, extent);
        auto sum = [=] ALPAKA_FN_HOST_ACC (TRed i, TRed j) {
            return i + j;
        };
        auto doubleNum = [=] ALPAKA_FN_HOST_ACC (TRed i) {
            return 2 * i;
        };
        std::cout << "Testing accelerator: " << alpaka::acc::getAccName<TAcc>() << " with size: " << n <<"\n";

        auto start = std::chrono::high_resolution_clock::now();
        Idx reduceResult = vikunja::reduce::deviceReduce<TAcc>(devAcc, devHost, queueAcc, n, alpaka::mem::view::getPtrNative(deviceMem), sum);
        auto end = std::chrono::high_resolution_clock::now();
        auto expectedResult = (n * (n + 1) / 2);
        REQUIRE(expectedResult == reduceResult);
        std::cout << "Runtime of " << alpaka::acc::getAccName<TAcc>() << ": "
                << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " microseconds\n";
        using MemAccess = vikunja::mem::iterator::MemAccessPolicy<TAcc>;
        std::cout << "MemAccessPolicy: " << MemAccess::getName() << "\n";

        auto expectedTransformReduce = expectedResult * 2;
        Idx transformReduceResult = vikunja::reduce::deviceTransformReduce<TAcc>(devAcc, devHost, queueAcc, n, alpaka::mem::view::getPtrNative(deviceMem), doubleNum, sum);
        REQUIRE(expectedTransformReduce == transformReduceResult);
    }
};

TEST_CASE("Test reduce", "[reduce]")
{

    using TestAccs = alpaka::test::acc::EnabledAccs<
            alpaka::dim::DimInt<3u>,
            std::uint64_t>;
    //std::cout << std::thread::hardware_concurrency() << "\n";
    SECTION("deviceReduce") {

        std::vector<uint64_t> memorySizes{1, 10, 777,(1<< 10) + 1, 1 << 12, 1 << 14, 1 << 15, 1 << 18, (1 << 18) + 1, 1 << 25, 1 << 27};

        for(auto &memSize: memorySizes) {
            alpaka::meta::forEachType<TestAccs>(TestTemplate(memSize));
        }
#ifdef VIKUNJA_REDUCE_COMPARING_BENCHMARKS
        std::cout << "---------------------------------------------\n";
        std::cout << "Now performing some benchmarks...\n";
        const std::uint64_t size = memorySizes.back();
        std::vector<uint64_t> reduce(size);
        for(uint64_t i = 0; i < reduce.size(); ++i) {
            reduce[i] = i + 1;
        }
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t tSum = 0;
        for(uint64_t i = 0; i < reduce.size(); ++i) {
            tSum += reduce[i];
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Runtime of dumb: ";
        std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " microseconds\n";
        std::cout << "tSum = " << tSum << "\n";
#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED
        // test against thrust
        thrust::device_vector<std::uint64_t> deviceReduce(reduce);
        start = std::chrono::high_resolution_clock::now();
        tSum = thrust::reduce(deviceReduce.begin(), deviceReduce.end(), static_cast<std::uint64_t>(0), thrust::plus<std::uint64_t>());
        end = std::chrono::high_resolution_clock::now();
        std::cout << "Runtime of thrust reduce: ";
        std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " microseconds\n";
        std::cout << "tSum = " << tSum << "\n";

#endif

#endif
    }
}