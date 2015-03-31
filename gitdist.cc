#include <string.h>
#include <node.h>
#include <git2.h>
#include "./object_v8.h"
#include "./common.h"
#include "./repository.h"

using namespace v8;

struct InitCfg{
    int no_options;
    const char *dir;
    int bare;
    uint32_t shared;
    git_repository_init_options opts;

    InitCfg(const char *d, int b, const char *s, const char *g, const char *t, const char *de)
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
        int hasD = strcmp(de, "");
        if (shared || hasG || hasT || hasD){
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

            if (hasD) opts.description = de;

            if (shared) opts.mode = shared;
        }
    }
};

void Init(const FunctionCallbackInfo<Value> &args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

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
    String::Utf8Value desc(o.get("description", ""));

    InitCfg cfg(
        *dir,
        o.get("bare", 0), 
        *shared,
        *gitdir,
        *templ,
        *desc
    );

    git_libgit2_init();
    git_repository *repo = NULL;
    int error = 0;

    if (cfg.no_options){
        error = git_repository_init(&repo, cfg.dir, cfg.bare);
    }else{
        error = git_repository_init_ext(&repo, cfg.dir, &(cfg.opts));
    }

    next(iso, cb, error, repo_create, repo);

    git_libgit2_shutdown();
}

struct OpenCfg{
    int no_options;
    const char *dir;
    int bare;
    unsigned int flags;
    const char *ceiling;

    OpenCfg(const char *d, int b, int s, const char *c)
        :no_options(1), dir(d), bare(b), flags(0), ceiling(c)
    {
        if (!b || !s || !c) return;
        no_options = 0;

        if (!s) flags = GIT_REPOSITORY_OPEN_NO_SEARCH;
        if (c) flags = GIT_REPOSITORY_OPEN_CROSS_FS;
    }
};

void Open(const FunctionCallbackInfo<Value>& args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

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

    String::Utf8Value ceil(o.get("ceiling", ""));

    OpenCfg cfg(
        *dir,
        o.get("bare", 0), 
        o.get("search", 0), 
        *ceil
    );

    git_libgit2_init();
    git_repository *repo = NULL;
    int error = 0;

    if (cfg.no_options){
        error = git_repository_open(&repo, cfg.dir);
    }else if (cfg.bare){
        error = git_repository_open_bare(&repo, cfg.dir);
    }else{
        error = git_repository_open_ext(&repo, cfg.dir, cfg.flags, cfg.ceiling);
    }

    next(iso, cb, error, repo_create, repo);

    git_libgit2_shutdown();
}

struct progress_data {
    size_t completed_steps;
    size_t total_steps;
    const char *path;
    git_transfer_progress fetch_progress;
    progress_data():completed_steps(0),total_steps(0),path(NULL){
    }
};

static void print_progress(const progress_data *pd) {
    int network_percent = pd->fetch_progress.total_objects > 0 ?  (100*pd->fetch_progress.received_objects) / pd->fetch_progress.total_objects : 0;
    int index_percent = pd->fetch_progress.total_objects > 0 ?  (100*pd->fetch_progress.indexed_objects) / pd->fetch_progress.total_objects : 0;

    int checkout_percent = pd->total_steps > 0 ? (100 * pd->completed_steps) / pd->total_steps : 0;
    int kbytes = pd->fetch_progress.received_bytes / 1024;

    if (pd->fetch_progress.total_objects && pd->fetch_progress.received_objects == pd->fetch_progress.total_objects) {
        printf("Resolving deltas %d/%d\r", pd->fetch_progress.indexed_deltas, pd->fetch_progress.total_deltas);
    } else {
        printf("net %3d%% (%4d kb, %5d/%5d)  /  idx %3d%% (%5d/%5d)  /  chk %3d%% (%4zu/%4zu) %s\n",
           network_percent, kbytes,
           pd->fetch_progress.received_objects, pd->fetch_progress.total_objects,
           index_percent, pd->fetch_progress.indexed_objects, pd->fetch_progress.total_objects,
           checkout_percent,
           pd->completed_steps, pd->total_steps,
           pd->path);
    }
}

static int fetch_progress(const git_transfer_progress *stats, void *payload) {
    progress_data *pd = (progress_data*)payload;
    pd->fetch_progress = *stats;
    print_progress(pd);
    return 0;
}
static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload) {
    progress_data *pd = (progress_data*)payload;
    pd->completed_steps = cur;
    pd->total_steps = tot;
    pd->path = path;
    print_progress(pd);
}

struct CloneCfg{
    const char *url;
    const char *dir;
    progress_data pd;
    git_checkout_options checkout_opts;
    git_clone_options clone_opts;

    CloneCfg(const char *u, const char *d, int f)
        :url(u), dir(d)
    {
        git_clone_init_options(&clone_opts, GIT_CLONE_OPTIONS_VERSION);
        git_checkout_init_options(&checkout_opts, GIT_CHECKOUT_OPTIONS_VERSION);

        checkout_opts.checkout_strategy = f ? GIT_CHECKOUT_FORCE : GIT_CHECKOUT_SAFE_CREATE;
        checkout_opts.progress_cb = &checkout_progress;
        checkout_opts.progress_payload = &pd;
        clone_opts.checkout_opts = checkout_opts;
        clone_opts.remote_callbacks.transfer_progress = &fetch_progress;
        clone_opts.remote_callbacks.credentials = cred_acquire_cb;
        clone_opts.remote_callbacks.payload = &pd;
    }
};

void Clone(const FunctionCallbackInfo<Value>& args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> aUrl;
    Local<String> aDir;
    Local<Value> aOpts;
    Local<Function> cb;

    switch(args.Length()){
    case 3:
        aUrl = args[0]->ToString();
        aDir = args[1]->ToString();
        aOpts = Undefined(iso);
        cb = Local<Function>::Cast(args[2]);
        break;
    case 4:
        aUrl = args[0]->ToString();
        aDir = args[1]->ToString();
        aOpts = args[2];
        cb = Local<Function>::Cast(args[3]);
        break;
    default:
        iso->ThrowException(Exception::TypeError(String::NewFromUtf8(iso, "Wrong number of arguments")));
        return;
    }
    String::Utf8Value dir(aDir);
    String::Utf8Value url(aUrl);
    ObjectV8 o(Local<Object>::Cast(aOpts));

    CloneCfg cfg(
        *url,
        *dir,
        o.get("force", 0)
    );

    git_libgit2_init();
    git_repository *repo = NULL;
    int error = git_clone(&repo, cfg.url, cfg.dir, &(cfg.clone_opts));

    next(iso, cb, error, repo_create, repo);

    git_libgit2_shutdown();
}

void Sign(const FunctionCallbackInfo<Value> &args){
}

void setup(Handle<Object> exports){
    NODE_SET_METHOD(exports, "init", Init);
    NODE_SET_METHOD(exports, "open", Open);
    NODE_SET_METHOD(exports, "clone", Clone);
    NODE_SET_METHOD(exports, "sign", Sign);
}

NODE_MODULE(gitdist, setup)
