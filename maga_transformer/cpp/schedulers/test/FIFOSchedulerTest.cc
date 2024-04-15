#define private public

#include <memory>
#include "torch/all.h"
#include "gmock/gmock-actions.h"
#include "gmock/gmock-function-mocker.h"
#include "gtest/gtest.h"
#include "maga_transformer/cpp/schedulers/FIFOScheduler.h"
#include "src/fastertransformer/core/Types.h"
#include "src/fastertransformer/devices/testing/TestBase.h"

using namespace std;

namespace rtp_llm {

class FIFOSchedulerTest: public DeviceTestBase {
public:

};

TEST_F(FIFOSchedulerTest, testSimple) {
    CacheConfig                   cache_config(1, 4, 1, 4, 8, fastertransformer::DataType::TYPE_FP16);
    std::shared_ptr<CacheManager> cache_manager = make_shared<CacheManager>(cache_config, nullptr, device_);
    ASSERT_EQ(cache_manager->freeBlockNums(), 3);
    MagaInitParams init_config;
    init_config.gpt_init_parameter               = c10::make_intrusive<GptInitParameter>();
    init_config.gpt_init_parameter->max_seq_len_ = 8192;
    FIFOScheduler scheduler(init_config, cache_manager);
    scheduler.enable_fallback            = true;
    std::shared_ptr<GenerateInput> query = make_shared<GenerateInput>();
    query->input_ids                     = createBuffer<int32_t>({1}, {1}, AllocationType::HOST);
    query->generate_config               = make_shared<GenerateConfig>();
    shared_ptr<GenerateStream> stream    = make_shared<GenerateStream>(query);
    ASSERT_TRUE(scheduler.enqueue(stream).ok());
    auto streams_status = scheduler.schedule();
    ASSERT_TRUE(streams_status.ok());
    ASSERT_EQ(streams_status.value().size(), 1);
    ASSERT_EQ(cache_manager->freeBlockNums(), 2);

    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 1);

    stream->setFinished();

    auto streams_status2 = scheduler.schedule();
    ASSERT_TRUE(streams_status2.ok());
    ASSERT_EQ(streams_status2.value().size(), 0);
    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 0);
    ASSERT_EQ(cache_manager->freeBlockNums(), 3);
}

TEST_F(FIFOSchedulerTest, testInitKVCacheLackMem) {
    CacheConfig                   cache_config(1, 2, 1, 4, 2, fastertransformer::DataType::TYPE_FP16);
    std::shared_ptr<CacheManager> cache_manager = make_shared<CacheManager>(cache_config, nullptr, device_);
    ASSERT_EQ(cache_manager->freeBlockNums(), 1);
    MagaInitParams init_config;
    init_config.gpt_init_parameter               = c10::make_intrusive<GptInitParameter>();
    init_config.gpt_init_parameter->max_seq_len_ = 8192;
    FIFOScheduler scheduler(init_config, cache_manager);
    scheduler.enable_fallback            = true;
    std::shared_ptr<GenerateInput> query = make_shared<GenerateInput>();
    query->input_ids                     = createBuffer<int32_t>({3}, {1, 2, 3}, AllocationType::HOST);
    query->generate_config               = make_shared<GenerateConfig>();
    shared_ptr<GenerateStream> stream    = make_shared<GenerateStream>(query);
    ASSERT_TRUE(scheduler.enqueue(stream).ok());
    auto streams_status = scheduler.schedule();
    ASSERT_TRUE(streams_status.ok());
    ASSERT_EQ(streams_status.value().size(), 0);
    ASSERT_TRUE(stream->stopped());
    ASSERT_EQ(stream->stopReason(), "can not be add input queue");

    auto streams_status2 = scheduler.schedule();
    ASSERT_TRUE(streams_status2.ok());
    ASSERT_EQ(streams_status2.value().size(), 0);
    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 0);
    ASSERT_EQ(cache_manager->freeBlockNums(), 1);
}

