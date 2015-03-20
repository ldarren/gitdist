#include <node.h>
#include <git2.h>
#include "./object_v8.h"
#include "./repository_v8.h"
#include <string.h>

using namespace v8;

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

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

struct InitCfg{
    int no_options;
    const char *dir;
    int bare;
    uint32_t shared;
    git_repository_init_options opts;

    InitCfg(const char*, int, const char*, const char*, const char*, const char*);
};

InitCfg::InitCfg(const char *d, int b, const char *s, const char *g, const char *t, const char *de)
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

void Init(const FunctionCallbackInfo<Value> &args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    git_libgit2_init();

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

    git_libgit2_shutdown();
}

struct OpenCfg{
    int no_options;
    const char *dir;
    int bare;
    unsigned int flags;
    const char *ceiling;

    OpenCfg(const char*, int, int, const char*);
};

OpenCfg::OpenCfg(const char *d, int b, int s, const char *c)
    :no_options(1), dir(d), bare(b), flags(0), ceiling(c)
{
    if (!b || !s || !c) return;
    no_options = 0;

    if (!s) flags = GIT_REPOSITORY_OPEN_NO_SEARCH;
    if (c) flags = GIT_REPOSITORY_OPEN_CROSS_FS;
}

void Open(const FunctionCallbackInfo<Value>& args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    git_libgit2_init();

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
    git_repository *repo = NULL;
    int error = 0;

    if (cfg.no_options){
        error = git_repository_open(&repo, cfg.dir);
    }else if (cfg.bare){
        error = git_repository_open_bare(&repo, cfg.dir);
    }else{
        error = git_repository_open_ext(&repo, cfg.dir, cfg.flags, cfg.ceiling);
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

    git_libgit2_shutdown();
}

int cred_acquire_cb(git_cred **out,
        const char * UNUSED(url),
        const char * UNUSED(username_from_url),
        unsigned int allowed_types,
        void * UNUSED(payload))
{
    char username[128] = {0};
    char password[128] = {0};
printf("allowed_types: %d\n",allowed_types);
printf("GIT_CREDTYPE_USERPASS_PLAINTEXT: %d\n",allowed_types & GIT_CREDTYPE_USERPASS_PLAINTEXT);
printf("GIT_CREDTYPE_SSH_KEY: %d\n",allowed_types & GIT_CREDTYPE_SSH_KEY);
printf("GIT_CREDTYPE_SSH_CUSTOM: %d\n",allowed_types & GIT_CREDTYPE_SSH_CUSTOM);
printf("GIT_CREDTYPE_DEFAULT: %d\n",allowed_types & GIT_CREDTYPE_DEFAULT);
printf("GIT_CREDTYPE_SSH_INTERACTIVE: %d\n",allowed_types & GIT_CREDTYPE_SSH_INTERACTIVE);
printf("GIT_CREDTYPE_USERNAME: %d\n",allowed_types & GIT_CREDTYPE_USERNAME);

    printf("Username: ");
    scanf("%s", username);

    /* Yup. Right there on your terminal. Careful where you copy/paste output. */
    printf("Password: ");
    scanf("%s", password);
//git_cred_ssh_custom_new
//git_cred_ssh_key_new
    return git_cred_userpass_plaintext_new(out, username, password);
}

typedef struct progress_data {
    git_transfer_progress fetch_progress;
    size_t completed_steps;
    size_t total_steps;
    const char *path;
} progress_data;

static void print_progress(const progress_data *pd)
{
    int network_percent = pd->fetch_progress.total_objects > 0 ?
        (100*pd->fetch_progress.received_objects) / pd->fetch_progress.total_objects :
        0;
    int index_percent = pd->fetch_progress.total_objects > 0 ?
        (100*pd->fetch_progress.indexed_objects) / pd->fetch_progress.total_objects :
        0;

    int checkout_percent = pd->total_steps > 0
        ? (100 * pd->completed_steps) / pd->total_steps
        : 0;
    int kbytes = pd->fetch_progress.received_bytes / 1024;

    if (pd->fetch_progress.total_objects &&
        pd->fetch_progress.received_objects == pd->fetch_progress.total_objects) {
        printf("Resolving deltas %d/%d\r",
               pd->fetch_progress.indexed_deltas,
               pd->fetch_progress.total_deltas);
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

static int fetch_progress(const git_transfer_progress *stats, void *payload)
{
    progress_data *pd = (progress_data*)payload;
    pd->fetch_progress = *stats;
    print_progress(pd);
    return 0;
}
static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
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

    CloneCfg(const char*, const char*, int);
};

CloneCfg::CloneCfg(const char *u, const char *d, int f)
    :url(u), dir(d), pd({{0}})
{
    git_clone_init_options(&clone_opts, GIT_CLONE_OPTIONS_VERSION);
    git_checkout_init_options(&checkout_opts, GIT_CHECKOUT_OPTIONS_VERSION);

    checkout_opts.checkout_strategy = f ? GIT_CHECKOUT_FORCE : GIT_CHECKOUT_SAFE;
    checkout_opts.progress_cb = checkout_progress;
    checkout_opts.progress_payload = &pd;
    clone_opts.checkout_opts = checkout_opts;
    clone_opts.remote_callbacks.transfer_progress = &fetch_progress;
    clone_opts.remote_callbacks.credentials = cred_acquire_cb;
    clone_opts.remote_callbacks.payload = &pd;
}

void Clone(const FunctionCallbackInfo<Value>& args){
    Isolate *iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    git_libgit2_init();

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
    git_repository *repo = NULL;
    int error = git_clone(&repo, cfg.url, cfg.dir, &(cfg.clone_opts));

    if (error){
        const unsigned argc = 1;
        Local<Value> argv[argc] = { Error(error) };
        cb->Call(iso->GetCurrentContext()->Global(), argc, argv);
        return;
    }

    const unsigned argc = 2;
    Local<Value> argv[argc] = { Null(iso), repo_create(repo) };
    cb->Call(iso->GetCurrentContext()->Global(), argc, argv);

    git_libgit2_shutdown();
}

void setup(Handle<Object> exports){
    NODE_SET_METHOD(exports, "init", Init);
    NODE_SET_METHOD(exports, "open", Open);
    NODE_SET_METHOD(exports, "clone", Clone);
}

NODE_MODULE(gitdist, setup)
