// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/api.h"
#include "src/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ObjectTest = TestWithContext;

void accessor_name_getter_callback(Local<Name>,
                                   const PropertyCallbackInfo<Value>&) {}

TEST_F(ObjectTest, SetAccessorWhenUnconfigurablePropAlreadyDefined) {
  TryCatch try_catch(isolate());

  Local<Object> global = context()->Global();
  Local<String> property_name =
      String::NewFromUtf8(isolate(), "foo", NewStringType::kNormal)
          .ToLocalChecked();

  PropertyDescriptor prop_desc;
  prop_desc.set_configurable(false);
  global->DefineProperty(context(), property_name, prop_desc).ToChecked();

  Maybe<bool> result = global->SetAccessor(context(), property_name,
                                           accessor_name_getter_callback);
  ASSERT_TRUE(result.IsJust());
  ASSERT_FALSE(result.FromJust());
  ASSERT_FALSE(try_catch.HasCaught());
}

using LapContextTest = TestWithIsolate;

TEST_F(LapContextTest, CurrentContextMustBeFunctionContext) {
  // The receiver object is created in |receiver_context|, but its prototype
  // object is created in |prototype_context|, and the property is accessed
  // from |caller_context|.
  Local<Context> receiver_context = Context::New(isolate());
  Local<Context> prototype_context = Context::New(isolate());
  Local<Context> caller_context = Context::New(isolate());

  Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate());
  Local<Signature> signature = Signature::New(isolate(), function_template);
  Local<String> property_key =
      String::NewFromUtf8(isolate(), "property", NewStringType::kNormal)
          .ToLocalChecked();
  Local<FunctionTemplate> get_or_set = FunctionTemplate::New(
      isolate(),
      [](const FunctionCallbackInfo<Value>& info) {
        Local<Context> prototype_context = *reinterpret_cast<Local<Context>*>(
            info.Data().As<External>()->Value());
        EXPECT_EQ(prototype_context, info.GetIsolate()->GetCurrentContext());
      },
      External::New(isolate(), &prototype_context), signature);
  function_template->PrototypeTemplate()->SetAccessorProperty(
      property_key, get_or_set, get_or_set);

  // |object| is created in |receiver_context|, and |prototype| is created
  // in |prototype_context|.  And then, object.__proto__ = prototype.
  Local<Function> interface_for_receiver =
      function_template->GetFunction(receiver_context).ToLocalChecked();
  Local<Function> interface_for_prototype =
      function_template->GetFunction(prototype_context).ToLocalChecked();
  Local<String> prototype_key =
      String::NewFromUtf8(isolate(), "prototype", NewStringType::kNormal)
          .ToLocalChecked();
  Local<Object> prototype =
      interface_for_prototype->Get(caller_context, prototype_key)
          .ToLocalChecked()
          .As<Object>();
  Local<Object> object =
      interface_for_receiver->NewInstance(receiver_context).ToLocalChecked();
  object->SetPrototype(caller_context, prototype).ToChecked();
  EXPECT_EQ(receiver_context, object->CreationContext());
  EXPECT_EQ(prototype_context, prototype->CreationContext());

  object->Get(caller_context, property_key).ToLocalChecked();
  object->Set(caller_context, property_key, Null(isolate())).ToChecked();

  // Test with a compiled version.
  Local<String> object_key =
      String::NewFromUtf8(isolate(), "object", NewStringType::kNormal)
          .ToLocalChecked();
  caller_context->Global()->Set(caller_context, object_key, object).ToChecked();
  const char script[] =
      "function f() { object.property; object.property = 0; } "
      "f(); f(); "
      "%OptimizeFunctionOnNextCall(f); "
      "f();";
  Context::Scope scope(caller_context);
  internal::FLAG_allow_natives_syntax = true;
  Script::Compile(
      caller_context,
      String::NewFromUtf8(isolate(), script, v8::NewStringType::kNormal)
          .ToLocalChecked())
      .ToLocalChecked()
      ->Run(caller_context)
      .ToLocalChecked();
}

}  // namespace
}  // namespace v8
