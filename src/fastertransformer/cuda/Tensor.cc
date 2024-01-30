/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/fastertransformer/cuda/Tensor.h"

#include "src/fastertransformer/cuda/cuda_utils.h"
#include "src/fastertransformer/utils/string_utils.h"

#include "stdlib.h"
#include <cuda_fp16.h>
#include <cuda_runtime_api.h>
#include <dirent.h>
#include <numeric>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

namespace fastertransformer {

Tensor::Tensor():
    // a none tensor.
    where_(MEMORY_CPU),
    type_(TYPE_INVALID),
    shape_({}),
    data_(nullptr),
    owned_(false) {}

Tensor::Tensor(const MemoryType          where,
               const DataType            type,
               const std::vector<size_t> shape,
               IAllocator*               allocator,
               const bool                is_set_zero):
    where_(where),
    type_(type),
    shape_(shape),
    data_(nullptr),
    allocator_(allocator),
    owned_(true),
    ref_counter_(std::make_shared<int>(1)) {
    const size_t allocBytes = std::accumulate(shape_.begin(), shape_.end(), (size_t)1, std::multiplies<size_t>())
                              * Tensor::getTypeSize(type_);
    if (where_ == MEMORY_GPU) {
        // TODO(wangyin.yx): Self owned tensor should use Allocator::<AllocatorType::CUDA> to allocate memory.
        // check_cuda_error(allocator->reMalloc(data, sizeBytes(), is_set_zero));
        // check_cuda_error(cudaMalloc(&data, sizeBytes()));
        data_ = allocator_->reMalloc(data_, allocBytes, is_set_zero);
    } else {
        data_ = malloc(allocBytes);
    }
}

Tensor::Tensor(const MemoryType where, const DataType type, const std::vector<size_t> shape, const void* data):
    where_(where),
    type_(type),
    shape_(shape),
    data_(const_cast<void*>(data)),
    owned_(false),
    ref_counter_(nullptr) {}

Tensor::~Tensor() {
    if (owned_ && data_ != nullptr && ref_counter_.use_count() == 1) {
        if (where_ == MEMORY_GPU) {
            // check_cuda_error(cudaFree((void*)data));
            allocator_->free((void**)(&data_));
        } else {
            free((void*)data_);
        }
    }
}

MemoryType Tensor::where() const {
    return where_;
}

DataType Tensor::type() const {
    return type_;
}

const std::vector<size_t>& Tensor::shape() const {
    return shape_;
}

void* Tensor::data() const {
    return data_;
}

IAllocator* Tensor::allocator() const {
    return allocator_;
}

// TODO(wangyin): move this implementation to DeviceOps.
template<typename T>
inline T Tensor::getVal(size_t index) const {
    FT_LOG_DEBUG("%s start", __PRETTY_FUNCTION__);
    FT_CHECK(data_ != nullptr);
    FT_CHECK_WITH_INFO(index < size(), "index is larger than buffer size");

    if (getTensorType<T>() != type_) {
        FT_LOG_DEBUG("getVal with type %s, but data type is: %s",
                        getNumpyTypeDesc(getTensorType<T>()).c_str(),
                        getNumpyTypeDesc(type_).c_str());
    }
    if (where_ == MEMORY_CPU) {
        return ((T*)data())[index];
    } else {
        using ValueType = typename std::remove_const<T>::type;
        ValueType val;
        cudaMemcpy(&val, (ValueType*)data_ + index, sizeof(ValueType), cudaMemcpyDeviceToHost);
        return val;
    }
}

template<typename T>
DataType getTensorType() {
    using RealT = typename std::remove_cv<T>::type;
    if (std::is_same<RealT, float>::value) {
        return TYPE_FP32;
    } else if (std::is_same<RealT, half>::value) {
        return TYPE_FP16;
    }
#ifdef ENABLE_BF16
    else if (std::is_same<RealT, __nv_bfloat16>::value) {
        return TYPE_BF16;
    }
#endif
#ifdef ENABLE_FP8
    else if (std::is_same<RealT, __nv_fp8_e4m3>::value) {
        return TYPE_FP8_E4M3;
    }
#endif
    else if (std::is_same<RealT, int>::value) {
        return TYPE_INT32;
    } else if (std::is_same<RealT, int8_t>::value) {
        return TYPE_INT8;
    } else if (std::is_same<RealT, uint>::value) {
        return TYPE_UINT32;
    } else if (std::is_same<RealT, unsigned long long int>::value || std::is_same<RealT, uint64_t>::value) {
        return TYPE_UINT64;
    } else if (std::is_same<RealT, bool>::value) {
        return TYPE_BOOL;
    } else if (std::is_same<RealT, char>::value) {
        return TYPE_BYTES;
    } else {
        return TYPE_INVALID;
    }
}

#define DECLARE_TEMPLATE_METHODS_WITH_TYPE(T) \
    template T Tensor::getVal<T>(size_t index) const; \
    template const T Tensor::getVal<const T>(size_t index) const; \
    template DataType getTensorType<T>(); \
    template DataType getTensorType<const T*>(); \
    template DataType getTensorType<const T>();

DECLARE_TEMPLATE_METHODS_WITH_TYPE(float)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(half)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(char)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(int8_t)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(int)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(uint)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(uint64_t)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(long)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(unsigned long long int)
DECLARE_TEMPLATE_METHODS_WITH_TYPE(bool)
// DECLARE_TEMPLATE_METHODS_WITH_TYPE(void)
#ifdef ENABLE_BF16
DECLARE_TEMPLATE_METHODS_WITH_TYPE(__nv_bfloat16)
#endif
#ifdef ENABLE_FP8
DECLARE_TEMPLATE_METHODS_WITH_TYPE(__nv_fp8_e4m3)
#endif

#undef DECLARE_TEMPLATE_METHODS_WITH_TYPE

template DataType getTensorType<void>();


void Tensor::parseNpyIntro(FILE*& f_ptr, uint32_t& header_len, uint32_t& start_data) {
    const char magic[]                   = "\x93"
                                           "NUMPY";
    char       magic_test[sizeof(magic)] = "\0";

    size_t n_elems = fread((void*)magic_test, sizeof(char), sizeof(magic) - 1, f_ptr);
    if (n_elems != sizeof(magic) - 1 || std::string(magic) != std::string(magic_test)) {
        throw std::runtime_error("Could read magic token in NPY file");
    }

    uint8_t npy_major = 0;
    uint8_t npy_minor = 0;
    n_elems           = fread((void*)&npy_major, sizeof(uint8_t), 1, f_ptr);
    n_elems += fread((void*)&npy_minor, sizeof(uint8_t), 1, f_ptr);

    if (npy_major == 1) {
        uint16_t header_len_u16 = 0;
        n_elems                 = fread((void*)&header_len_u16, sizeof(uint16_t), 1, f_ptr);
        header_len              = header_len_u16;
    } else if (npy_major == 2) {
        uint32_t header_len_u32 = 0;
        n_elems                 = fread((void*)&header_len_u32, sizeof(uint32_t), 1, f_ptr);
        header_len              = header_len_u32;
    } else {
        throw std::runtime_error("Unsupported npy version: " + std::to_string(npy_major));
    }

    start_data = 8 + 2 * npy_major + header_len;
}

int Tensor::parseNpyHeader(FILE*& f_ptr, uint32_t header_len, DataType& type, std::vector<size_t>& shape) {
    char*  header_c = (char*)malloc(header_len * sizeof(char));
    size_t n_elems  = fread((void*)header_c, sizeof(char), header_len, f_ptr);
    if (n_elems != header_len) {
        free(header_c);
        return -1;
    }
    std::string header(header_c, header_len);
    free(header_c);

    size_t start, end;
    start = header.find("'descr'") + 7;
    start = header.find("'", start);
    end   = header.find("'", start + 1);
    type  = typeFromNumpyDesc(header.substr(start + 1, end - start - 1));

    start = header.find("'fortran_order'") + 15;
    start = header.find(":", start);
    end   = header.find(",", start + 1);
    if (header.substr(start + 1, end - start - 1).find("False") == std::string::npos) {
        throw std::runtime_error("Unsupported value for fortran_order while reading npy file");
    }

    start = header.find("'shape'") + 7;
    start = header.find("(", start);
    end   = header.find(")", start + 1);

    std::istringstream shape_stream(header.substr(start + 1, end - start - 1));
    std::string        token;

    shape.clear();
    while (std::getline(shape_stream, token, ',')) {
        if (token.find_first_not_of(' ') == std::string::npos) {
            break;
        }
        shape.push_back(std::stoul(token));
    }

    return 0;
}

Tensor Tensor::loadNpy(const std::string& npy_file, const MemoryType where) {
    DataType            type;
    std::vector<size_t> shape;

    FILE* f_ptr = fopen(npy_file.c_str(), "rb");
    if (f_ptr == nullptr) {
        throw std::runtime_error("Could not open file " + npy_file);
    }
    uint32_t header_len, start_data;
    parseNpyIntro(f_ptr, header_len, start_data);
    parseNpyHeader(f_ptr, header_len, type, shape);

    auto tensor = Tensor(where, type, shape, (void*)nullptr);

    const size_t size     = tensor.size();
    void*        data_cpu = (where == MEMORY_CPU) ? const_cast<void*>(tensor.data()) : malloc(tensor.sizeBytes());
    void*        data     = data_cpu;

    size_t n_elems = fread(data_cpu, Tensor::getTypeSize(type), size, f_ptr);
    FT_CHECK_WITH_INFO(n_elems == size, "reading tensor failed");

    if (where == MEMORY_GPU) {
        cudaMemcpy(const_cast<void*>(tensor.data()), data_cpu, tensor.sizeBytes(), cudaMemcpyHostToDevice);
        free(data_cpu);
    }

    fclose(f_ptr);
    return std::move(tensor);
}

size_t Tensor::size() const {
    if (data_ == nullptr || shape_.size() == 0) {
        return 0;
    }
    return std::accumulate(shape_.begin(), shape_.end(), (size_t)1, std::multiplies<size_t>());
}

size_t Tensor::sizeBytes() const {
    return size() * Tensor::getTypeSize(type());
}

std::string Tensor::whereToString() const {
    static const std::unordered_map<MemoryType, std::string> mem_to_string{
        {MEMORY_CPU, "CPU"}, {MEMORY_CPU_PINNED, "CPU_PINNED"}, {MEMORY_GPU, "GPU"}};
    return mem_to_string.at(where());
}

std::string Tensor::toString() const {
    std::string memtype_str = whereToString();

    static const std::unordered_map<DataType, std::string> type_to_string{
        {TYPE_BOOL, "BOOL"},
        {TYPE_UINT8, "UINT8"},
        {TYPE_UINT16, "UINT16"},
        {TYPE_UINT32, "UINT32"},
        {TYPE_UINT64, "UINT64"},
        {TYPE_INT8, "INT8"},
        {TYPE_INT16, "INT16"},
        {TYPE_INT32, "INT32"},
        {TYPE_INT64, "INT64"},
        {TYPE_BF16, "BF16"},
        {TYPE_FP16, "FP16"},
        {TYPE_FP32, "FP32"},
        {TYPE_FP64, "FP64"},
        {TYPE_BYTES, "BYTES"},
        {TYPE_INVALID, "INVALID"},
        {TYPE_FP8_E4M3, "E4M3"},
        {TYPE_VOID, "VOID"},
    };
    return fmtstr("Tensor[where=%s, type=%s, shape=%s, data=%p]",
                  memtype_str.c_str(),
                  type_to_string.at(type()).c_str(),
                  vec2str(shape()).c_str(),
                  data());
}

DataType Tensor::typeFromNumpyDesc(std::string type) {
    static const std::unordered_map<std::string, DataType> type_map{{"?", TYPE_BOOL},
                                                                    {"b", TYPE_BYTES},
                                                                    {"u1", TYPE_UINT8},
                                                                    {"u2", TYPE_UINT16},
                                                                    {"u4", TYPE_UINT32},
                                                                    {"u8", TYPE_UINT64},
                                                                    {"i1", TYPE_INT8},
                                                                    {"i2", TYPE_INT16},
                                                                    {"i4", TYPE_INT32},
                                                                    {"i8", TYPE_INT64},
                                                                    {"f2", TYPE_FP16},
                                                                    {"f4", TYPE_FP32},
                                                                    {"f8", TYPE_FP64}};
    return type_map.at(type);
}

size_t Tensor::getTypeSize(DataType type) {
    static const std::unordered_map<DataType, size_t> type_map{{TYPE_BOOL, sizeof(bool)},
                                                               {TYPE_BYTES, sizeof(char)},
                                                               {TYPE_UINT8, sizeof(uint8_t)},
                                                               {TYPE_UINT16, sizeof(uint16_t)},
                                                               {TYPE_UINT32, sizeof(uint32_t)},
                                                               {TYPE_UINT64, sizeof(uint64_t)},
                                                               {TYPE_INT8, sizeof(int8_t)},
                                                               {TYPE_INT16, sizeof(int16_t)},
                                                               {TYPE_INT32, sizeof(int32_t)},
                                                               {TYPE_INT64, sizeof(int64_t)},
#ifdef ENABLE_BF16
                                                               {TYPE_BF16, sizeof(__nv_bfloat16)},
#endif
#ifdef ENABLE_FP8
                                                               {TYPE_FP8_E4M3, sizeof(__nv_fp8_e4m3)},
#endif
                                                               {TYPE_FP16, sizeof(half)},
                                                               {TYPE_FP32, sizeof(float)},
                                                               {TYPE_FP64, sizeof(double)}};
    return type_map.at(type);
}

std::string Tensor::getNumpyTypeDesc(DataType type) const {
    static const std::unordered_map<DataType, std::string> type_map{{TYPE_INVALID, "x"},
                                                                    {TYPE_BOOL, "?"},
                                                                    {TYPE_BYTES, "b"},
                                                                    {TYPE_UINT8, "u1"},
                                                                    {TYPE_UINT16, "u2"},
                                                                    {TYPE_UINT32, "u4"},
                                                                    {TYPE_UINT64, "u8"},
                                                                    {TYPE_INT8, "i1"},
                                                                    {TYPE_INT16, "i2"},
                                                                    {TYPE_INT32, "i4"},
                                                                    {TYPE_INT64, "i8"},
                                                                    {TYPE_FP16, "f2"},
                                                                    {TYPE_FP32, "f4"},
                                                                    {TYPE_FP64, "f8"}};

    if (type == TYPE_BF16) {
        FT_LOG_WARNING("getNumpyTypeDesc(TYPE_BF16) returns an invalid type 'x' since Numpy doesn't "
                       "support bfloat16 as of now, it will be properly extended if numpy supports. "
                       "Please refer for the discussions https://github.com/numpy/numpy/issues/19808.");
    }

    return type_map.count(type) > 0 ? type_map.at(type) : "x";
}

void Tensor::saveNpy(const std::string& filename) const {
    // Save tensor to NPY 1.0 format (see https://numpy.org/neps/nep-0001-npy-format.html)
    void*  cpu_data     = (void*)data();
    bool   is_data_temp = false;
    size_t tensor_size  = size();
    if (where() == MemoryType::MEMORY_GPU) {
        cpu_data     = malloc(tensor_size * Tensor::getTypeSize(type()));
        is_data_temp = true;
        cudaDeviceSynchronize();
        cudaMemcpy(cpu_data, data_, tensor_size * Tensor::getTypeSize(type()), cudaMemcpyDeviceToHost);
    }

    const char    magic[]   = "\x93"
                              "NUMPY";
    const uint8_t npy_major = 1;
    const uint8_t npy_minor = 0;

    std::stringstream header_stream;
    header_stream << "{'descr': '" << getNumpyTypeDesc(type()) << "', 'fortran_order': False, 'shape': (";
    for (size_t i = 0; i < shape().size(); ++i) {
        header_stream << shape()[i];
        if (i + 1 < shape().size() || shape().size() == 1) {
            header_stream << ", ";
        }
    }
    header_stream << ")}";
    int base_length = 6 + 4 + header_stream.str().size();
    int pad_length  = 16 * ((base_length + 1 + 15) / 16);  // Take ceiling of base_length + 1 (for '\n' ending)
    for (int i = 0; i < pad_length - base_length; ++i) {
        header_stream << ((i == pad_length - base_length - 1) ? "\n" : "\x20");
    }
    std::string    header     = header_stream.str();
    const uint16_t header_len = header.size();

    FILE* f_ptr = fopen(filename.c_str(), "wb");
    FT_CHECK_WITH_INFO(f_ptr != nullptr, fmtstr("Unable to open %s for writing.\n", filename.c_str()));

    fwrite(magic, sizeof(char), sizeof(magic) - 1, f_ptr);
    fwrite(&npy_major, sizeof(uint8_t), 1, f_ptr);
    fwrite(&npy_minor, sizeof(uint8_t), 1, f_ptr);
    fwrite(&header_len, sizeof(uint16_t), 1, f_ptr);
    fwrite(header.c_str(), sizeof(char), header_len, f_ptr);
    fwrite(cpu_data, Tensor::getTypeSize(type()), tensor_size, f_ptr);

    fclose(f_ptr);

    if (is_data_temp) {
        free(cpu_data);
    }
}

Tensor Tensor::slice(std::vector<size_t> shape, size_t offset) const {
    if (this->data() != nullptr) {
        size_t n_elts        = this->size();
        size_t n_sliced_elts = std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());
        FT_CHECK_WITH_INFO(
            n_sliced_elts + offset <= n_elts,
            fmtstr("The number (%ld) of elements of sliced tensor exceeds that (%ld) of the original tensor",
                   n_sliced_elts + offset,
                   n_elts));
    }
    return Tensor(this->where(), this->type(), shape, this->getPtrWithOffset(offset));
}

