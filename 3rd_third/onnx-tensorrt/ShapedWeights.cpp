/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ShapedWeights.hpp"
#include "onnx2trt_utils.hpp"
#include "trt_utils.hpp"
#include <cstdint>
#include <cstring>

namespace onnx2trt {

size_t ShapedWeights::count() const {
    if (this->values == nullptr && this->shape.nbDims <= 0) {
        return 0;
    }
    // TRT supports scalars, so 0D tensors should have a count of 1.
    size_t c = 1;
    for (int i = 0; i < this->shape.nbDims; ++i) {
        c *= this->shape.d[i];
    }
    return c;
}

ShapedWeights ShapedWeights::empty(DataType type) {
    return ShapedWeights(type, nullptr, nvinfer1::Dims{0});
}

ShapedWeights::ShapedWeights() :
    values(nullptr), shape{0} {
}

ShapedWeights::ShapedWeights(DataType type_, void *values_, nvinfer1::Dims shape_) :
    type(type_), values(values_), shape(shape_) {
    // Note: this->shape.type[] is not used
}

size_t ShapedWeights::size_bytes() const {
    return this->count() * getDtypeSize(this->type);
}

ShapedWeights::operator bool() const {
    return (bool)this->values;
}

ShapedWeights::operator nvinfer1::Weights() const {
    nvinfer1::Weights w{};
    w.values = this->values;
    bool supported_type = convertDtype(this->type, &w.type);
    (void)supported_type;
    assert(supported_type);
    w.count = this->count();
    return w;
}

const char *ShapedWeights::getName() const {
    return this->name;
}

void ShapedWeights::setName(const char *name) {
    this->name = name;
}

template <typename DType>
void transpose4DWeights(ShapedWeights const &weights, nvinfer1::Permutation const perm, ShapedWeights *result) {
    nvinfer1::Dims original_shape = weights.shape;
    nvinfer1::Dims new_shape = result->shape;
    int nbDims = new_shape.nbDims;
    DType const *src = reinterpret_cast<DType *>(weights.values);
    DType *dst = reinterpret_cast<DType *>(result->values);

    nvinfer1::Dims expanded_original_shape{4, {1, 1, 1, 1}};
    nvinfer1::Dims expanded_new_shape{4, {1, 1, 1, 1}};
    nvinfer1::Permutation expanded_perm{0, 1, 2, 3};

    int pad = 4 - nbDims;
    for (int i = 0; i < nbDims; ++i) {
        expanded_original_shape.d[pad + i] = original_shape.d[i];
        expanded_new_shape.d[pad + i] = new_shape.d[i];
        expanded_perm.order[pad + i] = perm.order[i] + pad;
    }

    int src_strides[4] = {1, 1, 1, 1};
    int dst_strides[4] = {1, 1, 1, 1};

    for (int i = 2; i >= 0; --i) {
        src_strides[i] = expanded_original_shape.d[i + 1] * src_strides[i + 1];
        dst_strides[i] = expanded_new_shape.d[i + 1] * dst_strides[i + 1];
    }

    for (int n = 0; n < expanded_original_shape.d[0]; ++n) {
        for (int c = 0; c < expanded_original_shape.d[1]; ++c) {
            for (int h = 0; h < expanded_original_shape.d[2]; ++h) {
                for (int w = 0; w < expanded_original_shape.d[3]; ++w) {
                    int src_index = 0;
                    int dst_index = 0;
                    int src_coord[4] = {n, c, h, w};
                    int dst_coord[4];
                    for (int i = 0; i < 4; ++i) {
                        dst_coord[i] = src_coord[expanded_perm.order[i]];
                        src_index += src_coord[i] * src_strides[i];
                        dst_index += dst_coord[i] * dst_strides[i];
                    }
                    dst[dst_index] = src[src_index];
                }
            }
        }
    }
}

bool transposeWeights(ShapedWeights const &weights, nvinfer1::Permutation const &perm, ShapedWeights *result, IImporterContext *ctx) {
    nvinfer1::Dims shape = weights.shape;
    int nbDims = shape.nbDims;
    nvinfer1::Dims new_shape;
    new_shape.nbDims = nbDims;
    for (int d = 0; d < nbDims; ++d) {
        new_shape.d[d] = shape.d[perm.order[d]];
        result->shape.d[d] = new_shape.d[d];
    }

    if (shape.nbDims <= 4) {
        if (weights.type == ::onnx::TensorProto::FLOAT) {
            transpose4DWeights<float>(weights, perm, result);
        } else if (weights.type == ::onnx::TensorProto::FLOAT16) {
            transpose4DWeights<uint16_t>(weights, perm, result);
        } else {
            return false;
        }
    } else {
        // TODO: Implement general transposes and multiple data types
        // Unsupported weights transpose
        return false;
    }
    nvinfer1::Dims permDims{nbDims, {}};
    std::copy_n(perm.order, nbDims, permDims.d);
    LOG_WARNING("Weights "
                << weights.getName() << " has been transposed with permutation of " << permDims
                << "! If you plan on overwriting the weights with the Refitter API, the new weights must be pre-transposed.");
    result->setName(weights.getName());
    return true;
}

} // namespace onnx2trt
