#include <node.h>
#include <git2.h>
#include "./repository_v8.h"

using namespace v8;

Handle<Function> wrap_func(FunctionCallback func, const char *name=NULL){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<FunctionTemplate> tpl = FunctionTemplate::New(iso, func);
    Local<Function> fn = tpl->GetFunction();

    // omit this to make it anonymous
    if (name) fn->SetName(String::NewFromUtf8(iso, name));

    return scope.Escape(fn);
}

Handle<Object> repo_create(git_repository *repo){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<Object> obj = Object::New(iso);
    obj->SetHiddenValue(String::NewFromUtf8(iso, "_repo"), External::New(iso, repo));
    obj->Set(String::NewFromUtf8(iso, "pull"), wrap_func(repo_pull));
    obj->Set(String::NewFromUtf8(iso, "commit"), wrap_func(repo_commit));
    obj->Set(String::NewFromUtf8(iso, "push"), wrap_func(repo_push));
    obj->Set(String::NewFromUtf8(iso, "free"), wrap_func(repo_remove));

    return scope.Escape(obj);
}

void repo_remove(const FunctionCallbackInfo<Value>& args){
    Isolate* iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> key = String::NewFromUtf8(iso, "_repo");
    Local<Object> r = args.Holder();
    Local<External> ptr = Local<External>::Cast(r->GetHiddenValue(key));
    if (!ptr->IsExternal()){
        args.GetReturnValue().Set(Number::New(iso, 1));
        return;
    }
    git_libgit2_init();
    git_repository_free(static_cast<git_repository*>(ptr->Value()));
    r->DeleteHiddenValue(key);
    git_libgit2_shutdown();

    args.GetReturnValue().Set(Number::New(iso, 0));
}

void repo_pull(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("pull\n");
}

void repo_commit(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("commit\n");
}

void repo_push(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("push\n");
}
