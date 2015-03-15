#include <node.h>
#include <git2.h>
#include "./object_v8.h"

using namespace v8;

struct opts {
    int no_options;
    int quiet;
    int bare;
    int initial_commit;
    uint32_t shared;
    const char *templ;
    const char *gitdir;
    const char *dir;
};

void LastErr(const FunctionCallbackInfo<Value>& args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    ObjectV8 ret;
    const git_error *err = giterr_last();
    ret.set("message", err->message);
    ret.set("klass", err->klass);

    args.GetReturnValue().Set(ret.toJSObject());
}

void Init(const FunctionCallbackInfo<Value>& args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    ObjectV8 cfg(Handle<Object>::Cast(args[0]));

    git_repository *repo = NULL;
    String::Utf8Value utf(cfg.get("dir", ""));
    struct opts o = { 1, cfg.get("quiet", 0), cfg.get("bare", 0), 0, GIT_REPOSITORY_INIT_SHARED_UMASK, 0, 0, *utf };
    git_threads_init();

    int error = git_repository_init(&repo, o.dir, 0);
    if (error){
        args.GetReturnValue().Set(Number::New(iso, error));
        return;
    }

    if (!o.quiet) {
        if (o.bare || o.gitdir)
            o.dir = git_repository_path(repo);
        else
            o.dir = git_repository_workdir(repo);
    }

    git_repository_free(repo);
    git_threads_shutdown();
    args.GetReturnValue().Set(Number::New(iso, 0));
}

void setup(Handle<Object> exports){
    NODE_SET_METHOD(exports, "init", Init);
    NODE_SET_METHOD(exports, "lastError", LastErr);
}

NODE_MODULE(gitdist, setup)