TensorMap::TensorMap(const std::unordered_map<std::string, Tensor>& tensor_map) {
    for (auto& kv : tensor_map) {
        if (isValid(kv.second)) {
            insert(kv.first, kv.second);
        } else {
            FT_LOG_DEBUG(fmtstr("%s is not a valid tensor, skipping insert into TensorMap", kv.first.c_str()));
        }
    }
}

TensorMap::TensorMap(const std::vector<Tensor>& tensor_map) {
    for (size_t i = 0; i < tensor_map.size(); i++) {
        insert(std::to_string(i), tensor_map[i]);
    }
}

TensorMap::TensorMap(std::initializer_list<std::pair<std::string, Tensor>> tensor_map) {
    for (auto& pair : tensor_map) {
        if (isValid(pair.second)) {
            insert(pair.first, pair.second);
        } else {
            FT_LOG_DEBUG(fmtstr("%s is not a valid tensor, skipping insert into TensorMap", pair.first.c_str()));
        }
    }
}

TensorMap::~TensorMap() {
    tensor_map_.clear();
}

std::vector<std::string> TensorMap::keys() const {
    std::vector<std::string> key_names;
    for (auto& kv : tensor_map_) {
        key_names.push_back(kv.first);
    }
    return key_names;
}

std::string TensorMap::toString() const {
    std::stringstream ss;
    ss << "{";
    std::vector<std::string> key_names = keys();
    for (size_t i = 0; i < tensor_map_.size(); ++i) {
        ss << key_names[i] << ": " << at(key_names[i]).toString();
        if (i < tensor_map_.size() - 1) {
            ss << ", ";
        }
    }
    ss << "}";
    return ss.str();
}

