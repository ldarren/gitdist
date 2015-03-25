#include <node.h>
#include <git2.h>
#include "./object_v8.h"
#include "./common.h"
#include "./repository.h"
#include "./commit.h"

using namespace v8;

Handle<Object> repo_create(git_repository *repo){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<Object> obj = Object::New(iso);
    obj->SetHiddenValue(String::NewFromUtf8(iso, "_repo"), External::New(iso, repo));
    obj->Set(String::NewFromUtf8(iso, "pull"), wrap_func(repo_pull));
    obj->Set(String::NewFromUtf8(iso, "commit"), wrap_func(repo_commit_get));
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
    git_repository_free((git_repository*)(ptr->Value()));
    r->DeleteHiddenValue(key);
    git_libgit2_shutdown();

    args.GetReturnValue().Set(Number::New(iso, 0));
}

void repo_commit_get(const v8::FunctionCallbackInfo<v8::Value> &args){
    Isolate* iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> key = String::NewFromUtf8(iso, "_repo");
    Local<Object> r = args.Holder();
    Local<External> ptr = Local<External>::Cast(r->GetHiddenValue(key));
    if (!ptr->IsExternal()){
        iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong context")));
    }

    if (2 != args.Length()) iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong number of arguments")));
    if (!args[0]->IsString() || !args[1]->IsFunction()) iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Missing oid or callback")));

    String::Utf8Value oidHex(args[0]->ToString());
    Local<Function> cb = Local<Function>::Cast(args[1]);

    git_libgit2_init();

    git_commit *commit;
    git_oid oid;
    git_oid_fromstrp(&oid, *oidHex);

    int error = git_commit_lookup(&commit, (git_repository*)(ptr->Value()), &oid);

    if (error){
        const unsigned argc = 1;
        Local<Value> argv[argc] = { Error(error) };
        cb->Call(iso->GetCurrentContext()->Global(), argc, argv);
        return;
    }

    const unsigned argc = 2;
    Local<Value> argv[argc] = { Null(iso), commit_create(commit) };
    cb->Call(iso->GetCurrentContext()->Global(), argc, argv);

    git_libgit2_shutdown();
}

void repo_pull(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("pull\n");
}

void repo_push(const v8::FunctionCallbackInfo<v8::Value> &args){
    printf("push\n");
}
