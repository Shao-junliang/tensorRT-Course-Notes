#include "../../../3rd_third/onnx-tensorrt/onnxplugin.hpp"

using namespace ONNXPlugin;

static __device__ float sigmoid(float x) {
    return 1 / (1 + expf(-x));
}

static __global__ void MySigmoid_kernel_fp32(const float *x, float *output, int edge) {
    int position = threadIdx.x + blockDim.x * blockIdx.x;
    if (position >= edge) { return; }
    output[position] = sigmoid(x[position]);
}

// class MySigmoid : public TRTPlugin {
// public:
//     SetupPlugin(MySigmoid);

//     virtual void config_finish() override {
//         printf("\033[33minit MySigmoid config: %s\033[0m\n", config_->info_.c_str());
//         printf("weights count is %d\n", config_->weights_.size());
//     }

//     int enqueue(const std::vector<GTensor> &inputs, std::vector<GTensor> &outputs, const std::vector<GTensor> &weights, void *workspace, cudaStream_t stream) {
//         int n = inputs[0].count();
//         const int nthreads = 512;
//         int block_size = n < nthreads ? n : nthreads;
//         int grid_size = (n + block_size - 1) / block_size;

//         MySigmoid_kernel_fp32<<<grid_size, block_size, 0, stream>>>(inputs[0].ptr<float>(), outputs[0].ptr<float>(), n);
//         return 0;
//     }
// };

// RegisterPlugin(MySigmoid);