TensorMap TensorMap::fromNpyFolder(const std::string& base_folder) {
    DIR* dir_p = opendir(base_folder.c_str());
    FT_CHECK_WITH_INFO(dir_p != nullptr, fmtstr("Could not open folder %s. ", base_folder.c_str()));
    struct dirent* dp;

    TensorMap ret_tensor;
    while ((dp = readdir(dir_p)) != nullptr) {
        std::string filename(dp->d_name);
        size_t      len = filename.length();
        if (len < 4 || filename.compare(len - 4, 4, ".npy")) {
            continue;
        }

        size_t pos = filename.find('-');
        FT_CHECK_WITH_INFO(pos != std::string::npos, fmtstr("Invalid filename: %s\n", filename.c_str()));

        MemoryType where;
        if (filename.compare(0, pos, "GPU") == 0) {
            where = MEMORY_GPU;
        } else if (filename.compare(0, pos, "CPU") == 0) {
            where = MEMORY_CPU;
        } else if (filename.compare(0, pos, "CPU_PINNED") == 0) {
            where = MEMORY_CPU_PINNED;
        } else {
            FT_CHECK_WITH_INFO(false, fmtstr("Invalid filename: %s\n", filename.c_str()));
        }
        std::string key = filename.substr(pos + 1, len - pos - 5);

        ret_tensor.tensor_map_.insert({key, Tensor::loadNpy(base_folder + "/" + filename, where)});
    }

    closedir(dir_p);

    return ret_tensor;
}

void TensorMap::saveNpy(const std::string& base_folder) {
    mode_t mode_0755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    int    ret       = mkdir(base_folder.c_str(), mode_0755);
    FT_CHECK_WITH_INFO(ret == 0 || errno == EEXIST, fmtstr("Could not create folder %s.\n", base_folder.c_str()));

    for (const auto& item : tensor_map_) {
        item.second.saveNpy(base_folder + "/" + item.second.whereToString() + "-" + item.first + ".npy");
    }
}

}  // namespace fastertransformer