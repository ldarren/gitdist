#include <node.h>
#include <git2.h>
#include "./object_v8.h"
#include "./repository_v8.h"
#include <string.h>

using namespace v8;

struct InitCfg{
    int no_options;
    const char *dir;
    int bare;
    uint32_t shared;
    git_repository_init_options opts;

    InitCfg(const char*, int, const char*, const char*, const char*);
};

InitCfg::InitCfg(const char *d, int b, const char *s, const char *g, const char *t)
    :no_options(1), dir(d), bare(b), shared(0)
{
    if (!strcmp(s, "false") || !strcmp(s, "umask"))
        shared = GIT_REPOSITORY_INIT_SHARED_UMASK;

    else if (!strcmp(s, "true") || !strcmp(s, "group"))
        shared =  GIT_REPOSITORY_INIT_SHARED_GROUP;

    else if (!strcmp(s, "all") || !strcmp(s, "world") || !strcmp(s, "everybody"))
        shared = GIT_REPOSITORY_INIT_SHARED_ALL;

    else if (s[0] == '0') {
        char *end = NULL;
        long val = strtol(s + 1, &end, 8);
        if (end == s + 1 || *end != 0) return; //invalid octal value for --shared
        shared = (uint32_t)val;
    }

    git_repository_init_init_options(&opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);

    int hasG = strcmp(g, "");
    int hasT = strcmp(t, "");
    if (shared || hasG || hasT){
        no_options = 0;
        /**
         * Some command line options were specified, so we'll use the
         * extended init API to handle them
         */
        opts.flags = GIT_REPOSITORY_INIT_MKPATH;

        if (bare) opts.flags |= GIT_REPOSITORY_INIT_BARE;

        if (hasT) {
            opts.flags |= GIT_REPOSITORY_INIT_EXTERNAL_TEMPLATE;
            opts.template_path = t;
        }

        if (hasG) {
            /**
             * If you specified a separate git directory, then initialize
             * the repository at that path and use the second path as the
             * working directory of the repository (with a git-link file)
             */
            opts.workdir_path = dir;
            dir = g;
        }

        if (shared) opts.mode = shared;
    }
}

Handle<Object> Error(int id){
    Isolate *iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    ObjectV8 ret;
    ret.set("id", id);
    const git_error *err = giterr_last();
    if (err){
        ret.set("message", err->message);
        ret.set("klass", err->klass);
    }else{
        ret.set("message", "");
        ret.set("klass", 0);
    }

    Local<Object> o= ret.toJSObject();
    return scope.Escape(o);
}

void Init(const FunctionCallbackInfo<Value> &args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    git_threads_init();

    Local<String> aDir;
    Local<Value> aOpts;
    Local<Function> cb;

    switch(args.Length()){
    case 2:
        aDir = args[0]->ToString();
        aOpts = Undefined(iso);
        cb = Local<Function>::Cast(args[1]);
        break;
    case 3:
        aDir = args[0]->ToString();
        aOpts = args[1];
        cb = Local<Function>::Cast(args[2]);
        break;
    default:
        iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong number of arguments")));
        return;
    }
    String::Utf8Value dir(aDir);
    ObjectV8 o(Local<Object>::Cast(aOpts));

    String::Utf8Value shared(o.get("shared", ""));
    String::Utf8Value gitdir(o.get("gitdir", ""));
    String::Utf8Value templ(o.get("template", ""));

    InitCfg cfg(
        *dir,
        o.get("bare", 0), 
        *shared,
        *gitdir,
        *templ
    );
    git_repository *repo = NULL;
    int error = 0;

    if (cfg.no_options){
        error = git_repository_init(&repo, cfg.dir, cfg.bare);
    }else{
        error = git_repository_init_ext(&repo, cfg.dir, &(cfg.opts));
    }

    if (error){
        const unsigned argc = 1;
        Local<Value> argv[argc] = { Error(error) };
        cb->Call(iso->GetCurrentContext()->Global(), argc, argv);
        return;
    }

    const unsigned argc = 2;
    Local<Value> argv[argc] = { Null(iso), repo_create(repo) };
    cb->Call(iso->GetCurrentContext()->Global(), argc, argv);

    git_threads_shutdown();
}

void Open(const FunctionCallbackInfo<Value>& args){
}

void Clone(const FunctionCallbackInfo<Value>& args){
}

void Free(const FunctionCallbackInfo<Value>& args){
}

void setup(Handle<Object> exports){
    NODE_SET_METHOD(exports, "init", Init);
    NODE_SET_METHOD(exports, "clone", Clone);
    NODE_SET_METHOD(exports, "open", Open);
    NODE_SET_METHOD(exports, "free", Free);
}

NODE_MODULE(gitdist, setup)
