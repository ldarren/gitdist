#include <node.h>
#include <git2.h>
#include <stdarg.h>
#include "./object_v8.h"
#include "./common.h"

using namespace v8;

Handle<Object> self_alloc(void *ptr){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    // TODO: make KEY_SELF a persistence, https://github.com/libgit2/node-gitteh/blob/master/src/repository.cc
    Local<String> KEY_SELF = String::NewFromUtf8(iso, "self");

    Local<Object> self = Object::New(iso);
    self->SetHiddenValue(Local<String>::New(iso, KEY_SELF), External::New(iso, ptr));

    return scope.Escape(self);
}

void* self_get(const FunctionCallbackInfo<Value>& args){
    Isolate* iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> KEY_SELF = String::NewFromUtf8(iso, "self");

    Local<Object> r = args.Holder();
    Local<External> ptr = Local<External>::Cast(r->GetHiddenValue(Local<String>::New(iso, KEY_SELF)));
    if (!ptr->IsExternal()){
        return 0;
    }
    return ptr->Value();
}

void self_free(const FunctionCallbackInfo<Value>& args){
    Isolate* iso = Isolate::GetCurrent();
    HandleScope scope(iso);

    Local<String> KEY_SELF = String::NewFromUtf8(iso, "self");

    args.Holder()->DeleteHiddenValue(Local<String>::New(iso, KEY_SELF));
}

Handle<Function> wrap_func(FunctionCallback func, const char *name){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<FunctionTemplate> tpl = FunctionTemplate::New(iso, func);
    Local<Function> fn = tpl->GetFunction();

    // omit this to make it anonymous
    if (name) fn->SetName(String::NewFromUtf8(iso, name));

    return scope.Escape(fn);
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

void next(Isolate *iso, Handle<Function> cb, int error, FactoryFunc func, void *ptr){
    if (error){
        const unsigned argc = 1;
        Local<Value> argv[argc] = { Error(error) };
        cb->Call(iso->GetCurrentContext()->Global(), argc, argv);
    }else if (func){
        const unsigned argc = 2;
        Local<Value> argv[argc] = { Null(iso), func(ptr) };
        cb->Call(iso->GetCurrentContext()->Global(), argc, argv);
    }else{
        const unsigned argc = 0;
        Local<Value> argv[argc] = {};
        cb->Call(iso->GetCurrentContext()->Global(), argc, argv);
    }
}

int cred_acquire_cb(git_cred **out, const char *url, const char *username_from_url, unsigned int allowed_types, void * /*payload*/) {
    if (allowed_types & GIT_CREDTYPE_SSH_KEY){
        return git_cred_ssh_key_new(out, username_from_url, "/home/ubuntu/.ssh/id_rsa.pub", "/home/ubuntu/.ssh/id_rsa", "darren<3snow");
    }else if (allowed_types & GIT_CREDTYPE_SSH_CUSTOM){
        //return git_cred_ssh_custom_new();
    }else if (allowed_types & GIT_CREDTYPE_DEFAULT){
        //return git_cred_default_new();
    }else if (allowed_types & GIT_CREDTYPE_SSH_INTERACTIVE){
        //return git_cred_ssh_interactive_new();
    }else if (allowed_types & GIT_CREDTYPE_USERNAME){
        //return git_cred_username_new();
    }

    // GIT_CREDTYPE_USERPASS_PLAINTEXT
    char username[128] = {0};
    char password[128] = {0};
    printf("Username for %s: [%s]",url,username_from_url);
    if (EOF == scanf("%s", username)) return -1;
    /* Yup. Right there on your terminal. Careful where you copy/paste output. */
    printf("Password: ");
    if (EOF == scanf("%s", password)) return -1;
    return git_cred_userpass_plaintext_new(out, username, password);
}
