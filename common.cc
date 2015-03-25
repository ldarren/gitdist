#include <node.h>
#include <git2.h>
#include "./object_v8.h"
#include "./common.h"

using namespace v8;

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

Handle<Function> wrap_func(FunctionCallback func, const char *name){
    Isolate* iso = Isolate::GetCurrent();
    EscapableHandleScope scope(iso);

    Local<FunctionTemplate> tpl = FunctionTemplate::New(iso, func);
    Local<Function> fn = tpl->GetFunction();

    // omit this to make it anonymous
    if (name) fn->SetName(String::NewFromUtf8(iso, name));

    return scope.Escape(fn);
}

int check_args(const FunctionCallbackInfo<Value> &args, Handle<Value> *typesList){
    int ret = 1;
    int argsLen = args.Length();
    int typesLen = sizeof(typesList);
    Handle<Value> *types;

    for(int i=0; i<typesLen; i++){
        types = typesList[i];
        if (argsLen == sizeof(types)){
            int correct = 1;
            Local<Value> t;
            for(int j=0; j<argsLen; j++){
                t = types[j];
                if (t->IsObject()){
                    if (!(args[j]->IsObject())){
                        correct=0;
                        break;
                    }
                }else if (t->IsArray()){
                    if (!(args[j]->IsArray())){
                        correct=0;
                        break;
                    }
                }else if (t->IsFunction()){
                    if (!(args[j]->IsFunction())){
                        correct=0;
                        break;
                    }
                }else if (t->IsNumber()){
                    if (!(args[j]->IsNumber())){
                        correct=0;
                        break;
                    }
                }else if (t->IsString()){
                    if (!(args[j]->IsString())){
                        correct=0;
                        break;
                    }
                }
                t = args[j];
            }
            if (correct) ret = 0;
            break;
        }
    }

    return ret;
}