TEST_F(FIFOSchedulerTest, testIncrKVCacheLackMem) {
    CacheConfig                   cache_config(1, 3, 1, 4, 2, fastertransformer::DataType::TYPE_FP16);
    std::shared_ptr<CacheManager> cache_manager = make_shared<CacheManager>(cache_config, nullptr, device_);
    ASSERT_EQ(cache_manager->freeBlockNums(), 2);
    MagaInitParams init_config;
    init_config.gpt_init_parameter               = c10::make_intrusive<GptInitParameter>();
    init_config.gpt_init_parameter->max_seq_len_ = 8192;
    FIFOScheduler scheduler(init_config, cache_manager);
    scheduler.enable_fallback            = true;
    std::shared_ptr<GenerateInput> query = make_shared<GenerateInput>();
    query->input_ids                     = createBuffer<int32_t>({4}, {1, 2, 3, 4}, AllocationType::HOST);
    query->generate_config               = make_shared<GenerateConfig>();
    shared_ptr<GenerateStream> stream    = make_shared<GenerateStream>(query);
    ASSERT_TRUE(scheduler.enqueue(stream).ok());
    auto streams_status = scheduler.schedule();
    ASSERT_TRUE(streams_status.ok());
    ASSERT_EQ(streams_status.value().size(), 1);
    ASSERT_FALSE(stream->stopped());
    ASSERT_EQ(stream->stopReason(), "");
    ASSERT_EQ(cache_manager->freeBlockNums(), 0);

    stream->seq_length_++;
    auto streams_status2 = scheduler.schedule();
    ASSERT_TRUE(streams_status2.ok());
    ASSERT_EQ(streams_status2.value().size(), 0);
    ASSERT_TRUE(stream->stopped());
    ASSERT_EQ(stream->stopReason(), "can not be add input queue");
    ASSERT_EQ(cache_manager->freeBlockNums(), 1);

    auto streams_status3 = scheduler.schedule();
    ASSERT_TRUE(streams_status3.ok());
    ASSERT_EQ(streams_status3.value().size(), 0);
    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 0);
    ASSERT_EQ(cache_manager->freeBlockNums(), 2);
}

TEST_F(FIFOSchedulerTest, testReserverBlock) {}

TEST_F(FIFOSchedulerTest, testIncrKVCacheLackMem2) {
    CacheConfig                   cache_config(1, 5, 1, 4, 2, fastertransformer::DataType::TYPE_FP16);
    std::shared_ptr<CacheManager> cache_manager = make_shared<CacheManager>(cache_config, nullptr, device_);
    ASSERT_EQ(cache_manager->freeBlockNums(), 4);
    MagaInitParams init_config;
    init_config.gpt_init_parameter               = c10::make_intrusive<GptInitParameter>();
    init_config.gpt_init_parameter->max_seq_len_ = 8192;
    FIFOScheduler scheduler(init_config, cache_manager);
    scheduler.enable_fallback            = true;
    std::shared_ptr<GenerateInput> query = make_shared<GenerateInput>();
    query->input_ids                     = createBuffer<int32_t>({4}, {1, 2, 3, 4}, AllocationType::HOST);
    query->generate_config               = make_shared<GenerateConfig>();
    shared_ptr<GenerateStream> stream1   = make_shared<GenerateStream>(query);
    shared_ptr<GenerateStream> stream2   = make_shared<GenerateStream>(query);
    ASSERT_TRUE(scheduler.enqueue(stream1).ok());
    ASSERT_TRUE(scheduler.enqueue(stream2).ok());

    auto streams_status = scheduler.schedule();
    ASSERT_TRUE(streams_status.ok());
    ASSERT_EQ(streams_status.value().size(), 2);
    ASSERT_FALSE(stream1->stopped());
    ASSERT_FALSE(stream2->stopped());
    ASSERT_EQ(stream1->stopReason(), "");
    ASSERT_EQ(stream2->stopReason(), "");
    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 2);
    ASSERT_EQ(cache_manager->freeBlockNums(), 0);

    stream1->seq_length_++;
    stream2->seq_length_++;

    auto streams_status2 = scheduler.schedule();
    ASSERT_TRUE(streams_status2.ok());
    ASSERT_EQ(streams_status2.value().size(), 1);
    ASSERT_FALSE(stream1->stopped());
    ASSERT_FALSE(stream2->stopped());
    ASSERT_EQ(stream1->stopReason(), "");
    ASSERT_EQ(stream2->stopReason(), "");
    ASSERT_EQ(scheduler.waitingStreamsSize(), 1);
    ASSERT_EQ(scheduler.runningStreamsSize(), 1);
    ASSERT_EQ(cache_manager->freeBlockNums(), 1);

    stream1->setFinished();
    auto streams_status3 = scheduler.schedule();
    ASSERT_TRUE(streams_status3.ok());
    ASSERT_EQ(streams_status3.value().size(), 1);
    ASSERT_TRUE(stream1->finished());
    ASSERT_FALSE(stream2->stopped());
    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 1);
    ASSERT_EQ(cache_manager->freeBlockNums(), 1);
}

