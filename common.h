#ifndef _GITDIST_COMMON_H_
#define _GITDIST_COMMON_H_

typedef v8::Handle<v8::Value> (*FactoryFunc)(void*);

v8::Handle<v8::Object> self_alloc(void*);
void* self_get(const v8::FunctionCallbackInfo<v8::Value>& args);
void self_free(const v8::FunctionCallbackInfo<v8::Value>& args);

v8::Handle<v8::Object> Error(int id);
v8::Handle<v8::Function> wrap_func(v8::FunctionCallback, const char *name=NULL);
void next(v8::Isolate*, v8::Handle<v8::Function>, int, FactoryFunc func=NULL, void *ptr=NULL);

#endif // _GITDIST_COMMON_H_
