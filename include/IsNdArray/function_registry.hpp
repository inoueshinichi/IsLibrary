#pragma once

#include <IsNdArray/context.hpp>
#include <IsNdArray/exception.hpp>
#include <IsNdArray/function.hpp>
#include <IsNdArray/preprocessor_magic.hpp>

#include <functional>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace is
{
    namespace nbla
    {
        using std::string;
		using std::shared_ptr;
		using std::ostringstream;

        /**
         * @brief Print function backends.
         * 
         * @tparam Item 
         */
        template <typename Item>
        string print_function_items(vector<shared_ptr<Item>> items)
        {
            ostringstream ss;
            ss << "[";
            for (auto&& item : items)
            {
                ss << item->backend << ", ";
            }
            ss << "]";
            return ss.str();
        }


        /**
         * @brief Container of Item(FunctionDbItem) that can be queried by regex.
         * 
         */
        template <typename Item>
        class NBLA_API FunctionDb
        {
            vector<shared_ptr<Item>> items_;

        public:
            typename Item::fuctory_func_t query(const vector<string>& backend)
            {
                auto iter = items_.end();
                for (auto& be : backend)
                {
                    iter = std::find_if(items_.begin(), items_.end(), 
                                        [be](const shared_ptr<Item>& item) {
                                            return item->backend == be;
                                        });
                    if (iter != items_.end())
                    {
                        break;
                    }
                }

                NBLA_CHECK(iter != items_.end(), error_code::value,
						   "Any of [%s] could not be found in %s",
							string_join(backend, ", ").c_str(),
							print_function_items<Item>(items_).c_str());

                return (*iter)->function;
            }


            /**
             * @brief Adding function item (FunctionDbItem).
             * 
             * @param item 
             */
            void add(shared_ptr<Item> item)
            {
                items_.push_back(item);
            }
        };



        /**
         * @brief Item of FunctionDb that stores backend key and
         *        a creator function (with variadic template args).
         * @tparam Base 
         * @tparam Args 
         */
        template <typename Base, typename... Args>
        struct FunctionDbItem
        {
            // new shared_ptr<Function>(const Context& ctx, Args...)???????????????????????????????????????
            using fuctory_func_t = std::function<shared_ptr<Base>(const Context& ctx, Args...)>;
            
            // ?????????????????????????????????
            string backend; // e.g. {"cpu:float"}
            fuctory_func_t function; // new shared_ptr<Base>(const Context& ctx, Args...)?????????
        };


        /**
         * @brief This is used as a base class of function registry for each function that has
         *        numerically unique operation. See NBLA_REGISTER_FUNCTION.
         * 
         * @tparam Base 
         * @tparam Args 
         */
        template <typename Base, typename... Args>
        class NBLA_API FunctionRegistry
        {
        public:
            using item_t = FunctionDbItem<Base, Args...>;

            /**
             * @brief Create a new function instance.
             * 
             * 
             * @param ctx 
             * @param args 
             * @return shared_ptr<Base> 
             */
            shared_ptr<Base> create(const Context& ctx, Args... args)
            {
                return function_db_.query(ctx.backend)(ctx, args...); // ????????? new shared_ptr<Base>();
            }

            void add(shared_ptr<item_t> item)
            {
                function_db_.add(item);
            }

        protected:
            FunctionDb<item_t> function_db_;
        };


#if __cplusplus <= 201703L // <= C++17

    // MSVC???CGG??????????????????????????????????????????
    /*!
    *  This ifdef statement aim to support build on MSVC compiler.
    *  The problem was `,##__VA_ARGS__` only works in GCC, but not works in MSVC compiler.
    *  On the other hand, the workaround by NBLA_VA_ARGS for MSVC breaks GCC preprocessor.
    *  I decided to branch very big portion of source code, but I understand this aproach
    *  is very inefficient and not maintainable. If anybody has an idea,
    *  please let me know or PR is welcom.
    */

    // 20/11/28 shinichi inoue
    // ???????????????????????????
    // #define LOG(fmt, ...) printf(fmt, __VA_ARGS__)
    // ??????????????????????????????1?????????????????????????????????????????????.
    // LOG("FOOBAR\n") // ????????????????????????
    // ????????????GCC????????????##__VA_ARGS__???????????????0?????????????????????????????????.
    // #define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__) // OK
    // MSVC?????????, ...????????????????????????????????????printf(fmt, __VA_ARGS__)???`,`???????????????????????????????????????
    // https://tyfkda.github.io/blog/2015/03/04/var-args.html
    // https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick#11172679
    // https://docs.microsoft.com/ja-jp/cpp/preprocessor/variadic-macros?view=msvc-160

    #ifdef _MSC_VER // `, NBLA_EXPAND`?????????????????????????????? 

        // _MSVC_TRADITIONAL ??????:1, ??????:0
        #if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL // ?????????????????????????????????: __VA__ARGS???????????????, __VA_ARGS__???????????????`,`?????????????????????

            #define NBLA_REGISTER_FUNCTION_HEADER(NAME, ...)                                                \
                FunctionRegistry<Function, NBLA_EXPAND(__VA_ARGS__)>& get_##NAME##Registry();               \
                                                                                                            \
                shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context&, NBLA_EXPAND(__VA_ARGS__))); \


            #define NBLA_REGISTER_FUNCTION_SOURCE(NAME, ...)                                                \
                FunctionRegistry<Function, NBLA_EXPAND(__VA_ARGS__)>& get_##NAME##Registry()                \
                {                                                                                           \
                    static FunctionRegistry<Function, NBLA_EXPAND(__VA_ARGS__)> registry;                   \
                    return registry;                                                                        \
                }                                                                                           \
                                                                                                            \
                shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context&, NBLA_EXPAND(__VA_ARGS__)))  \
                {                                                                                           \
                    init_cpu();                                                                             \
                    return get_##NAME##Registry().create(                                                   \
                        NBLA_ARGS(const Context&, NBLA_EXPAND(__VA_ARGS__)));                               \
                }

            /*!
            *  This will be used inside init method.
            */
            #define NBLA_REGISTER_FUNCTION_IMPL(BASE, CLS, BACKEND, ...)                                    \
                {                                                                                           \
                    std::function<shared_ptr<Function>(const Context&, NBLA_EXPAND(__VA_ARGS__))> func =    \
                        [](NBLA_ARGDEFS(const Context&, NBLA_EXPAND(__VA_ARGS__))) {                        \
                            return shared_ptr<Function>(                                                    \
                                new CLS(NBLA_ARGS(const Context&, NBLA_EXPAND(__VA_ARGS__))));              \
                        };                                                                                  \
                                                                                                            \
                    using item_t = FunctionDbItem<Function, NBLA_EXPAND(__VA_ARGS__)>;                      \
                    get_##BASE##Registry().add(shared_ptr<item_t>(new item_t(BACKEND, func)));              \
                }

        #else // ??????????????????????????????: __VA__ARGS???????????????, __VA_ARGS__???????????????`,`???????????????????????????. ##__VA_ARGS__ ?????????.

            #define NBLA_REGISTER_FUNCTION_HEADER(NAME, ...)                                                \
                FunctionRegistry<Function, ##__VA_ARGS__>& get_##NAME##Registry();                          \
                                                                                                            \
                shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context&, ##__VA_ARGS__));            \


            #define NBLA_REGISTER_FUNCTION_SOURCE(NAME, ...)                                                \
                FunctionRegistry<Function, ##__VA_ARGS__>& get_##NAME##Registry()                           \
                {                                                                                           \
                    static FunctionRegistry<Function, ##__VA_ARGS__> registry;                              \
                    return registry;                                                                        \
                }                                                                                           \
                                                                                                            \
                shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context&, ##__VA_ARGS__))             \
                {                                                                                           \
                    init_cpu();                                                                             \
                    return get_##NAME##Registry().create(                                                   \
                        NBLA_ARGS(const Context&, ##__VA_ARGS__));                                          \
                }

            /*!
            *  This will be used inside init method.
            */
            #define NBLA_REGISTER_FUNCTION_IMPL(BASE, CLS, BACKEND, ...)                                    \
                {                                                                                           \
                    std::function<shared_ptr<Function>(const Context&, ##__VA_ARGS__)> func =               \
                        [](NBLA_ARGDEFS(const Context&, ##__VA_ARGS__)) {                                   \
                            return shared_ptr<Function>(                                                    \
                                new CLS(NBLA_ARGS(const Context&, ##__VA_ARGS__)));                         \
                        };                                                                                  \
                                                                                                            \
                    using item_t = FunctionDbItem<Function, ##__VA_ARGS__>;                                 \
                    get_##BASE##Registry().add(shared_ptr<item_t>(new item_t(BACKEND, func)));              \
                }

        #endif

    #else // GCC `, ##__VA_ARGS__'(GNU??????)?????????

        #define NBLA_REGISTER_FUNCTION_HEADER(NAME, ...)                                                \
            FunctionRegistry<Function, ##__VA_ARGS__>& get_##NAME##Registry();                          \
                                                                                                        \
            shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context&, ##__VA_ARGS__));            


        #define NBLA_REGISTER_FUNCTION_SOURCE(NAME, ...)                                                \
            FunctionRegistry<Function, ##__VA_ARGS__> & get_##NAME##Registry()                          \
            {                                                                                           \
                static FunctionRegistry<Function, ##__VA_ARGS__> registry;                              \
                return registry;                                                                        \
            }                                                                                           \
                                                                                                        \
            shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context&, ##__VA_ARGS__))             \
            {                                                                                           \
                init_cpu();                                                                             \
                return get_##NAME##Registry().create(NBLA_ARGS(const Context&, ##__VA_ARGS__));         \
            }

        
        /*!
        *  This will be used inside init method.
        */
        #define NBLA_REGISTER_FUNCTION_IMPL(BASE, CLS, BACKEND, ...)                                    \
            {                                                                                           \
                std::function<shared_ptr<Function>(const Context&, ##__VA_ARGS__)> func =               \
                    [](NBLA_ARGDEFS(const Context&, ##__VA_ARGS__)) {                                   \
                        return shared_ptr<Function>(                                                    \
                            new CLS(NBLA_ARGS(const Context&, ##__VA_ARGS__)));                         \
                    };                                                                                  \
                                                                                                        \
                using item_t = FunctionDbItem<Function, ##__VA_ARGS__>;                                 \
                get_##BASE##Registry().add(shared_ptr<item_t>(new item_t(BACKEND, func)));              \
            }

    #endif   

#else // (>= C++20)
    #ifdef _MSC_VER

        // Visual Studio 2019 version 16.5.1 & ??????????????????????????????(C++????????????????????????) /Zc:preprocessor
        // ???????????????????????????????????????`,`?????????????????????????????????. GCC?????????, ##__VA_ARGS__????????????.
        // ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????.
        // Visual Studio 2019 version 16.5.1???C++20??????__VA_OPT__(,)????????????.
        // ??????????????????????????????(C++????????????????????????) /Zc:preprocessor

        #if _MSC_VER >= 1925 // >= Visual Studio 2019 16.5.1

            #define NBLA_REGISTER_FUNCTION_HEADER(NAME, ...)                                                \
                FunctionRegistry<Function __VA_OPT__(,) __VA_ARGS__>& get_##NAME##Registry();               \
                                                                                                            \
                shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context& __VA_OPT__(,) __VA_ARGS__)); 

            #define NBLA_REGISTER_FUNCTION_SOURCE(NAME, ...)                                                \
                FunctionRegistry<Function __VA_OPT__(,) __VA_ARGS__>& get_##NAME##Registry()                \
                {                                                                                           \
                    static FunctionRegistry<Function __VA_OPT__(,) __VA_ARGS__> registry;                   \
                    return registry;                                                                        \
                }                                                                                           \
                                                                                                            \
                shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context& __VA_OPT__(,) __VA_ARGS__))  \
                {                                                                                           \
                    init_cpu();                                                                             \
                    return get_##NAME##Registry().create(                                                   \
                        NBLA_ARGS(const Context& __VA_OPT__(,) __VA_ARGS__));                               \
                }


            /*!
            *  This will be used inside init method.
            */
            #define NBLA_REGISTER_FUNCTION_IMPL(BASE, CLS, BACKEND, ...)                                    \
                {                                                                                           \
                    std::function<shared_ptr<Function>(const Context& __VA_OPT__(,) __VA_ARGS__)> func =    \
                        [](NBLA_ARGDEFS(const Context& __VA_OPT__(,) __VA_ARGS__)) {                        \
                            return shared_ptr<Function>(                                                    \
                                new CLS(NBLA_ARGS(const Context& __VA_OPT__(,) __VA_ARGS__)));              \
                        };                                                                                  \
                                                                                                            \
                    using item_t = FunctionDbItem<Function __VA_OPT__(,) __VA_ARGS__>;                      \
                    get_##BASE##Registry().add(shared_ptr<item_t>(new item_t{BACKEND, func}));              \
                }

        #else // < Visual Studio 2019 16.5.1

            // _MSVC_TRADITIONAL ??????:1, ??????:0
            #if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL // ??????????????????????????????

                #define NBLA_REGISTER_FUNCTION_HEADER(NAME, ...)                                                \
                    FunctionRegistry<Function NBLA_VA_ARGS(__VA_ARGS__)>& get_##NAME##Registry();               \
                                                                                                                \
                    shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context& NBLA_EXPAND(__VA_ARGS__))); 

                #define NBLA_REGISTER_FUNCTION_SOURCE(NAME, ...)                                                \
                    FunctionRegistry<Function NBLA_EXPAND(__VA_ARGS__)>& get_##NAME##Registry()                 \
                    {                                                                                           \
                        static FunctionRegistry<Function NBLA_EXPAND(__VA_ARGS__)> registry;                    \
                        return registry;                                                                        \
                    }                                                                                           \
                                                                                                                \
                    shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(const Context& NBLA_EXPAND(__VA_ARGS__)))   \
                    {                                                                                           \
                        init_cpu();                                                                             \
                        return get_##NAME##Registry().create(                                                   \
                            NBLA_ARGS(const Context& NBLA_EXPAND(__VA_ARGS__)));                                \
                    }

                /*!
                *  This will be used inside init method.
                */
                #define NBLA_REGISTER_FUNCTION_IMPL(BASE, CLS, BACKEND, ...)                                    \
                    {                                                                                           \
                        std::function<shared_ptr<Function>(const Context& NBLA_EXPAND(__VA_ARGS__))> func =     \
                            [](NBLA_ARGDEFS(const Context& NBLA_EXPAND(__VA_ARGS__))) {                         \
                                return shared_ptr<Function>(                                                    \
                                    new CLS(NBLA_ARGS(const Context& NBLA_EXPAND(__VA_ARGS__))));               \
                            };                                                                                  \
                                                                                                                \
                        using item_t = FunctionDbItem<Function NBLA_EXPAND(__VA_ARGS__)>;                       \
                        get_##BASE##Registry().add(shared_ptr<item_t>(new item_t{BACKEND, func}));              \
                    }

            #else // ???????????????????????????

                #define NBLA_REGISTER_FUNCTION_HEADER(NAME, ...)                                                \
                    FunctionRegistry<Function , ##__VA_ARGS__>& get_##NAME##Registry();                         \
                    shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(                                            \
                                                const Context& , ##__VA_ARGS__))

                #define NBLA_REGISTER_FUNCTION_SOURCE(NAME, ...)                                                \
                    FunctionRegistry<Function , ##__VA_ARGS__>& get_##NAME##Registry()                          \
                    {                                                                                           \
                        static FunctionRegistry<Function , ##__VA_ARGS__> registry;                             \
                        return registry;                                                                        \
                    }                                                                                           \
                                                                                                                \
                    shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(                                            \
                                                    const Context& , ##__VA_ARGS__))                            \
                    {                                                                                           \
                        init_cpu();                                                                             \
                        return get_##NAME##Registry().create(                                                   \
                            NBLA_ARGS(const Context& , ##__VA_ARGS__));                                         \
                    }

            #endif

        #endif // #if _MSC_VER >= 1925

    #else // GCC (>= C++20)
        #define NBLA_REGISTER_FUNCTION_HEADER(NAME, ...)                                                \
            FunctionRegistry<Function __VA_OPT__(,) __VA_ARGS__>& get_##NAME##Registry();               \
                                                                                                        \
            shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(                                            \
                                                const Context& __VA_OPT__(,) __VA_ARGS__));

        #define NBLA_REGISTER_FUNCTION_SOURCE(NAME, ...)                                                \
            FunctionRegistry<Function __VA_OPT__(,) __VA_ARGS__>& get_##NAME##Registry()                \
            {                                                                                           \
                static FunctionRegistry<Function __VA_OPT__(,) __VA_ARGS__> registry;                   \
                return registry;                                                                        \
            }                                                                                           \
                                                                                                        \
            shared_ptr<Function> create_##NAME(NBLA_ARGDEFS(                                            \
                                            const Context& __VA_OPT__(,) __VA_ARGS__))                  \
            {                                                                                           \
                init_cpu();                                                                             \
                return get_##NAME##Registry().create(                                                   \
                    NBLA_ARGS(const Context& __VA_OPT__(,) __VA_ARGS__));                               \
            }

        /*!
        *  This will be used inside init method.
        */
        #define NBLA_REGISTER_FUNCTION_IMPL(BASE, CLS, BACKEND, ...)                                    \
            {                                                                                           \
                std::function<shared_ptr<Function>(const Context& __VA_OPT__(,) __VA_ARGS__)> func =    \
                    [](NBLA_ARGDEFS(const Context& __VA_OPT__(,) __VA_ARGS__)) {                        \
                        return shared_ptr<Function>(                                                    \
                            new CLS(NBLA_ARGS(const Context& __VA_OPT__(,) __VA_ARGS__)));              \
                    };                                                                                  \
                                                                                                        \
                using item_t = FunctionDbItem<Function __VA_OPT__(,) __VA_ARGS__>;                      \
                get_##BASE##Registry().add(shared_ptr<item_t>(new item_t{BACKEND, func}));              \
            }
    #endif // #ifdef _MSC_VER
#endif // #if __cplusplus < 201703L

    }  // namespace nbla
} // namespace is