// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"

#include <functional>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(CallbackHelpersTest, IsBaseCallback) {
  // Check that base::{Once,Repeating}Closures and references to them are
  // considered base::{Once,Repeating}Callbacks.
  static_assert(base::IsBaseCallback<base::OnceClosure>::value, "");
  static_assert(base::IsBaseCallback<base::RepeatingClosure>::value, "");
  static_assert(base::IsBaseCallback<base::OnceClosure&&>::value, "");
  static_assert(base::IsBaseCallback<const base::RepeatingClosure&>::value, "");

  // Check that base::{Once, Repeating}Callbacks with a given RunType and
  // references to them are considered base::{Once, Repeating}Callbacks.
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>>::value, "");
  static_assert(base::IsBaseCallback<base::RepeatingCallback<int(int)>>::value,
                "");
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>&&>::value,
                "");
  static_assert(
      base::IsBaseCallback<const base::RepeatingCallback<int(int)>&>::value,
      "");

  // Check that POD types are not considered base::{Once, Repeating}Callbacks.
  static_assert(!base::IsBaseCallback<bool>::value, "");
  static_assert(!base::IsBaseCallback<int>::value, "");
  static_assert(!base::IsBaseCallback<double>::value, "");

  // Check that the closely related std::function is not considered a
  // base::{Once, Repeating}Callback.
  static_assert(!base::IsBaseCallback<std::function<void()>>::value, "");
  static_assert(!base::IsBaseCallback<const std::function<void()>&>::value, "");
  static_assert(!base::IsBaseCallback<std::function<void()>&&>::value, "");
}

TEST(CallbackHelpersTest, IsOnceCallback) {
  // Check that base::OnceClosures and references to them are considered
  // base::OnceCallbacks, but base::RepeatingClosures are not.
  static_assert(base::IsOnceCallback<base::OnceClosure>::value, "");
  static_assert(!base::IsOnceCallback<base::RepeatingClosure>::value, "");
  static_assert(base::IsOnceCallback<base::OnceClosure&&>::value, "");
  static_assert(!base::IsOnceCallback<const base::RepeatingClosure&>::value,
                "");

  // Check that base::OnceCallbacks with a given RunType and references to them
  // are considered base::OnceCallbacks, but base::RepeatingCallbacks are not.
  static_assert(base::IsOnceCallback<base::OnceCallback<int(int)>>::value, "");
  static_assert(!base::IsOnceCallback<base::RepeatingCallback<int(int)>>::value,
                "");
  static_assert(base::IsOnceCallback<base::OnceCallback<int(int)>&&>::value,
                "");
  static_assert(
      !base::IsOnceCallback<const base::RepeatingCallback<int(int)>&>::value,
      "");

  // Check that POD types are not considered base::OnceCallbacks.
  static_assert(!base::IsOnceCallback<bool>::value, "");
  static_assert(!base::IsOnceCallback<int>::value, "");
  static_assert(!base::IsOnceCallback<double>::value, "");

  // Check that the closely related std::function is not considered a
  // base::OnceCallback.
  static_assert(!base::IsOnceCallback<std::function<void()>>::value, "");
  static_assert(!base::IsOnceCallback<const std::function<void()>&>::value, "");
  static_assert(!base::IsOnceCallback<std::function<void()>&&>::value, "");

  // Check that the result of BindOnce is a OnceCallback.
  auto cb = base::BindOnce([](int* count) { ++*count; });
  static_assert(base::IsOnceCallback<decltype(cb)>::value, "");
}

void Increment(int* value) {
  (*value)++;
}

void IncrementWithRef(int& value) {
  value++;
}

