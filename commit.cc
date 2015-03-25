#include <node.h>
#include <git2.h>
#include "./common.h"
#include "./repository.h"
#include "./commit.h"

using namespace v8;

Handle<Object> commit_create(git_commit *commit){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<Object> obj = Object::New(iso);
    obj->SetHiddenValue(String::NewFromUtf8(iso, "_commit"), External::New(iso, commit));
    obj->Set(String::NewFromUtf8(iso, "free"), wrap_func(commit_remove));

    return scope.Escape(obj);
}

void commit_remove(const FunctionCallbackInfo<Value>& args){
    Isolate* iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> key = String::NewFromUtf8(iso, "_commit");
    Local<Object> r = args.Holder();
    Local<External> ptr = Local<External>::Cast(r->GetHiddenValue(key));
    if (!ptr->IsExternal()){
        args.GetReturnValue().Set(Number::New(iso, 1));
        return;
    }
    git_libgit2_init();
    git_commit_free(static_cast<git_commit*>(ptr->Value()));
    r->DeleteHiddenValue(key);
    git_libgit2_shutdown();

    args.GetReturnValue().Set(Number::New(iso, 0));
}
