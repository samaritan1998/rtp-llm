#pragma once

#include "maga_transformer/cpp/cache/CacheConfig.h"
#include "src/fastertransformer/th_op/GptInitParameter.h"

namespace rtp_llm {

class CacheConfigCreator {
public:
    static std::tuple<bool, CacheConfig> createConfig(const GptInitParameter& param);

private:
    static CacheConfig createBasicConfig(const GptInitParameter& param);
    static std::tuple<bool, int64_t> getKVCacheMemorySize(const GptInitParameter& param);
};

}  // namespace rtp_llm