TEST(CallbackHelpersTest, ScopedClosureRunnerHasClosure) {
  base::ScopedClosureRunner runner1;
  EXPECT_FALSE(runner1);

  base::ScopedClosureRunner runner2{base::DoNothing()};
  EXPECT_TRUE(runner2);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerExitScope) {
  int run_count = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerRelease) {
  int run_count = 0;
  base::OnceClosure c;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count));
    c = runner.Release();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(0, run_count);
  std::move(c).Run();
  EXPECT_EQ(1, run_count);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerReplaceClosure) {
  int run_count_1 = 0;
  int run_count_2 = 0;
  {
    base::ScopedClosureRunner runner;
    runner.ReplaceClosure(base::BindOnce(&Increment, &run_count_1));
    runner.ReplaceClosure(base::BindOnce(&Increment, &run_count_2));
    EXPECT_EQ(0, run_count_1);
    EXPECT_EQ(0, run_count_2);
  }
  EXPECT_EQ(0, run_count_1);
  EXPECT_EQ(1, run_count_2);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerRunAndResetNonNull) {
  int run_count_3 = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count_3));
    EXPECT_EQ(0, run_count_3);
    runner.RunAndReset();
    EXPECT_EQ(1, run_count_3);
  }
  EXPECT_EQ(1, run_count_3);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerRunAndResetNull) {
  base::ScopedClosureRunner runner;
  runner.RunAndReset();  // Should not crash.
}

TEST(CallbackHelpersTest, ScopedClosureRunnerMoveConstructor) {
  int run_count = 0;
  {
    std::unique_ptr<base::ScopedClosureRunner> runner(
        new base::ScopedClosureRunner(base::BindOnce(&Increment, &run_count)));
    base::ScopedClosureRunner runner2(std::move(*runner));
    runner.reset();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerMoveAssignment) {
  int run_count_1 = 0;
  int run_count_2 = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count_1));
    {
      base::ScopedClosureRunner runner2(
          base::BindOnce(&Increment, &run_count_2));
      runner = std::move(runner2);
      EXPECT_EQ(1, run_count_1);
      EXPECT_EQ(0, run_count_2);
    }
    EXPECT_EQ(1, run_count_1);
    EXPECT_EQ(0, run_count_2);
  }
  EXPECT_EQ(1, run_count_1);
  EXPECT_EQ(1, run_count_2);
}

TEST(CallbackHelpersTest, SplitOnceCallback_EmptyCallback) {
  base::OnceCallback<void(int*)> cb = base::NullCallback();
  EXPECT_FALSE(cb);

  auto split = base::SplitOnceCallback(std::move(cb));

  static_assert(std::is_same_v<decltype(split),
                               std::pair<base::OnceCallback<void(int*)>,
                                         base::OnceCallback<void(int*)>>>,
                "");
  EXPECT_FALSE(split.first);
  EXPECT_FALSE(split.second);
}

TEST(CallbackHelpersTest, SplitOnceCallback_FirstCallback) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));

  static_assert(std::is_same_v<decltype(split),
                               std::pair<base::OnceCallback<void(int*)>,
                                         base::OnceCallback<void(int*)>>>,
                "");

  EXPECT_EQ(0, count);
  std::move(split.first).Run(&count);
  EXPECT_EQ(1, count);

#if GTEST_HAS_DEATH_TEST
  EXPECT_CHECK_DEATH(std::move(split.second).Run(&count));
#endif  // GTEST_HAS_DEATH_TEST
}

TEST(CallbackHelpersTest, SplitOnceCallback_SecondCallback) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));

  static_assert(std::is_same_v<decltype(split),
                               std::pair<base::OnceCallback<void(int*)>,
                                         base::OnceCallback<void(int*)>>>,
                "");

  EXPECT_EQ(0, count);
  std::move(split.second).Run(&count);
  EXPECT_EQ(1, count);

  EXPECT_CHECK_DEATH(std::move(split.first).Run(&count));
}

TEST(CallbackHelpersTest, SplitSplitOnceCallback_FirstSplit) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));
  base::OnceCallback<void(int*)> cb1 = std::move(split.first);
  split = base::SplitOnceCallback(std::move(split.second));
  base::OnceCallback<void(int*)> cb2 = std::move(split.first);
  base::OnceCallback<void(int*)> cb3 = std::move(split.second);

  EXPECT_EQ(0, count);
  std::move(cb1).Run(&count);
  EXPECT_EQ(1, count);

  EXPECT_CHECK_DEATH(std::move(cb3).Run(&count));
}

