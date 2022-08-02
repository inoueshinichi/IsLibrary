#pragma once

#include <IsNdArray/cpu.hpp>
#include <IsNdArray/function.hpp>
#include <IsNdArray/function/sum.hpp>
#include <IsNdArray/function_registry.hpp>

namespace is
{
    namespace nbla
    {
        NBLA_REGISTER_FUNCTION_HEADER(Max, const vector<int64_t>&, bool, bool, bool)
        // axes
        // keep_dims
        // with_index
        // only_index


        /**
         * @brief Reduction along axes with max operation.
         * 
         * Inputs:
         * - N-D array
         * 
         * Outputs:
         * - one or two N-D arrays
         * 
         * @param T Data type for computation.
         * @param axes A list of axes to be reduced.
         * @param keep_dims Flag whether the reduced axes are kept.
         * @param with_index Return sorted values and index.
         * @param only_index Return only the sort index.
         * 
         */
        template <typename T>
        class Max : public Sum<T>
        {
        protected:
            bool with_index_;
            bool only_index_;
            NdArrayPtr index_buff_;

        public:
            Max(const Context& ctx, const vector<int64_t>& axes, bool keep_dims, bool with_index, bool only_index)
                : Sum<T>(ctx, axes, keep_dims)
                , with_index_(with_index)
                , only_index_(only_index) {}

            virtual ~Max() {}

            virtual shared_ptr<Function> copy() const 
            {
                return create_Max(this->ctx_, this->axes_, this->keep_dims_,
                                  this->with_index_, this->only_index_);
            }

            virtual string name() { return "Max"; }

        protected:
            NBLA_API virtual void setup_impl(const NdArrays& inputs,
                                             const NdArrays& outputs);

            NBLA_API virtual void execute_impl(const NdArrays& inputs,
                                               const NdArrays& outputs);

            NBLA_API virtual void execute_impl_reduce(const T *x, T *y, 
                                                      int outer_size,
                                                      int reduction_size);
        };
    }
}