TEST_F(FIFOSchedulerTest, testReuseCache) {
    CacheConfig                   cache_config(1, 11, 1, 4, 2, fastertransformer::DataType::TYPE_FP16);
    std::shared_ptr<CacheManager> cache_manager = make_shared<CacheManager>(cache_config, nullptr, device_);
    ASSERT_EQ(cache_manager->freeBlockNums(), 10);
    MagaInitParams init_config;
    init_config.gpt_init_parameter               = c10::make_intrusive<GptInitParameter>();
    init_config.gpt_init_parameter->max_seq_len_ = 8192;
    FIFOScheduler scheduler(init_config, cache_manager);
    scheduler.enable_fallback = true;

    std::shared_ptr<GenerateInput> query = make_shared<GenerateInput>();
    query->input_ids                     = createBuffer<int32_t>({5}, {1, 2, 3, 4, 5}, AllocationType::HOST);
    query->generate_config               = make_shared<GenerateConfig>();
    shared_ptr<GenerateStream> stream1   = make_shared<GenerateStream>(query);
    stream1->setReuseCache(true);
    ASSERT_TRUE(scheduler.enqueue(stream1).ok());

    auto streams_status = scheduler.schedule();
    ASSERT_TRUE(streams_status.ok());
    ASSERT_EQ(cache_manager->freeBlockNums(), 7);

    stream1->setFinished();
    auto streams_status2 = scheduler.schedule();

    ASSERT_TRUE(streams_status2.ok());
    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 0);
    ASSERT_EQ(cache_manager->freeBlockNums(), 8);

    std::shared_ptr<GenerateInput> query2 = make_shared<GenerateInput>();
    query2->input_ids                     = createBuffer<int32_t>({7}, {1, 2, 3, 4, 5, 6, 7}, AllocationType::HOST);
    query2->generate_config               = make_shared<GenerateConfig>();
    shared_ptr<GenerateStream> stream2    = make_shared<GenerateStream>(query2);
    stream2->setReuseCache(true);
    ASSERT_TRUE(scheduler.enqueue(stream2).ok());

    auto streams_status3 = scheduler.schedule();
    ASSERT_TRUE(streams_status3.ok());
    ASSERT_EQ(cache_manager->freeBlockNums(), 6);

    stream2->setFinished();
    auto streams_status4 = scheduler.schedule();
    ASSERT_TRUE(streams_status4.ok());
    ASSERT_EQ(scheduler.waitingStreamsSize(), 0);
    ASSERT_EQ(scheduler.runningStreamsSize(), 0);
    ASSERT_EQ(cache_manager->freeBlockNums(), 7);
}

}  // namespace rtp_llm