TEST(CallbackHelpersTest, SplitSplitOnceCallback_SecondSplit) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));
  base::OnceCallback<void(int*)> cb1 = std::move(split.first);
  split = base::SplitOnceCallback(std::move(split.second));
  base::OnceCallback<void(int*)> cb2 = std::move(split.first);
  base::OnceCallback<void(int*)> cb3 = std::move(split.second);

  EXPECT_EQ(0, count);
  std::move(cb2).Run(&count);
  EXPECT_EQ(1, count);

  EXPECT_CHECK_DEATH(std::move(cb1).Run(&count));
}

TEST(CallbackHelpersTest, IgnoreArgs) {
  int count = 0;
  base::RepeatingClosure repeating_closure =
      base::BindRepeating(&Increment, &count);
  base::OnceClosure once_closure = base::BindOnce(&Increment, &count);

  base::RepeatingCallback<void(int)> repeating_int_cb =
      base::IgnoreArgs<int>(repeating_closure);
  EXPECT_EQ(0, count);
  repeating_int_cb.Run(42);
  EXPECT_EQ(1, count);
  repeating_int_cb.Run(42);
  EXPECT_EQ(2, count);

  base::OnceCallback<void(int)> once_int_cb =
      base::IgnoreArgs<int>(std::move(once_closure));
  EXPECT_EQ(2, count);
  std::move(once_int_cb).Run(42);
  EXPECT_EQ(3, count);

  // Ignore only some (one) argument and forward the rest.
  auto repeating_callback = base::BindRepeating(&Increment);
  auto repeating_cb_with_extra_arg = base::IgnoreArgs<bool>(repeating_callback);
  repeating_cb_with_extra_arg.Run(false, &count);
  EXPECT_EQ(4, count);

  // Ignore two arguments and forward the rest.
  auto once_callback = base::BindOnce(&Increment);
  auto once_cb_with_extra_arg =
      base::IgnoreArgs<char, bool>(repeating_callback);
  std::move(once_cb_with_extra_arg).Run('d', false, &count);
  EXPECT_EQ(5, count);
}

TEST(CallbackHelpersTest, IgnoreArgs_EmptyCallback) {
  base::RepeatingCallback<void(int)> repeating_int_cb =
      base::IgnoreArgs<int>(base::RepeatingClosure());
  EXPECT_FALSE(repeating_int_cb);

  base::OnceCallback<void(int)> once_int_cb =
      base::IgnoreArgs<int>(base::OnceClosure());
  EXPECT_FALSE(once_int_cb);
}

TEST(CallbackHelpersTest, ForwardRepeatingCallbacks) {
  int count = 0;
  auto tie_cb =
      base::ForwardRepeatingCallbacks({base::BindRepeating(&IncrementWithRef),
                                       base::BindRepeating(&IncrementWithRef)});

  tie_cb.Run(count);
  EXPECT_EQ(count, 2);

  tie_cb.Run(count);
  EXPECT_EQ(count, 4);
}

TEST(CallbackHelpersTest, ReturnValueOnce) {
  // Check that copyable types are supported.
  auto string_factory = base::ReturnValueOnce(std::string("test"));
  static_assert(std::is_same_v<decltype(string_factory),
                               base::OnceCallback<std::string(void)>>);
  EXPECT_EQ(std::move(string_factory).Run(), "test");

  // Check that move-only types are supported.
  auto unique_ptr_factory = base::ReturnValueOnce(std::make_unique<int>(42));
  static_assert(std::is_same_v<decltype(unique_ptr_factory),
                               base::OnceCallback<std::unique_ptr<int>(void)>>);
  EXPECT_EQ(*std::move(unique_ptr_factory).Run(), 42);
}

}  // namespace
