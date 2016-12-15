/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "NormProjectionLayer.h"
#include "paddle/utils/Logging.h"
#include "paddle/utils/Stat.h"

namespace paddle {
size_t CMRProjectionNormLayer::getSize() {
  CHECK_EQ(inputLayers_.size(), 1UL);
  size_t layerSize = 0;
  imgSizeH_ = inputLayers_[0]->getOutput().getFrameHeight();
  imgSizeW_ = inputLayers_[0]->getOutput().getFrameWidth();
  if (imgSizeH_ == 0) {
    imgSizeH_ = imgSizeY_;
  }
  if (imgSizeW_ == 0) {
    imgSizeW_ = imgSize_;
  }
  outputH_ = imgSizeH_;
  outputW_ = imgSizeW_;
  layerSize = outputH_ * outputW_ * channels_;

  getOutput().setFrameHeight(outputH_);
  getOutput().setFrameWidth(outputW_);
  return layerSize;
}

bool CMRProjectionNormLayer::init(const LayerMap& layerMap,
                                  const ParameterMap& parameterMap) {
  /* Initialize the basic parent class */
  ResponseNormLayer::init(layerMap, parameterMap);

  /* the size of inputs for norm-layer is 1 */
  CHECK_EQ(config_.inputs_size(), 1);

  if (useGpu_) {
    forward_ = FunctionBase::funcRegistrar_.createByType(
        FUNC_NAME(CrossMapNormal, GPU));
    backward_ = FunctionBase::funcRegistrar_.createByType(
        FUNC_NAME(CrossMapNormalGrad, GPU));
  } else {
    forward_ = FunctionBase::funcRegistrar_.createByType(
        FUNC_NAME(CrossMapNormal, CPU));
    backward_ = FunctionBase::funcRegistrar_.createByType(
        FUNC_NAME(CrossMapNormalGrad, CPU));
  }
  forward_->init(
      FuncConfig().set("size", size_).set("scale", scale_).set("pow", pow_));

  backward_->init(
      FuncConfig().set("size", size_).set("scale", scale_).set("pow", pow_));

  return true;
}

void CMRProjectionNormLayer::forward(PassType passType) {
  Layer::forward(passType);

  /* malloc memory for the output_ if necessary */
  /* note: one sample correspond to one row */
  MatrixPtr input = inputLayers_[0]->getOutputValue();
  size_t batchSize = input->getHeight();
  int size = getSize();
  resetOutput(batchSize, size);

  MatrixPtr outV = getOutputValue();

  Matrix::resizeOrCreate(denoms_, batchSize, size, /* trans */ false, useGpu_);

  dims_ = {batchSize, channels_, imgSizeH_, imgSizeW_};
  forward_->calc(
      {Tensor(input->getData(), dims_)},
      {Tensor(outV->getData(), dims_), Tensor(denoms_->getData(), dims_)},
      {});
}

void CMRProjectionNormLayer::backward(const UpdateCallback& callback) {
  (void)callback;

  if (NULL == inputLayers_[0]->getOutputGrad()) {
    return;
  }
  /* Do derivation */
  MatrixPtr preOutGrad = inputLayers_[0]->getOutputGrad();
  MatrixPtr localGrad = getOutputGrad();
  MatrixPtr localOutV = getOutputValue();
  MatrixPtr preOutV = inputLayers_[0]->getOutputValue();

  backward_->calc({Tensor(preOutV->getData(), dims_),
                   Tensor(localOutV->getData(), dims_),
                   Tensor(localGrad->getData(), dims_),
                   Tensor(denoms_->getData(), dims_)},
                  {Tensor(preOutGrad->getData(), dims_)},
                  {});
}
}  // namespace paddle
