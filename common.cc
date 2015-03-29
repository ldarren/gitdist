#include <node.h>
#include <git2.h>
#include <stdarg.h>
#include "./object_v8.h"
#include "./common.h"

using namespace v8;

Handle<Object> self_alloc(void *ptr){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

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

void next(Isolate *iso, Handle<Function> cb, int error, FactoryFunc func=NULL, void *ptr=0){
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
