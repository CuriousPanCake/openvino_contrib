// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <functional>
#include <unordered_map>

#include "cuda_operation_base.hpp"

namespace CUDAPlugin {

namespace details {

template <typename TOperation>
inline constexpr bool isConstructibleWithNodeRef = std::is_constructible_v<TOperation,
                                                                           const CreationContext&,
                                                                           const ngraph::Node&,
                                                                           OperationBase::IndexCollection&&,
                                                                           OperationBase::IndexCollection&&>;

template <typename T, typename = void>
inline constexpr auto hasNodeOp = false;

template <typename T>
inline constexpr auto hasNodeOp<T, std::void_t<typename T::NodeOp>> = true;

template <typename TOperation>
struct IsConstructibleWithNodeOpRef {
    static constexpr bool value = std::is_constructible_v<TOperation,
                                                          const CreationContext&,
                                                          const typename TOperation::NodeOp&,
                                                          OperationBase::IndexCollection&&,
                                                          OperationBase::IndexCollection&&>;
};

template <typename TOperation>
inline constexpr bool isConstructibleWithNodeOpRef =
    std::conditional_t<hasNodeOp<TOperation>, IsConstructibleWithNodeOpRef<TOperation>, std::false_type>::value;

}  // namespace details

class OperationRegistry final {
 public:
  using IndexCollection = OperationBase::IndexCollection;
  using OperationBuilder = std::function<OperationBase::Ptr(
          const CreationContext&, const std::shared_ptr<ngraph::Node>&,
          IndexCollection&&, IndexCollection&&)>;
  template <typename TOperation>
  class Register {
  public:
    static_assert(std::is_base_of_v<OperationBase, TOperation>,
                  "TOperation should derive from OperationBase");
    explicit Register(const std::string& opName) {
        getInstance().registerOp(
            opName,
            [](const CreationContext& context,
               const std::shared_ptr<ngraph::Node>& node,
               IndexCollection&& inputs,
               IndexCollection&& outputs) {
                if constexpr (details::isConstructibleWithNodeOpRef<TOperation>) {
                    return std::make_shared<TOperation>(
                        context, downcast<const typename TOperation::NodeOp>(node), move(inputs), move(outputs));
                } else {
                    if constexpr (details::isConstructibleWithNodeRef<TOperation>) {
                        return std::make_shared<TOperation>(context, *node, move(inputs), move(outputs));
                    } else {
                        return std::make_shared<TOperation>(context, node, move(inputs), move(outputs));
                    }
                }
            });
    }
  };

  static OperationRegistry& getInstance();

  bool hasOperation(const std::shared_ptr<ngraph::Node>& node);

  OperationBase::Ptr createOperation(const CreationContext& context,
                                     const std::shared_ptr<ngraph::Node>& node,
                                     IndexCollection&& inIds,
                                     IndexCollection&& outIds);

  OperationBase::Ptr createOperation(const CreationContext& context,
                                     const std::shared_ptr<ngraph::Node>& node,
                                     gsl::span<const TensorID> inIds,
                                     gsl::span<const TensorID> outIds);

 private:
  void registerOp(const std::string& opName, OperationBuilder&& builder);

  bool hasOperation(const std::string& name);

  std::unordered_map<std::string, OperationBuilder> registered_operations_;
};

template <>
class OperationRegistry::Register<OperationBase> {
public:
    explicit Register(const std::string& opName, OperationBuilder&& builder) {
        getInstance().registerOp(opName, move(builder));
    }
};

}  // namespace CUDAPlugin

/**
 * @macro OPERATION_REGISTER
 * @brief Operator registration macro
 *
 * @param type - a class derived from OperationBase and having one of the following constructors
 *        1. type(const std::shared_ptr<ngraph::Node>&, IndexCollection&&, IndexCollection&&);
 *        2. type(const ngraph::Node&, IndexCollection&&, IndexCollection&&);
 *        3. type(const NodeOp&, IndexCollection&&, IndexCollection&&);
 *           where NodeOp is a type's inner alias for a concrete OpenVINO Node class
 * @param name - a textual operator's name
 */
#define OPERATION_REGISTER(type, name)                                                                              \
    extern "C" {                                                                                                    \
    [[maybe_unused]] const ::CUDAPlugin::OperationRegistry::Register<type> openvino_cuda_op_register_##name{#name}; \
    }

#define OPERATION_REGISTER_FACTORY(name, factory)                                                                     \
    extern "C" {                                                                                                      \
    [[maybe_unused]] const ::CUDAPlugin::OperationRegistry::Register<OperationBase> openvino_cuda_op_register_##name{ \
        #name, factory};                                                                                              \
    }
