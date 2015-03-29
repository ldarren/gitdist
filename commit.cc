#include <node.h>
#include <git2.h>
#include "./common.h"
#include "./blob.h"
#include "./tree.h"
#include "./commit.h"

using namespace v8;

Handle<Value> commit_create(void *ptr){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<Object> self = self_alloc(ptr);
    self->Set(String::NewFromUtf8(iso, "free"), wrap_func(commit_remove));

    return scope.Escape(self);
}

void commit_remove(const FunctionCallbackInfo<Value>& args){
    Isolate* iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    git_commit *ptr = (git_commit*)self_get(args);    
    git_libgit2_init();
    git_commit_free(ptr);
    self_free(args);
    git_libgit2_shutdown();

    args.GetReturnValue().Set(Number::New(iso, 0));
}
